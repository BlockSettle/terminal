cmake_minimum_required(VERSION 3.0.2)
project(libbreakpad CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(
    -DNOMINMAX
    -DUNICODE
    -DWIN32_LEAN_AND_MEAN
    -D_CRT_SECURE_NO_WARNINGS
    -D_CRT_SECURE_NO_DEPRECATE
    -D_CRT_NONSTDC_NO_DEPRECATE
    -D_LIBCPP_VERSION
)

file(GLOB_RECURSE LIBBREAKPAD_CLIENT_SOURCES breakpad/src/client/windows/*.cc breakpad/src/common/windows/*.cc)

include_directories("$ENV{VSINSTALLDIR}/DIA SDK/include")

file(GLOB LIBBREAKPAD_COMMON_SOURCES breakpad/src/common/*.cc breakpad/src/common/*.c breakpad/src/client/*.cc)

list(APPEND LIBBREAKPAD_CLIENT_SOURCES ${LIBBREAKPAD_COMMON_SOURCES})

list(FILTER LIBBREAKPAD_CLIENT_SOURCES EXCLUDE REGEX "/tests|/unittests|_unittest")

list(FILTER LIBBREAKPAD_CLIENT_SOURCES EXCLUDE REGEX "language.cc|path_helper.cc|stabs_to_module.cc|stabs_reader.cc|minidump_file_writer.cc")

add_library(libbreakpad ${LIBBREAKPAD_CLIENT_SOURCES})

target_link_libraries(libbreakpad PRIVATE wininet.lib)

target_include_directories(libbreakpad
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/breakpad/src
)
