module Veneer:Formatter;

import :Message;
import :AnsiFormatting;
import :UTF8Formatting;
import :Snippet;
import :CppSourceParser;
import std.compat;

namespace vnr {
    class Formatter {
    public:
        std::vector<Line> format(const Snippet& snippet);

    private:
        static void annotateSourceCode(ParsedSnippet& snippet, std::vector<Line>& result);
        static std::vector<std::string_view> extractLines(std::string_view source, size_t startByte, uint32_t lineCount);
        static std::string colorLine(std::string_view lineView, uint32_t row, std::span<ParsedTarget*> targetsOnLine);
        static void appendCaretLines(std::vector<const ParsedTarget*> endingTargets, std::vector<Line>& result);
        static void appendInsertions(uint32_t row, InsertPosition position, std::string_view anchorLine, const ParsedSnippet& snippet, std::vector<Line>& result);
        static std::string applyInlineInsertions(std::string_view lineView, uint32_t row, const ParsedSnippet& snippet, std::vector<ParsedTarget*>& targetsOnLine, std::vector<ParsedTarget>& inlineSpans);
        static ParsedSnippet reconstructReflectedStruct(const Snippet& snippet);

        CppSourceParser parser;
    };
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

std::vector<Line> Formatter::format(const Snippet& snippet) {
    std::vector<Line> result;

    // Start by printing the main snippet's location followed by a blank line
    result.push_back(Line{ .type = LineType::Location, .location = snippet.location });
    result.push_back(Line{ .type = LineType::GutterOnly });

    if (auto parsedSnippet = parser.parse(snippet)){
        annotateSourceCode(*parsedSnippet, result);
    } else if (snippet.reflectedStruct) {
        result.push_back(Line{ .type = LineType::GutterOnly, .text = "(source code not found - the struct below was reconstructed via reflection)" });
        auto fallback = reconstructReflectedStruct(snippet);
        annotateSourceCode(fallback, result);
    } else {
        // If we couldn't open the source file, just print all the target texts below the location as a bulleted list
        result.push_back(Line{ .type = LineType::GutterOnly, .text = "(source code not found)" });
        for (const auto& target : snippet.targets) {
            result.push_back(Line{ .type = LineType::GutterOnly, .text =  "- " + target.format(target.text) });
        }
    }

    result.push_back(Line{ .type = LineType::GutterOnly }); // push an extra empty line to add some breathing room
    return result;
}

void Formatter::annotateSourceCode(ParsedSnippet& snippet, std::vector<Line>& result) {
    // To print everything properly without having the targets fight for space, we sort them from top to bottom, then left to right.
    std::ranges::sort(snippet.targets, [](const ParsedTarget& a, const ParsedTarget& b) {
        if (a.start.row != b.start.row) {
            return a.start.row < b.start.row; // top to bottom
        }
        if (a.start.column != b.start.column) {
            return a.start.column < b.start.column; // left to right within the same row
        }
        return a.end.column > b.end.column; // this should be unreachable for any well-formed C++ lines, but added just in case
    });

    const uint32_t lineCount = snippet.endRow - snippet.startRow + 1;
    const auto lines = extractLines(snippet.sourceCode, snippet.startByte, lineCount);

    for (size_t i = 0; i < lines.size(); i++) {
        const uint32_t row = snippet.startRow + static_cast<uint32_t>(i);

        std::vector<ParsedTarget*> targetsOnLine;
        for (auto& target : snippet.targets) {
            if (row >= target.start.row && row <= target.end.row) {
                targetsOnLine.push_back(&target);
            }
        }
        std::vector<ParsedTarget> inlineSpans;
        std::string lineText = applyInlineInsertions(lines[i], row, snippet, targetsOnLine, inlineSpans);
        std::string coloredLine = colorLine(lineText, row, targetsOnLine);

        appendInsertions(row, InsertPosition::Before, lines[i], snippet, result);

        const uint32_t lineNumber = snippet.showLineNumbers ? (row + 1) : 0; // 0 is the sentinel "no line number"
        if (targetsOnLine.size() == 1 && targetsOnLine[0]->useArrow) {
            const auto* target = targetsOnLine[0];
            coloredLine += Ansi::unbold(target->format)(std::format(" {} ", UTF8::arrowLeft())) + target->format(target->text);
            result.push_back(Line{ .type = LineType::GutterOnly, .lineNumber = lineNumber, .text = std::move(coloredLine) });
        } else {
            result.push_back(Line{ .type = LineType::GutterOnly, .lineNumber = lineNumber, .text = std::move(coloredLine) });

            std::vector<const ParsedTarget*> endingTargets;
            for (const auto* target : targetsOnLine) {
                if (target->end.row == row) {
                    endingTargets.push_back(target);
                }
            }
            appendCaretLines(std::move(endingTargets), result);
        }
        appendInsertions(row, InsertPosition::After, lines[i], snippet, result);
    }
}

std::vector<std::string_view> Formatter::extractLines(std::string_view source, size_t startByte, uint32_t lineCount) {
    std::vector<std::string_view> lines;
    lines.reserve(lineCount);

    size_t lineStart = startByte;
    for (uint32_t i = 0; i < lineCount && lineStart < source.size(); i++) {
        size_t lineEnd = source.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = source.size(); // if we can't find the newline character, we're probably on the last line of the source code
        }

        std::string_view line = source.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        lines.push_back(line);

        if (lineEnd == source.size()) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return lines;
}

std::string Formatter::colorLine(std::string_view lineView, uint32_t row, std::span<ParsedTarget*> targetsOnLine) {
    uint32_t lineLength = static_cast<uint32_t>(lineView.length());
    std::string coloredLine;
    uint32_t cursor = 0;

    for (auto* target : targetsOnLine) {
        // If the target starts or ends in the middle of the current line, only highlight the portion that actually belongs to the target. Otherwise, highlight the full line.
        uint32_t startColumn = (row == target->start.row) ? std::min(target->start.column, lineLength) : 0;
        uint32_t endColumn   = (row == target->end.row)   ? std::min(target->end.column  , lineLength) : lineLength;

        auto firstNonWhitespace = lineView.find_first_not_of(" \t\r\n", startColumn);
        if (firstNonWhitespace < endColumn) {
            target->caretStart = std::min(target->caretStart, static_cast<uint32_t>(firstNonWhitespace));
        }
        target->caretEnd = std::max(target->caretEnd, endColumn);

        if (endColumn > cursor) {
            uint32_t sliceStart = std::max(startColumn, cursor);
            coloredLine += lineView.substr(cursor, sliceStart - cursor);
            coloredLine += target->format(lineView.substr(sliceStart, endColumn - sliceStart));
            cursor = endColumn;
        }
    }

    coloredLine += lineView.substr(cursor);
    return coloredLine;
}

void Formatter::appendCaretLines(std::vector<const ParsedTarget*> endingTargets, std::vector<Line>& result) {
    if (endingTargets.empty()) {
        return;
    }

    std::ranges::sort(endingTargets, [](const ParsedTarget* a, const ParsedTarget* b) {
        return a->caretStart < b->caretStart;
    });

    // Print all the carets required on the current line by placing spaces until we hit the caretStart of a target,
    // then placing caretEnd - column '^'s (column here acts as a cursor). This repeats for all targets ending on the current line.
    std::string caretLine;
    uint32_t column = 0;
    for (const auto* target : endingTargets) {
        if (target->caretStart > column) {
            caretLine.append(target->caretStart - column, ' ');
            column = target->caretStart;
        }
        if (target->caretEnd > column) {
            caretLine += target->format(std::string(target->caretEnd - column, '^'));
            column = target->caretEnd;
        }
    }

    const auto* rightmostTarget = endingTargets.back();
    caretLine += std::format(" {}", rightmostTarget->format(rightmostTarget->text));
    result.push_back(Line{ .type = LineType::GutterOnly, .text = std::move(caretLine) });

    // Gather any remaining targets to print their text below
    std::vector<const ParsedTarget*> pendingTargets(endingTargets.begin(), endingTargets.end() - 1);

    // This lambda prints the '|' character for the given number of targets.
    // Since the formatted pipes will contain ANSI escape codes, we also need a separate
    // visualColumn tracker to find the location of the current cursor excluding ANSI codes.
    auto printPipes = [&](size_t count) {
        std::string line;
        uint32_t visualColumn = 0;
        for (size_t i = 0; i < count; i++) {
            if (pendingTargets[i]->caretStart > visualColumn) {
                line.append(pendingTargets[i]->caretStart - visualColumn, ' ');
                visualColumn = pendingTargets[i]->caretStart;
            }
            line += Ansi::unbold(pendingTargets[i]->format)(UTF8::gutter());
            visualColumn++;
        }
        return std::pair{ std::move(line), visualColumn };
    };

    for (size_t i = pendingTargets.size(); i-- > 0;) {
        // We add a blank '|' line before each target's text to give them vertical breathing room
        result.push_back(Line{ .type = LineType::GutterOnly, .text = printPipes(i + 1).first });

        const auto* target = pendingTargets[i];
        auto [line, column] = printPipes(i);

        if (target->caretStart > column) {
            line.append(target->caretStart - column, ' ');
        }
        line += Ansi::unbold(target->format)(UTF8::branch()) + target->format(" " + target->text);
        result.push_back(Line{ .type = LineType::GutterOnly, .text = std::move(line) });
    }
}

void Formatter::appendInsertions(uint32_t row, InsertPosition position, std::string_view anchorLine, const ParsedSnippet& snippet, std::vector<Line>& result) {
    // We want to make the insertion appear as though it's part of the user's code, so we match the whitespace length
    size_t indentLength = anchorLine.find_first_not_of(" \t");
    std::string_view indent = (indentLength == std::string_view::npos) ? "" : anchorLine.substr(0, indentLength);

    for (const auto& insertion : snippet.insertions) {
        if (insertion.isInline || insertion.anchor.row != row || insertion.position != position) {
            continue;
        }
        std::string line = std::format("{}{}", indent, insertion.format(insertion.code));
        if (!insertion.text.empty()) {
            line += Ansi::unbold(insertion.format)(std::format(" {} ", UTF8::arrowLeft()) + insertion.format(insertion.text));
        }
        result.push_back(Line{ .type = LineType::Addition, .text = std::move(line) });
    }
}

std::string Formatter::applyInlineInsertions(std::string_view lineView, uint32_t row, const ParsedSnippet& snippet, std::vector<ParsedTarget*>& targetsOnLine, std::vector<ParsedTarget>& inlineSpans) {
    std::vector<const ParsedInsertion*> insertionsOnCurrentLine;
    for (const auto& insertion : snippet.insertions) {
        if (insertion.isInline && insertion.anchor.row == row) {
            insertionsOnCurrentLine.push_back(&insertion);
        }
    }
    if (insertionsOnCurrentLine.empty()) {
        return std::string(lineView);
    }
    inlineSpans.reserve(insertionsOnCurrentLine.size());

    // We use a stable sort to ensure insertions get inserted in order if they need to be inserted before the same field
    std::ranges::stable_sort(insertionsOnCurrentLine, [](const ParsedInsertion* a, const ParsedInsertion* b) {
        return a->anchor.column < b->anchor.column;
    });

    std::string patchedLine(lineView);
    for (size_t i = insertionsOnCurrentLine.size(); i-- > 0;) {
        const auto* insertion = insertionsOnCurrentLine[i];
        uint32_t column = std::min(insertion->anchor.column, static_cast<uint32_t>(patchedLine.size()));

        // Try to match the user's spacing (for example: int x; int y; vs int x;int y;)
        // TODO: if someone says int x, y; and we want to insert something before y, split it into int x; NewField field; int y; (defaulting to a multiline split unless other fields are also defined on the same line)
        std::string_view prefix = (column > 0 && patchedLine[column - 1] != ' ') ? " " : "";
        std::string_view suffix = (column < patchedLine.size() && patchedLine[column] != ' ') ? " " : "";
        std::string fullText = std::format("{}{}{}", prefix, insertion->code, suffix);
        uint32_t shift = static_cast<uint32_t>(fullText.size());

        patchedLine.insert(column, fullText);

        for (ParsedTarget* target : targetsOnLine) {
            if (target->start.row == row && target->start.column >= column) {
                target->start.column += shift;
            }
            if (target->end.row == row && target->end.column > column) {
                target->end.column += shift;
            }
        }

        uint32_t codeStart = column + static_cast<uint32_t>(prefix.size());
        uint32_t codeEnd   = codeStart + static_cast<uint32_t>(insertion->code.size());
        inlineSpans.push_back(ParsedTarget{
            .format   = insertion->format,
            .text     = insertion->text,
            .start    = { row, codeStart },
            .end      = { row, codeEnd },
            .useArrow = false,
        });
        targetsOnLine.push_back(&inlineSpans.back());
    }

    // Sort the targets on the current line after the extra targets were inserted
    std::ranges::sort(targetsOnLine, [](const ParsedTarget* a, const ParsedTarget* b) {
        if (a->start.column != b->start.column) {
            return a->start.column < b->start.column;
        }
        return a->end.column > b->end.column;
    });

    return patchedLine;
}

ParsedSnippet Formatter::reconstructReflectedStruct(const Snippet& snippet) {
    const auto& reflectedStruct = *snippet.reflectedStruct;

    std::string source = std::format("struct {} {{\n", reflectedStruct.name);
    std::vector<ParsedTarget> targets;
    std::vector<ParsedInsertion> insertions;

    auto sameLocation = [](const std::source_location& a, const std::source_location& b) {
        return a.line() == b.line() && a.column() == b.column();
    };

    for (uint32_t i = 0; i < reflectedStruct.fields.size(); i++) {
        const auto& field = reflectedStruct.fields[i];
        uint32_t row = i + 1; // row 0 is "struct Name {"
        source += std::format("    {} {};\n", field.type, field.name);

        if constexpr (CMakeOptions::ENABLE_LOGGING) {
            uint32_t typeStart = 4;
            uint32_t typeEnd   = typeStart + static_cast<uint32_t>(field.type.size());
            uint32_t nameStart = typeEnd + 1;
            uint32_t nameEnd   = nameStart + static_cast<uint32_t>(field.name.size());
            uint32_t lineEnd   = nameEnd + 1; // include ';'

            for (const auto& target : snippet.targets) {
                if (!sameLocation(target.location, field.location)) {
                    continue;
                }
                uint32_t columnStart = typeStart;
                uint32_t columnEnd   = lineEnd;
                bool useArrow     = true;

                if (target.type == TargetType::VariableType) {
                    columnStart = typeStart;
                    columnEnd   = typeEnd;
                    useArrow = false;
                } else if (target.type == TargetType::VariableName) {
                    columnStart = nameStart;
                    columnEnd   = nameEnd;
                    useArrow = false;
                }

                targets.push_back(ParsedTarget{
                    .format   = target.format,
                    .text     = target.text,
                    .start    = { row, columnStart },
                    .end      = { row, columnEnd },
                    .useArrow = useArrow,
                });
            }

            for (const auto& insertion : snippet.insertions) {
                if (!sameLocation(insertion.anchorLocation, field.location)) {
                    continue;
                }
                insertions.push_back(ParsedInsertion{
                    .anchor   = { row, typeStart },
                    .position = insertion.position,
                    .isInline = false,
                    .code     = insertion.code,
                    .text     = insertion.text,
                    .format   = insertion.format,
                });
            }
        }
    }

    source += "};";

    return ParsedSnippet{
        .sourceCode      = std::move(source),
        .startByte       = 0,
        .startRow        = 0,
        .endRow          = static_cast<uint32_t>(reflectedStruct.fields.size()) + 1,
        .targets         = std::move(targets),
        .insertions      = std::move(insertions),
        .showLineNumbers = false,
    };
}