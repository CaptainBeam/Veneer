export module Veneer;

export import :Location;
import :Log;
import :Reflection;
using namespace vnr;

enum class TestCodes { Parameters, Structs };

template<> struct vnr::LogCodes<TestCodes> {
    static constexpr std::string_view codePrefix = "TEST";
};

template<typename T>
int add(T a, T b, Location loc = {}) {
    Log::error<TestCodes::Parameters>("we are pointing at both function parameters")
        .addSnippet(loc, {
            Target::functionParameter(1, "this is the first one"),
            Target::functionParameter(2, "and this is the second one"),
        })
        .addNote("this system uses tree-sitter to parse source code")
        .addHelp("this will provide helpful messages to anyone working with Veneer")
        .print();
    return a + b;
}

struct TestStruct {
    int x;
};

template<typename T>
void verifyStruct() {
    auto structData = Reflect::typeData<T>();
    Log::warning<TestCodes::Structs>("we are annotating a struct")
        .addStruct(structData, {
            StructTarget::field(structData.fields[0], "this is the first field"),
            StructTarget::insertAfter(structData.fields[0], "float newField;", "this is a brand new field")
        })
        .addWarn("this is only possible due to C++26 reflection")
        .print();
}

export void runTest() {
    int sum = add<int>(
        3,
        5
        );
    verifyStruct<TestStruct>();
}