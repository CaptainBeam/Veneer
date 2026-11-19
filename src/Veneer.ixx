export module Veneer;

export import :Location;

import :Log;
import :Reflection;

using namespace vnr;

enum class TestCodes { Location };

template<> struct vnr::LogCodes<TestCodes> {
    static constexpr std::string_view codePrefix = "TEST";
};

struct TestStruct {
    int x;
};

template<typename T, uint32_t N>
int add(int a, int b, Location loc = {}) {
    auto data = Reflect::typeData<TestStruct>();

    Log::error<TestCodes::Location>("this should point to various things")
        .addStruct(data, {
            StructTarget::fieldName(data.fields[0], "are we pointing at x?"),
            StructTarget::fieldType(data.fields[0], "are we pointing at int?"),
            StructTarget::insertBefore(data.fields[0], "int beforeX;", "this is a new field!"),
            StructTarget::insertAfter(data.fields[0], "int afterX;", "this is also a new field!")
        })
        .addSnippet(loc, {
            Target::functionParameter(1, "can we point at 3?"),
            Target::functionParameter(2, "can we point at 5?"),
            Target::templateParameter(1, "can we point at int?"),
            Target::templateParameter(2, "can we point at 3?"),
            Target::variableName("can we point at sum?"),
            Target::variableType("can we point at int?"),
        })
        .addNote("did it work?")
        .print();
    return (a + b) * N;
}

export void runTest() {
    int sum = add<int, 3>(3, 5);
}