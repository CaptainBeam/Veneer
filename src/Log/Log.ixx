module Veneer:Log;

import :MessageBuilder;
import :CMakeOptions;
import std.compat;

namespace vnr {
    // To allow a system to use logs, this struct should be defined with an enum that contains that system's log codes.
    // The defined struct must contain a 'static constexpr std::string_view codePrefix', which is placed at the start of
    // the code (ex: RND0001). Since the codes are limited to 4 digits, we cannot have more than 10,000 codes for a system.
    template<typename T>
    struct LogCodes;

    // The logging classes have an enabled and disabled variant, and a 'using' declaration chooses the active one based on the CMake option for logging.
    // This duplicates the code, but unclutters the function implementations at the bottom from constantly using if constexpr and std::conditional_t
    template<bool Enabled> class LogImpl;

    template<>
    class LogImpl<true> {
    public:
        static void enableTraces();
        static void disableTraces();

        static void silenceCode(std::string_view code);
        static void unsilenceCode(std::string_view code);

        template<typename... Args> static void trace(std::format_string<Args...> fmt, Args&&... args);
        template<auto Code, typename... Args> static MessageBuilder note(std::format_string<Args...> fmt, Args&&... args);
        template<auto Code, typename... Args> static MessageBuilder warning(std::format_string<Args...> fmt, Args&&... args);
        template<auto Code, typename... Args> static MessageBuilder error(std::format_string<Args...> fmt, Args&&... args);

    private:
        template<auto Code> static std::string getLogCode();

        static inline bool tracesEnabled = false;
    };

    template<>
    class LogImpl<false> {
    public:
        static inline void enableTraces() {}
        static inline void disableTraces() {}

        static inline void silenceCode(std::string_view code) {}
        static inline void unsilenceCode(std::string_view code) {}

        template<typename... Args> static void trace(std::format_string<Args...> fmt, Args&&... args) {}
        template<auto Code, typename... Args> static MessageBuilder note(std::format_string<Args...> fmt, Args&&... args) { return {}; }
        template<auto Code, typename... Args> static MessageBuilder warning(std::format_string<Args...> fmt, Args&&... args) { return {}; }
        template<auto Code, typename... Args> static MessageBuilder error(std::format_string<Args...> fmt, Args&&... args) { return {}; }
    };

    using Log = LogImpl<CMakeOptions::ENABLE_LOGGING>;
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

inline void LogImpl<true>::enableTraces() {
    tracesEnabled = true;
}

inline void LogImpl<true>::disableTraces() {
    tracesEnabled = false;
}

inline void LogImpl<true>::silenceCode(std::string_view code) {
    MessageBuilderImpl<true>::silencedCodes.insert(std::string(code));
}

inline void LogImpl<true>::unsilenceCode(std::string_view code) {
    MessageBuilderImpl<true>::silencedCodes.erase(std::string(code));
}

template<typename... Args>
void LogImpl<true>::trace(std::format_string<Args...> fmt, Args&&... args) {
    if (!tracesEnabled) {
        return;
    }
    auto message = MessageBuilder("", std::format(fmt, std::forward<Args>(args)...), MessageType::Trace);
    message.print();
}

template<auto Code, typename... Args>
MessageBuilder LogImpl<true>::note(std::format_string<Args...> fmt, Args&&... args) {
    return MessageBuilder(getLogCode<Code>(), std::format(fmt, std::forward<Args>(args)...), MessageType::Note);
}

template<auto Code, typename... Args>
MessageBuilder LogImpl<true>::warning(std::format_string<Args...> fmt, Args&&... args) {
    return MessageBuilder(getLogCode<Code>(), std::format(fmt, std::forward<Args>(args)...), MessageType::Warning);
}

template<auto Code, typename... Args>
MessageBuilder LogImpl<true>::error(std::format_string<Args...> fmt, Args&&... args) {
    return MessageBuilder(getLogCode<Code>(), std::format(fmt, std::forward<Args>(args)...), MessageType::Error);
}

template<auto Code>
std::string LogImpl<true>::getLogCode() {
    using CodeEnum = decltype(Code);
    return std::format("{}{:04}", LogCodes<CodeEnum>::codePrefix, static_cast<uint32_t>(Code));
}