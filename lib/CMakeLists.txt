# Tree-sitter is used to parse code snippets for logging
# Both the C++ and GLSL grammars are added here because those are what Veneer currently
# needs/supports, but more shader language grammars could be added in the future.

# Core tree-sitter
add_library(tree-sitter STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/src/lib.c"
)
target_include_directories(tree-sitter
        PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/src"
)
set_target_properties(tree-sitter PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
add_library(TreeSitter::Core ALIAS tree-sitter)

# C++ grammar
add_library(tree-sitter-cpp STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cpp/src/parser.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cpp/src/scanner.c"
)
target_include_directories(tree-sitter-cpp
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cpp/src"
)
set_target_properties(tree-sitter-cpp PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
add_library(TreeSitter::Cpp  ALIAS tree-sitter-cpp)

# GLSL grammar
add_library(tree-sitter-glsl STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-glsl/src/parser.c"
)
target_include_directories(tree-sitter-glsl
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-glsl/src"
)
set_target_properties(tree-sitter-glsl PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
add_library(TreeSitter::GLSL ALIAS tree-sitter-glsl)