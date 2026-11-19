module Veneer:Snippet;

import :AnsiFormatting;
import :ReflectedStruct;
import std.compat;

namespace vnr {
    enum class TargetType { WholeLine, VariableName, VariableType, FunctionCall, FunctionParameter, TemplateParameter };

    struct Target {
        TargetType type;
        uint32_t parameterNumber = 0;
        std::source_location location; // This will only differ from the main snippet's location in the case of structs, where each field has its own std::source_location extracted via reflection
        std::string text;
        AnsiFormat format = nullptr;

        static Target wholeLine        (std::string_view text, AnsiFormat format = nullptr);
        static Target variableName     (std::string_view text, AnsiFormat format = nullptr);
        static Target variableType     (std::string_view text, AnsiFormat format = nullptr);
        static Target functionCall     (std::string_view text, AnsiFormat format = nullptr);
        static Target functionParameter(uint32_t index, std::string_view text, AnsiFormat format = nullptr);
        static Target templateParameter(uint32_t index, std::string_view text, AnsiFormat format = nullptr);
    };

    enum class InsertPosition { Before, After };

    struct Insertion {
        std::source_location anchorLocation;
        InsertPosition position;

        // Code and text are separated so we can insert code and then separately use our existing text formatter to annotate it
        std::string code;
        std::string text;
        AnsiFormat format = nullptr;
    };

    struct Snippet {
        std::source_location location;
        std::vector<Target> targets;
        std::vector<Insertion> insertions;
        const ReflectedStruct* reflectedStruct = nullptr; // used to reconstruct the struct if we couldn't load the source file
    };

    // Our own version of TSPoint so we don't have to use tree-sitter outside the parsing file
    struct SourcePoint {
        uint32_t row = 0;
        uint32_t column = 0;
    };

    struct ParsedTarget {
        AnsiFormat format = nullptr;
        std::string text;
        SourcePoint start;
        SourcePoint end;
        bool useArrow = false;
        uint32_t caretStart = std::numeric_limits<uint32_t>::max();
        uint32_t caretEnd   = 0;
    };

    struct ParsedInsertion {
        SourcePoint anchor;
        InsertPosition position;
        bool isInline = false;
        std::string code;
        std::string text;
        AnsiFormat format = nullptr;
    };

    struct ParsedSnippet {
        std::string sourceCode;
        size_t startByte  = 0;
        uint32_t startRow = 0;
        uint32_t endRow   = 0;
        std::vector<ParsedTarget> targets;
        std::vector<ParsedInsertion> insertions;
        bool showLineNumbers = true; // used to avoid printing lines if we are reconstructing something from reflected data rather than from source code
    };

    struct StructTarget {
        std::variant<Target, Insertion> data;
        static StructTarget field      (const ReflectedStructField& f, std::string_view text, AnsiFormat format = nullptr);
        static StructTarget fieldType  (const ReflectedStructField& f, std::string_view text, AnsiFormat format = nullptr);
        static StructTarget fieldName  (const ReflectedStructField& f, std::string_view text, AnsiFormat format = nullptr);
        static StructTarget insertBefore(const ReflectedStructField& f, std::string_view code, std::string_view text = "", AnsiFormat format = Ansi::bold_green);
        static StructTarget insertAfter(const ReflectedStructField& f, std::string_view code, std::string_view text = "", AnsiFormat format = Ansi::bold_green);

    private:
        static std::source_location unwrap(const ReflectedStructField& f);
        static StructTarget makeField(TargetType type, const ReflectedStructField& f, std::string_view text, AnsiFormat format);
        static StructTarget makeInsertion(InsertPosition pos, const ReflectedStructField& f, std::string_view code, std::string_view text, AnsiFormat format);
    };
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

Target Target::wholeLine(std::string_view text, AnsiFormat format) {
    return { .type = TargetType::WholeLine, .text = std::string(text), .format = format };
}

Target Target::variableName(std::string_view text, AnsiFormat format) {
    return { .type = TargetType::VariableName, .text = std::string(text), .format = format };
}

Target Target::variableType(std::string_view text, AnsiFormat format) {
    return { .type = TargetType::VariableType, .text = std::string(text), .format = format };
}

Target Target::functionCall(std::string_view text, AnsiFormat format) {
    return { .type = TargetType::FunctionCall, .text = std::string(text), .format = format };
}

Target Target::functionParameter(uint32_t index, std::string_view text, AnsiFormat format) {
    return { .type = TargetType::FunctionParameter, .parameterNumber = std::max(index, 1u) - 1, .text = std::string(text), .format = format };
}

Target Target::templateParameter(uint32_t index, std::string_view text, AnsiFormat format) {
    return { .type = TargetType::TemplateParameter, .parameterNumber = std::max(index, 1u) - 1, .text = std::string(text), .format = format };
}

StructTarget StructTarget::field(const ReflectedStructField& f, std::string_view text, AnsiFormat format) {
    return makeField(TargetType::WholeLine, f, text, format);
}

StructTarget StructTarget::fieldType(const ReflectedStructField& f, std::string_view text, AnsiFormat format) {
    return makeField(TargetType::VariableType, f, text, format);
}

StructTarget StructTarget::fieldName(const ReflectedStructField& f, std::string_view text, AnsiFormat format) {
    return makeField(TargetType::VariableName, f, text, format);
}

StructTarget StructTarget::insertBefore(const ReflectedStructField& f, std::string_view code, std::string_view text, AnsiFormat format) {
    return makeInsertion(InsertPosition::Before, f, code, text, format);
}

StructTarget StructTarget::insertAfter(const ReflectedStructField& f, std::string_view code, std::string_view text, AnsiFormat format) {
    return makeInsertion(InsertPosition::After, f, code, text, format);
}

std::source_location StructTarget::unwrap(const ReflectedStructField& f) {
    if constexpr (CMakeOptions::ENABLE_LOGGING) {
        return f.location;
    }
    return {};
}

StructTarget StructTarget::makeField(TargetType type, const ReflectedStructField& f, std::string_view text, AnsiFormat format) {
    return { Target{ .type = type, .location = unwrap(f), .text = std::string(text), .format = format } };
}

StructTarget StructTarget::makeInsertion(InsertPosition pos, const ReflectedStructField& f, std::string_view code, std::string_view text, AnsiFormat format) {
    return { Insertion{ .anchorLocation = unwrap(f), .position = pos, .code = std::string(code), .text = std::string(text), .format = format } };
}