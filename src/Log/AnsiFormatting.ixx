module Veneer:AnsiFormatting;

import :CMakeOptions;
import std;

namespace vnr {
    using AnsiFormat = std::string(*)(std::string_view);

    class Ansi {
    public:
        Ansi() = delete;
        static constexpr std::string bold        (std::string_view text) { return wrap(text, "\033[1m"); }
        static constexpr std::string black       (std::string_view text) { return wrap(text, "\033[30m"); }
        static constexpr std::string red         (std::string_view text) { return wrap(text, "\033[31m"); }
        static constexpr std::string green       (std::string_view text) { return wrap(text, "\033[32m"); }
        static constexpr std::string yellow      (std::string_view text) { return wrap(text, "\033[33m"); }
        static constexpr std::string blue        (std::string_view text) { return wrap(text, "\033[34m"); }
        static constexpr std::string magenta     (std::string_view text) { return wrap(text, "\033[35m"); }
        static constexpr std::string cyan        (std::string_view text) { return wrap(text, "\033[36m"); }
        static constexpr std::string white       (std::string_view text) { return wrap(text, "\033[37m"); }
        static constexpr std::string bold_black  (std::string_view text) { return wrap(text, "\033[1;30m"); }
        static constexpr std::string bold_red    (std::string_view text) { return wrap(text, "\033[1;31m"); }
        static constexpr std::string bold_green  (std::string_view text) { return wrap(text, "\033[1;32m"); }
        static constexpr std::string bold_yellow (std::string_view text) { return wrap(text, "\033[1;33m"); }
        static constexpr std::string bold_blue   (std::string_view text) { return wrap(text, "\033[1;34m"); }
        static constexpr std::string bold_magenta(std::string_view text) { return wrap(text, "\033[1;35m"); }
        static constexpr std::string bold_cyan   (std::string_view text) { return wrap(text, "\033[1;36m"); }
        static constexpr std::string bold_white  (std::string_view text) { return wrap(text, "\033[1;37m"); }

    private:
        // To use ANSI escape codes for formatting strings, we must wrap the string with the
        // symbol corresponding to the format we want and reset the output after it with the
        // "\033[0m" reset code. If the CMake option is disabled, we pass the text unchanged.
        static constexpr std::string wrap(std::string_view text, std::string_view style) {
            if constexpr (!CMakeOptions::ENABLE_COLORED_LOGS) {
                return std::string(text);
            }
            constexpr std::string_view reset = "\033[0m";
            std::string result;
            result.reserve(style.size() + text.size() + reset.size());
            result.append(style);
            result.append(text);
            result.append(reset);
            return result;
        }
    };
}