module;
#include <cpp-tree-sitter.h>
module Veneer:CppSourceParser;

import :Snippet;
import std.compat;

extern "C" const TSLanguage* tree_sitter_cpp();

namespace vnr {
    class CppSourceParser {
    public:
        std::optional<ParsedSnippet> parse(const Snippet& snippet);

    private:
        static std::string loadFile(const std::filesystem::path& path);
        static std::optional<ts::Node> nodeAtLocation(ts::Node root, const std::source_location& location);
        static std::optional<ts::Node> findNode(ts::Node startNode, std::initializer_list<std::string_view> targetTypes, bool searchInwards = true);
        static ts::Node findTargetNode(ts::Node targetBaseNode, ts::Node targetLineNode, const Target& target);
        static bool hasNamedSiblingOnRow(ts::Node node, uint32_t row, bool after);
        static bool canUseArrow(ts::Node fullLineNode, ts::Node targetNode);
        static bool canInsertOnOwnLine(ts::Node fullLineNode, ts::Node anchorNode, InsertPosition position);

        std::expected<ts::Parser, ts::Error> parser = ts::Parser::create(tree_sitter_cpp());
    };
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

std::optional<ParsedSnippet> CppSourceParser::parse(const Snippet& snippet) {
    if (!parser) {
        return std::nullopt;
    }

    std::string sourceCode = loadFile(snippet.location.file_name());
    if (sourceCode.empty()) {
        return std::nullopt;
    }

    const auto tree = parser->parse(sourceCode);
    if (!tree) {
        return std::nullopt;
    }

    ts::Node root = tree->getRootNode();

    // This is the node that the compiler's std::source_location throws us at
    ts::Node baseNode = nodeAtLocation(root, snippet.location).value_or(root);
    ts::Node fullLineNode = findNode(baseNode, { "declaration", "expression_statement", "struct_specifier", "class_specifier" }, false).value_or(baseNode);

    std::vector<ParsedTarget> targets;
    for (const auto& target : snippet.targets) {
        const bool hasCustomLocation = target.location.line() != 0;
        ts::Node targetBaseNode = hasCustomLocation
                                  ? nodeAtLocation(root, target.location).value_or(baseNode)
                                  : baseNode;
        ts::Node targetLineNode = hasCustomLocation
                                  ? findNode(targetBaseNode, { "field_declaration", "declaration", "expression_statement" }, false).value_or(targetBaseNode)
                                  : fullLineNode;
        ts::Node targetNode = findTargetNode(targetBaseNode, targetLineNode, target);
        auto [targetStart, targetEnd] = targetNode.getPointRange();

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
        ts::Node anchorBase = nodeAtLocation(root, insertion.anchorLocation).value_or(baseNode);
        ts::Node anchorLine = findNode(anchorBase, { "field_declaration", "declaration", "expression_statement" }, false).value_or(anchorBase);

        auto [anchorStart, anchorEnd] = anchorLine.getPointRange();
        auto anchorPoint = (insertion.position == InsertPosition::Before) ? anchorStart : anchorEnd;

        insertions.push_back(ParsedInsertion{
            .anchor   = { anchorPoint.row, anchorPoint.column },
            .position = insertion.position,
            .isInline = !canInsertOnOwnLine(fullLineNode, anchorLine, insertion.position),
            .code     = insertion.code,
            .text     = insertion.text,
            .format   = insertion.format,
        });
    }

    auto [lineStart, lineEnd] = fullLineNode.getPointRange();
    size_t startByte = fullLineNode.getByteRange().start - lineStart.column;

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

std::optional<ts::Node> CppSourceParser::nodeAtLocation(ts::Node root, const std::source_location& location) {
    ts::Point point = { location.line() - 1, location.column() - 1 }; // translate from 1-indexing to 0-indexing
    return root.getDescendantForPointRange({ point, point });
}

std::optional<ts::Node> CppSourceParser::findNode(ts::Node startNode, std::initializer_list<std::string_view> targetTypes, bool searchInwards) {
    auto matches = [targetTypes](auto node) {
        return std::ranges::contains(targetTypes, node.getType());
    };

    // When searching upwards, these are the types we stop at
    static constexpr std::array<std::string_view, 3> searchBoundaries = {
        "call_expression", "declaration", "expression_statement"
    };

    std::optional<ts::Node> current = startNode;
    while (current) {
        if (matches(*current)) {
            return current;
        }
        if (searchInwards && std::ranges::contains(searchBoundaries, current->getType())) {
            auto subtree = ts::preorder(*current);
            if (auto it = std::ranges::find_if(subtree, matches); it != subtree.end()) {
                return *it;
            }
        }
        current = current->getParent();
    }
    return std::nullopt;
}

ts::Node CppSourceParser::findTargetNode(ts::Node targetBaseNode, ts::Node targetLineNode, const Target& target) {
    auto getParameter = [&](std::initializer_list<std::string_view> nodeTypes) {
        return findNode(targetBaseNode, nodeTypes)
            .and_then([](ts::Node node) { return node.getChildByFieldName("arguments"); })
            .and_then([&](ts::Node args) { return args.getNamedChild(target.parameterNumber); })
            .value_or(targetLineNode);
    };

    switch (target.type) {
        case TargetType::WholeLine:
            return targetLineNode;

        case TargetType::VariableName: {
            // The declaration might have several nodes (like a pointer, initialization braces, etc.), so we go to the deepest one for the name
            ts::Node node = targetLineNode;
            while (auto nextNode = node.getChildByFieldName("declarator")) {
                node = *nextNode;
            }
            return node;
        }
        case TargetType::VariableType:
            return targetLineNode.getChildByFieldName("type").value_or(targetLineNode);

        case TargetType::FunctionCall:
            return findNode(targetBaseNode, { "call_expression" }).value_or(targetLineNode);

        case TargetType::FunctionParameter:
            return getParameter({ "call_expression" });

        case TargetType::TemplateParameter:
            return getParameter({ "template_function", "template_method" });
    }
    return targetLineNode;
}

// The C++ wrapper of tree-sitter being used here has no direct named sibling check so we have to do it with this function
bool CppSourceParser::hasNamedSiblingOnRow(ts::Node node, uint32_t row, bool after) {
    auto step = [after](ts::Node node) {
        return after ? node.getNextSibling() : node.getPreviousSibling();
    };
    for (auto sibling = step(node); sibling; sibling = step(*sibling)) {
        if (sibling->isNamed()) {
            auto [start, end] = sibling->getPointRange();
            return (after ? start.row : end.row) == row;
        }
    }
    return false;
}

bool CppSourceParser::canUseArrow(ts::Node fullLineNode, ts::Node targetNode) {
    auto [start, end] = targetNode.getPointRange();
    if (fullLineNode.getPointRange().start.row == start.row || start.row != end.row) {
        return false;
    }
    return !hasNamedSiblingOnRow(targetNode, start.row, false)
        && !hasNamedSiblingOnRow(targetNode, start.row, true);
}

bool CppSourceParser::canInsertOnOwnLine(ts::Node fullLineNode, ts::Node anchorNode, InsertPosition position) {
    bool after = (position == InsertPosition::After);
    uint32_t anchorRow = after ? anchorNode.getPointRange().end.row : anchorNode.getPointRange().start.row;
    uint32_t fullRow   = after ? fullLineNode.getPointRange().end.row : fullLineNode.getPointRange().start.row;
    return anchorRow != fullRow && !hasNamedSiblingOnRow(anchorNode, anchorRow, after);
}