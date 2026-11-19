module;
#include <tree-sitter/lib/include/tree_sitter/api.h>
module Veneer:CppSourceParser;

import :Snippet;
import std.compat;

extern "C" const TSLanguage* tree_sitter_cpp();

namespace vnr {
    class CppSourceParser {
    public:
        CppSourceParser();
        ~CppSourceParser();

        CppSourceParser(const CppSourceParser&) = delete;
        CppSourceParser& operator=(const CppSourceParser&) = delete;

        std::optional<ParsedSnippet> parse(const Snippet& snippet) const;

    private:
        static std::string loadFile(const std::filesystem::path& path);
        static TSNode nodeAtLocation(TSNode root, const std::source_location& location);
        static TSNode findNode(TSNode startNode, std::initializer_list<std::string_view> targetTypes, bool searchInwards = true);
        static TSNode findTargetNode(TSNode targetBaseNode, TSNode targetLineNode, const Target& target);
        static bool canUseArrow(TSNode fullLineNode, TSNode targetNode);
        static bool canInsertOnOwnLine(TSNode fullLineNode, TSNode anchorNode, InsertPosition position);

        TSParser* parser = nullptr;
    };
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

CppSourceParser::CppSourceParser() {
    parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_cpp());
}

CppSourceParser::~CppSourceParser() {
    ts_parser_delete(parser);
}

std::optional<ParsedSnippet> CppSourceParser::parse(const Snippet& snippet) const {
    std::string sourceCode = loadFile(snippet.location.file_name());
    if (sourceCode.empty()) {
        return std::nullopt;
    }

    TSTree* tree = ts_parser_parse_string(parser, nullptr, sourceCode.data(), sourceCode.size());
    TSNode root  = ts_tree_root_node(tree);

    // This is the node that the compiler's std::source_location throws us at
    TSNode baseNode = nodeAtLocation(root, snippet.location);

    TSNode fullLineNode = findNode(baseNode, { "declaration", "expression_statement", "struct_specifier", "class_specifier" }, false);
    if (ts_node_is_null(fullLineNode)) {
        fullLineNode = baseNode;
    }

    std::vector<ParsedTarget> targets;
    for (const auto& target : snippet.targets) {
        const bool hasCustomLocation = target.location.line() != 0;
        TSNode targetBaseNode = hasCustomLocation ? nodeAtLocation(root, target.location) : baseNode;
        TSNode targetLineNode = fullLineNode;

        if (hasCustomLocation) {
            targetLineNode = findNode(targetBaseNode, { "field_declaration", "declaration", "expression_statement" }, false);
            if (ts_node_is_null(targetLineNode)) {
                targetLineNode = targetBaseNode;
            }
        }

        TSNode targetNode = findTargetNode(targetBaseNode, targetLineNode, target);
        TSPoint targetStart = ts_node_start_point(targetNode);
        TSPoint targetEnd   = ts_node_end_point(targetNode);

        targets.push_back(ParsedTarget{
            .format   = target.format,
            .text     = target.text,
            .start    = { targetStart.row, targetStart.column },
            .end      = { targetEnd.row, targetEnd.column },
            .useArrow = canUseArrow(fullLineNode, targetNode)
        });
    }

    std::vector<ParsedInsertion> insertions;
    for (const auto& insertion : snippet.insertions) {
        TSNode anchorBase = nodeAtLocation(root, insertion.anchorLocation);
        TSNode anchorLine = findNode(anchorBase, { "field_declaration", "declaration", "expression_statement" }, false);
        if (ts_node_is_null(anchorLine)) {
            anchorLine = anchorBase;
        }

        TSPoint anchorPoint = (insertion.position == InsertPosition::Before)
            ? ts_node_start_point(anchorLine)
            : ts_node_end_point(anchorLine);

        insertions.push_back(ParsedInsertion{
            .anchor   = { anchorPoint.row, anchorPoint.column },
            .position = insertion.position,
            .isInline = !canInsertOnOwnLine(fullLineNode, anchorLine, insertion.position),
            .code     = insertion.code,
            .text     = insertion.text,
            .format   = insertion.format,
        });
    }

    TSPoint lineStart = ts_node_start_point(fullLineNode);
    TSPoint lineEnd   = ts_node_end_point(fullLineNode);
    size_t startByte  = ts_node_start_byte(fullLineNode) - lineStart.column;
    ts_tree_delete(tree);

    return ParsedSnippet{
        .sourceCode = std::move(sourceCode),
        .startByte  = startByte,
        .startRow   = lineStart.row,
        .endRow     = lineEnd.row,
        .targets    = std::move(targets),
        .insertions = std::move(insertions)
    };
}

std::string CppSourceParser::loadFile(const std::filesystem::path& path) {
    if (std::ifstream file{ path }) {
        return { std::istreambuf_iterator<char>(file), {} };
    }
    return {};
}

TSNode CppSourceParser::nodeAtLocation(TSNode root, const std::source_location& location) {
    // Tree-sitter uses 0-based indexing as opposed to std::source_location's 1-based indexing, so we must subtract 1
    TSPoint point = { location.line() - 1, location.column() - 1 };
    return ts_node_descendant_for_point_range(root, point, point);
}

TSNode CppSourceParser::findNode(TSNode startNode, std::initializer_list<std::string_view> targetTypes, bool searchInwards) {
    // When we specify the location we want to parse around in the file, we'll typically land
    // somewhere that we have no control over based on how the compiler handles std::source_location.
    //
    // This function should take us to the node of the type we want so we can then process it however we want.
    // It should handle both cases where we're inside and want to branch out, or we're outside and want to dive inwards.

    auto matches = [targetTypes](TSNode node) {
        return std::ranges::contains(targetTypes, std::string_view(ts_node_type(node)));
    };

    // This lambda is for the case where we want to search inwards recursively
    auto findChild = [&matches](this auto&& self, TSNode node) -> TSNode {
        if (matches(node)) {
            return node;
        }
        uint32_t childCount = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < childCount; i++) {
            TSNode foundChild = self(ts_node_named_child(node, i));
            if (!ts_node_is_null(foundChild)) {
                return foundChild;
            }
        }
        return {};
    };

    // When searching upwards, these are the types we stop at
    static constexpr std::array<std::string_view, 3> searchBoundaries = {
        "call_expression", "declaration", "expression_statement"
    };

    TSNode current = startNode;
    while (!ts_node_is_null(current)) {
        if (matches(current)) {
            return current;
        }
        if (searchInwards && std::ranges::contains(searchBoundaries, std::string_view(ts_node_type(current)))) {
            TSNode correctChild = findChild(current);
            if (!ts_node_is_null(correctChild)) {
                return correctChild;
            }
        }
        current = ts_node_parent(current);
    }
    return {};
}

TSNode CppSourceParser::findTargetNode(TSNode targetBaseNode, TSNode targetLineNode, const Target& target) {
    auto getField = [](TSNode node, std::string_view name) {
        return ts_node_child_by_field_name(node, name.data(), static_cast<uint32_t>(name.size()));
    };

    TSNode targetNode = targetBaseNode;
    switch (target.type) {
        case TargetType::WholeLine:
            targetNode = targetLineNode;
            break;

        case TargetType::VariableName:
            // The declaration might have several nodes (like a pointer, initialization braces, etc.), so we go to the deepest one for the name
            targetNode = targetLineNode;
            while (true) {
                TSNode nextNode = getField(targetNode, "declarator");
                if (ts_node_is_null(nextNode)) {
                    break;
                }
                targetNode = nextNode;
            }
            break;

        case TargetType::VariableType:
            targetNode = getField(targetLineNode, "type");
            break;

        case TargetType::FunctionCall:
            targetNode = findNode(targetBaseNode, { "call_expression" });
            break;

        case TargetType::FunctionParameter: {
            TSNode arguments = getField(findNode(targetBaseNode, { "call_expression" }), "arguments");
            if (!ts_node_is_null(arguments)) {
                targetNode = ts_node_named_child(arguments, target.parameterNumber);
            }
            break;
        }
        case TargetType::TemplateParameter: {
            TSNode templateNode = findNode(targetBaseNode, { "template_function", "template_method" });
            if (!ts_node_is_null(templateNode)) {
                TSNode templateArguments = getField(templateNode, "arguments");
                if (!ts_node_is_null(templateArguments)) {
                    targetNode = ts_node_named_child(templateArguments, target.parameterNumber);
                }
            }
            break;
        }
    }
    return ts_node_is_null(targetNode) ? targetLineNode : targetNode;
}

bool CppSourceParser::canUseArrow(TSNode fullLineNode, TSNode targetNode) {
    TSPoint lineStart   = ts_node_start_point(fullLineNode);
    TSPoint targetStart = ts_node_start_point(targetNode);
    TSPoint targetEnd   = ts_node_end_point(targetNode);

    // If the target starts on the same line as the line OR the target spans multiple lines, don't use an arrow
    if (lineStart.row == targetStart.row || targetStart.row != targetEnd.row) {
        return false;
    }

    // Then check if the target is alone on its line, and if it does, we can use an arrow
    auto siblingOnSameLine = [](TSNode sibling, uint32_t row, bool isPrevious) {
        if (ts_node_is_null(sibling)) {
            return false;
        }
        return (isPrevious ? ts_node_end_point(sibling).row : ts_node_start_point(sibling).row) == row;
    };
    return !siblingOnSameLine(ts_node_prev_named_sibling(targetNode), targetStart.row, true) && !siblingOnSameLine(ts_node_next_named_sibling(targetNode), targetStart.row, false);
}

bool CppSourceParser::canInsertOnOwnLine(TSNode fullLineNode, TSNode anchorNode, InsertPosition position) {
    if (position == InsertPosition::Before) {
        uint32_t row = ts_node_start_point(anchorNode).row;
        if (row == ts_node_start_point(fullLineNode).row) {
            return false;
        }
        TSNode prev = ts_node_prev_named_sibling(anchorNode);
        return ts_node_is_null(prev) || ts_node_end_point(prev).row != row;
    } else {
        uint32_t row = ts_node_end_point(anchorNode).row;
        if (row == ts_node_end_point(fullLineNode).row) {
            return false;
        }
        TSNode next = ts_node_next_named_sibling(anchorNode);
        return ts_node_is_null(next) || ts_node_start_point(next).row != row;
    }
}