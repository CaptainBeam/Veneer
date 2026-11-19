cmake_minimum_required(VERSION 4.4.2)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
set(CMAKE_CXX_MODULE_STD ON)

project(Veneer VERSION 1.0 LANGUAGES C CXX) # C is required for tree-sitter

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -freflection")

option(VENEER_BUILD_SANDBOX   "Build the main.cpp file used for testing random code" ON) # this should be removed later

option(VENEER_ENABLE_COLORED_LOGS "Color Veneer's output logs using ANSI escape codes (ON by default)" ON)
option(VENEER_ENABLE_UNICODE_LOGS "Use unicode characters in Veneer's output logs (ON by default)" ON)
option(VENEER_ENABLE_LOGGING      "Enable Veneer's rich diagnostic system (ON by default)" ON)

# Download and use the CMake Package Manager module
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
    file(
            DOWNLOAD
            https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.43.2/CPM.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
            EXPECTED_HASH SHA256=49a3bef91ceb65bb66d57255e12d1ffd22abc2f6408fa9fe4534c544a2f232aa
    )
endif()
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

add_subdirectory(lib)

add_library(Veneer STATIC)
add_library(Veneer::Veneer ALIAS Veneer)

target_sources(Veneer PUBLIC FILE_SET CXX_MODULES FILES
        src/Veneer.ixx
        src/CMakeOptions.ixx

        src/Log/Log.ixx
        src/Log/AnsiFormatting.ixx
        src/Log/Printer.ixx
        src/Log/Message.ixx
        src/Log/Formatter.ixx
        src/Log/MessageBuilder.ixx
        src/Log/Location.ixx
        src/Log/CppSourceParser.ixx
        src/Log/Snippet.ixx
        src/Log/UTF8Formatting.ixx

        src/Reflection/ReflectedStruct.ixx
        src/Reflection/Reflection.ixx
)

target_include_directories(Veneer PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        ${Stb_INCLUDE_DIR}
)

target_compile_definitions(Veneer PRIVATE
        VENEER_ENABLE_COLORED_LOGS=$<BOOL:${VENEER_ENABLE_COLORED_LOGS}>
        VENEER_ENABLE_UNICODE_LOGS=$<BOOL:${VENEER_ENABLE_UNICODE_LOGS}>
        VENEER_ENABLE_LOGGING=$<BOOL:${VENEER_ENABLE_LOGGING}>
)

target_link_libraries(Veneer
        PUBLIC
            stdc++exp
        PRIVATE
            TreeSitter::Core
            TreeSitter::Cpp
            TreeSitter::GLSL
)

if(VENEER_BUILD_SANDBOX)
    add_executable(VeneerSandbox src/main.cpp)
    target_link_libraries(VeneerSandbox PRIVATE Veneer::Veneer)

    if(MINGW)
        # GCC gives a linker error related to std::format having multiple definitions which is fixed by disabling this warning
        target_link_options(VeneerSandbox PRIVATE -Wl,--allow-multiple-definition)
    endif()
endif()