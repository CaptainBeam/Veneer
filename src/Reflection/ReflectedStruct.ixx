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
        std::source_location location;
        const ReflectedStruct* subStruct = nullptr;
    };

    struct ReflectedStruct {
        std::string_view name;
        uint32_t size;
        std::source_location location;
        std::span<const ReflectedStructField> fields;
    };
}