export module Veneer:Location;

import :CMakeOptions;
import std;

namespace vnr {
    // The point of this type is to:
    // 1) Simplify the parameter passed into functions to capture locations (now we just pass 'Location loc = {}' to capture locations)
    // 2) Make it compile to nothing when logging is disabled
    template<bool Enabled> struct LocationImpl;

    template<>
    struct LocationImpl<true> {
        constexpr LocationImpl(const std::source_location& loc = std::source_location::current())
            : location(loc) {}

        std::source_location location;
    };

    template<>
    struct LocationImpl<false> {
        constexpr LocationImpl() = default;
        constexpr LocationImpl(const std::source_location& loc) {}
    };

    using Location = LocationImpl<CMakeOptions::ENABLE_LOGGING>;
}