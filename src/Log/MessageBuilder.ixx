module Veneer:MessageBuilder;

import :Message;
import :Printer;
import :Location;
import :Formatter;
import :CMakeOptions;
import :ReflectedStruct;
import :AnsiFormatting;
import std.compat;

namespace vnr {
    template<bool Enabled> class MessageBuilderImpl;

    template<>
    class MessageBuilderImpl<true> {
    public:
        // these must explicitly take the 'true' variant of LocationImpl because they try to access its contents, which don't exist on the 'false' variant
        MessageBuilderImpl& addLocation(LocationImpl<true> location = {});
        MessageBuilderImpl& addSnippet(LocationImpl<true> loc, Target target);
        MessageBuilderImpl& addSnippet(LocationImpl<true> loc, std::vector<Target> targets);
        MessageBuilderImpl& addStruct(const ReflectedStruct& mainStruct, StructTarget target);
        MessageBuilderImpl& addStruct(const ReflectedStruct& mainStruct, std::vector<StructTarget> targets);

        template<typename... Args> MessageBuilderImpl& addBlankLine(std::format_string<Args...> fmt, Args&&... args);
        template<typename... Args> MessageBuilderImpl& addNote(std::format_string<Args...> fmt, Args&&... args);
        template<typename... Args> MessageBuilderImpl& addHelp(std::format_string<Args...> fmt, Args&&... args);
        template<typename... Args> MessageBuilderImpl& addWarn(std::format_string<Args...> fmt, Args&&... args);
        template<typename... Args> MessageBuilderImpl& addError(std::format_string<Args...> fmt, Args&&... args);

        void print() const;
        void printAndThrow() const;

    private:
        MessageBuilderImpl() = default;
        MessageBuilderImpl(std::string_view code, std::string_view title, MessageType type);

        AnsiFormat defaultFormat() const;

        Message message;

        static inline std::mutex printingMutex;
        static inline std::unordered_set<std::string> silencedCodes;
        static inline thread_local Formatter formatter;

        template<bool Enabled> friend class LogImpl;
    };

    template<>
    class MessageBuilderImpl<false> {
    public:
        inline MessageBuilderImpl& addLocation(Location location = {}) { return *this; }
        MessageBuilderImpl& addSnippet(Location loc, Target target) { return *this; }
        MessageBuilderImpl& addSnippet(Location loc, std::vector<Target> targets) { return *this; }
        MessageBuilderImpl& addStruct(const ReflectedStruct& mainStruct, StructTarget target) { return *this; }
        MessageBuilderImpl& addStruct(const ReflectedStruct& mainStruct, std::vector<StructTarget> targets) { return *this; }

        template<typename... Args> MessageBuilderImpl& addBlankLine(std::format_string<Args...> fmt, Args&&... args) { return *this; }
        template<typename... Args> MessageBuilderImpl& addNote(std::format_string<Args...> fmt, Args&&... args) { return *this; }
        template<typename... Args> MessageBuilderImpl& addHelp(std::format_string<Args...> fmt, Args&&... args) { return *this; }
        template<typename... Args> MessageBuilderImpl& addWarn(std::format_string<Args...> fmt, Args&&... args) { return *this; }
        template<typename... Args> MessageBuilderImpl& addError(std::format_string<Args...> fmt, Args&&... args) { return *this; }

        inline void print() const {}
        inline void printAndThrow() const {}

    private:
        MessageBuilderImpl() = default;
        MessageBuilderImpl(std::string_view code, std::string_view title, MessageType type) {}

        template<bool Enabled> friend class LogImpl;
    };

    using MessageBuilder = MessageBuilderImpl<CMakeOptions::ENABLE_LOGGING>;
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

inline MessageBuilderImpl<true>& MessageBuilderImpl<true>::addLocation(LocationImpl<true> location) {
    message.lines.push_back(Line{
        .type = LineType::Location,
        .location = location.location
    });
    return *this;
}

inline MessageBuilderImpl<true>& MessageBuilderImpl<true>::addSnippet(LocationImpl<true> loc, Target target) {
    std::vector<Target> targets;
    targets.push_back(std::move(target));
    return addSnippet(loc, std::move(targets));
}

inline MessageBuilderImpl<true>& MessageBuilderImpl<true>::addSnippet(LocationImpl<true> loc, std::vector<Target> targets) {
    for (auto& target : targets) {
        if (!target.format) {
            target.format = defaultFormat(); // the targets received are not passed by const reference so we can make this modification
        }
    }

    auto lines = formatter.format(Snippet{
        .location = loc.location,
        .targets  = std::move(targets),
    });
    message.lines.append_range(lines);
    return *this;
}

inline MessageBuilderImpl<true>& MessageBuilderImpl<true>::addStruct(const ReflectedStruct& mainStruct, StructTarget target) {
    std::vector<StructTarget> targets;
    targets.push_back(std::move(target));
    return addStruct(mainStruct, std::move(targets));
}

inline MessageBuilderImpl<true>& MessageBuilderImpl<true>::addStruct(const ReflectedStruct& mainStruct, std::vector<StructTarget> targets) {
    Snippet snippet{ .location = mainStruct.location, .reflectedStruct = &mainStruct };
    for (auto& entry : targets) {
        if (auto* target = std::get_if<Target>(&entry.data)) {
            if (!target->format) {
                target->format = defaultFormat();
            }
            snippet.targets.push_back(std::move(*target));
        } else if (auto* insertion = std::get_if<Insertion>(&entry.data)) {
            if (!insertion->format) {
                insertion->format = Ansi::bold_green;
            }
            snippet.insertions.push_back(std::move(*insertion));
        }
    }

    auto lines = formatter.format(snippet);
    message.lines.append_range(lines);
    return *this;
}

template<typename... Args>
MessageBuilderImpl<true>& MessageBuilderImpl<true>::addBlankLine(std::format_string<Args...> fmt, Args&&... args) {
    message.lines.push_back(Line{
        .type = LineType::Blank,
        .text = std::format(fmt, std::forward<Args>(args)...)
    });
    return *this;
}

template<typename... Args>
MessageBuilderImpl<true>& MessageBuilderImpl<true>::addNote(std::format_string<Args...> fmt, Args&&... args) {
    message.lines.push_back(Line{
        .type = LineType::Note,
        .text = std::format(fmt, std::forward<Args>(args)...)
    });
    return *this;
}

template<typename... Args>
MessageBuilderImpl<true>& MessageBuilderImpl<true>::addHelp(std::format_string<Args...> fmt, Args&&... args) {
    message.lines.push_back(Line{
        .type = LineType::Help,
        .text = std::format(fmt, std::forward<Args>(args)...)
    });
    return *this;
}

template<typename... Args>
MessageBuilderImpl<true>& MessageBuilderImpl<true>::addWarn(std::format_string<Args...> fmt, Args&&... args) {
    message.lines.push_back(Line{
        .type = LineType::Warn,
        .text = std::format(fmt, std::forward<Args>(args)...)
    });
    return *this;
}

template<typename... Args>
MessageBuilderImpl<true>& MessageBuilderImpl<true>::addError(std::format_string<Args...> fmt, Args&&... args) {
    message.lines.push_back(Line{
        .type = LineType::Error,
        .text = std::format(fmt, std::forward<Args>(args)...)
    });
    return *this;
}

inline void MessageBuilderImpl<true>::print() const {
    std::lock_guard lock(printingMutex);
    // Errors should not be silenceable
    if (silencedCodes.contains(message.code) && message.type != MessageType::Error) {
        return;
    }
    Printer::print(message);
}

inline void MessageBuilderImpl<true>::printAndThrow() const {
    print();
    throw std::runtime_error(message.title); // throw a simple exception with the message title since the main message is printed to the console directly
}

inline MessageBuilderImpl<true>::MessageBuilderImpl(std::string_view code, std::string_view title, MessageType type) {
    message.title = title;
    message.code = code;
    message.type = type;
}

inline AnsiFormat MessageBuilderImpl<true>::defaultFormat() const {
    switch (message.type) {
        case MessageType::Trace:   return Ansi::bold;
        case MessageType::Note:    return Ansi::bold_cyan;
        case MessageType::Warning: return Ansi::bold_yellow;
        case MessageType::Error:   return Ansi::bold_red;
    }
    return Ansi::bold;
}