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
    float y;
    uint32_t z;
};

template<typename T, uint32_t N>
int add(int a, int b, Location loc = {}) {
    auto data = Reflect::typeData<TestStruct>();

    data.location = std::source_location{};

    Log::error<TestCodes::Location>("this should point to various things")
        .addStruct(data, {
            StructTarget::field(data.fields[0], "are we pointing at x?"),
            StructTarget::insertAbove(data.fields[1], "int beforeY;", "this is a new field!"),
            StructTarget::insertBelow(data.fields[1], "int afterY;", "this is also a new field!")
        })
        .addNote("did it work?")
        .print();
    return (a + b) * N;
}

export void runTest() {
    int sum = add<int, 3>(3, 5);
}