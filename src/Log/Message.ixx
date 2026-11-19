module Veneer:Message;

import std.compat;

namespace vnr {
    enum class LineType { Blank, GutterOnly, Addition, Location, Note, Help, Warn, Error };

    struct Line {
        LineType type = LineType::GutterOnly;
        uint32_t lineNumber = 0; // 0 means no number
        std::string text;
        std::source_location location;
    };

    enum class MessageType { Trace, Note, Warning, Error };

    struct Message {
        MessageType type = MessageType::Note;
        std::string code; // not applicable for traces
        std::string title;
        std::vector<Line> lines;
    };
}