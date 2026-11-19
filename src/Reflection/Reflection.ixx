module Veneer:Reflection;

import :ReflectedStruct;
import std.compat;

namespace vnr {
    // This class is the backbone of a lot of Veneer's machinery and means
    // that Veneer requires a compiler that supports C++26 static reflection.
    class Reflect {
    public:
        Reflect() = delete;

        template<typename T> static consteval ReflectedStruct typeData();
        template<typename T> static consteval std::string_view typeName();

    private:
        static consteval std::string_view nameOf(std::meta::info typeInfo);

        template<typename T> static consteval const ReflectedStruct* substructOf();
        template<typename T> static consteval std::span<const ReflectedStructField> fieldsOf();
    };
}

// ============== //
// Implementation //
// ============== //
using namespace vnr;

template<typename T>
consteval ReflectedStruct Reflect::typeData() {
    static constexpr auto result = ReflectedStruct{
        .name   = typeName<T>(),
        .size   = static_cast<uint32_t>(sizeof(T)),
        .location = std::meta::source_location_of(^^T),
        .fields = fieldsOf<T>(),
    };
    return result;
}

template<typename T>
consteval std::string_view Reflect::typeName() {
    return nameOf(^^T);
}

consteval std::string_view Reflect::nameOf(std::meta::info typeInfo) {
    if (std::meta::has_identifier(typeInfo)) {
        return std::meta::identifier_of(typeInfo);
    }
    return std::meta::display_string_of(typeInfo);
}

template<typename T>
consteval const ReflectedStruct* Reflect::substructOf() {
    if constexpr (!std::is_class_v<T>) {
        return nullptr;
    } else {
        static constexpr ReflectedStruct result = typeData<T>();
        return &result;
    }
}

template<typename T>
consteval std::span<const ReflectedStructField> Reflect::fieldsOf() {
    // This is a recursive function that intends to find the fields of all nested structs in a given struct.
    if constexpr (!std::is_class_v<T>) {
        return {};
    } else {
        static constexpr auto fields = []<typename U>() consteval {
            static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^U, std::meta::access_context::unchecked()));

            std::array<ReflectedStructField, members.size()> result{};
            size_t i = 0;
            template for (constexpr auto member : members) {
                constexpr auto fieldType = std::meta::type_of(member);
                result[i++] = ReflectedStructField{
                    .type   = nameOf(fieldType),
                    .name   = nameOf(member),
                    .size   = static_cast<uint32_t>(std::meta::size_of(fieldType)),
                    .offset = static_cast<uint32_t>(std::meta::offset_of(member).bytes),
                    .location = std::meta::source_location_of(member),
                    .subStruct = substructOf<typename [:fieldType:]>(),
                };
            }
            return result;
        }.template operator()<T>();
        return fields;
    }
}