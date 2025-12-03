cmake_minimum_required(VERSION 3.30)
project(JSON5 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
  SOURCE_FILES
  include/json5/json5_base.hpp
  include/json5/json5_builder.hpp
  include/json5/json5_filter.hpp
  include/json5/json5_input.hpp
  include/json5/json5_output.hpp
  include/json5/json5.hpp
)

include_directories(.)

# Generate the `compile_commands.json` file.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# Tests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
  JSON5Tests
  test/Json5UtilsTests.cpp
  ${SOURCE_FILES}
)

add_custom_command(TARGET JSON5Tests
                   POST_BUILD  # Run after the target is built
                   COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/test/short_example.json5 ${PROJECT_BINARY_DIR}
                   COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/test/twitter.json ${PROJECT_BINARY_DIR}
)

target_link_libraries(
  JSON5Tests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(JSON5Tests)