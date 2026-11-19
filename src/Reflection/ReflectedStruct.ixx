module Veneer:ReflectedStruct;

import :Location;
import std.compat;

namespace vnr {
    struct ReflectedStruct;

    struct ReflectedStructField {
        std::string_view type;
        std::string_view name;
        uint32_t size;
        uint32_t offset;
        Location location;
        const ReflectedStruct* subStruct = nullptr;
    };

    struct ReflectedStruct {
        std::string_view name;
        uint32_t size;
        Location location;
        std::span<const ReflectedStructField> fields;
    };
}