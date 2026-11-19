module Veneer:UTF8Formatting;

import :CMakeOptions;
import std;

namespace vnr {
    class UTF8 {
    public:
        static constexpr std::string_view gutter()     { return utf8Enabled ? "│"   : "|"; }
        static constexpr std::string_view noteGutter() { return utf8Enabled ? "├─"   : "="; }
        static constexpr std::string_view location()   { return utf8Enabled ? " ├─▶" : "-->"; } // an extra space is added to the UTF8 one so we can treat it the same way as the other one during printing
        static constexpr std::string_view branch()     { return utf8Enabled ? "╰─"  : "'-"; }
        static constexpr std::string_view arrowLeft()  { return utf8Enabled ? "◀──" : "<--"; }
        static constexpr std::string_view caret()      { return utf8Enabled ? "^" : "^"; }

    private:
        static constexpr bool utf8Enabled = CMakeOptions::ENABLE_UNICODE_LOGS;
    };
}