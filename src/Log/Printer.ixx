module Veneer:Printer;

import :AnsiFormatting;
import :Message;
import std.compat;

namespace vnr {
    class Printer {
    public:
        Printer() = delete;

        static void print(const Message& message);

    private:
        static std::string buildOutput(const Message& message);
        static uint32_t calculateGutterPosition(const Message& message);
    };
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

void Printer::print(const Message& message) {
    std::print("{}", buildOutput(message));
}

std::string Printer::buildOutput(const Message& message) {
    std::string result;

    switch (message.type) {
        case MessageType::Trace:
            result += Ansi::bold("trace");
            break;
        case MessageType::Note:
            result += Ansi::bold_cyan(std::format("note[{}]", message.code));
            break;
        case MessageType::Warning:
            result += Ansi::bold_yellow(std::format("warning[{}]", message.code));
            break;
        case MessageType::Error:
            result += Ansi::bold_red(std::format("error[{}]", message.code));
            break;
    }
    result += Ansi::bold(std::format(": {}\n", message.title));

    uint32_t gutterPosition = calculateGutterPosition(message);
    auto printGutter = [gutterPosition, &result](uint32_t lineNumber, char gutterChar = '|') {
        if (lineNumber > 0) {
            result += Ansi::bold_blue(std::format("{:>{}} {} ", lineNumber, gutterPosition - 1, gutterChar)); // -1 because we want to right-align while leaving a space between the number and the gutter
        } else {
            result += Ansi::bold_blue(std::format("{:>{}}{} ", " ", gutterPosition, gutterChar)); // add 'gutterPosition' spaces before the |
        }
    };

    for (const auto& line : message.lines) {
        switch (line.type) {
            case LineType::Blank:
                result += line.text + '\n';
                break;
            case LineType::GutterOnly:
                printGutter(line.lineNumber);
                result += line.text + '\n';
                break;
            case LineType::Addition:
                result += Ansi::bold_green(std::format("{:>{}}+ ", " ", gutterPosition));
                result += line.text + '\n';
                break;
            case LineType::Location:
                result += Ansi::bold_blue(std::format("{:>{}}--> ", " ", gutterPosition - 1));
                result += std::format("{}:{}:{}", line.location.file_name(), line.location.line(), line.location.column()) + '\n';
                break;
            case LineType::Note:
                printGutter(line.lineNumber, '=');
                result += Ansi::bold_cyan("note") + ": " + line.text + '\n';
                break;
            case LineType::Help:
                printGutter(line.lineNumber, '=');
                result += Ansi::bold_green("help") + ": " + line.text + '\n';
                break;
            case LineType::Warn:
                printGutter(line.lineNumber, '=');
                result += Ansi::bold_yellow("warn") + ": " + line.text + '\n';
                break;
            case LineType::Error:
                printGutter(line.lineNumber, '=');
                result += Ansi::bold_red("error") + ": " + line.text + '\n';
                break;
        }
    }
    return result + '\n'; // add an extra newline so there is a gap between messages
}

// The gutter should be at least 2 spaces away from the left edge of the line and increase
// dynamically. There should also be at least one space after the number, hence the +1
uint32_t Printer::calculateGutterPosition(const vnr::Message& message) {
    uint32_t largestLineNumber = 0;
    for (const auto& line : message.lines) {
        largestLineNumber = std::max(largestLineNumber, line.lineNumber);
    }
    return static_cast<uint32_t>(std::max(2ull, std::to_string(largestLineNumber).length() + 1));
}