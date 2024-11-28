cmake_minimum_required(VERSION 3.28)
project(jbutil LANGUAGES C)

# Compiler settings
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${N})
endif()

# Enable compiler cache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
endif()

# Ninja-specific flags
if(CMAKE_GENERATOR STREQUAL "Ninja")
    # Optionally limit jobs to prevent overwhelming the system
    set(CMAKE_NINJA_MAKE_PROGRAM ninja)
    set(CMAKE_NINJA_MAKE_ARGUMENTS -j${N})

    # Use colored output for better readability
    set(CMAKE_COLOR_MAKEFILE ON)
endif()

# Enable Link Time Optimization for release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Remove -ffast-math flag
string(REPLACE "-ffast-math" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# External dependencies
include(FetchContent)

# SQLite3
FetchContent_Declare(
        sqlite3
        URL https://www.sqlite.org/2018/sqlite-amalgamation-3260000.zip
        URL_HASH SHA3_256=275dec17c07a8d8a5d05644420159cd19c79fa49556bf898e7cfbb3d204bb416
    )
FetchContent_MakeAvailable(sqlite3)

# Add subdirectories
add_subdirectory(cJSON)
add_subdirectory(zlib)


add_library(minizip minizip/ioapi.c minizip/unzip.c minizip/zip.c)
target_include_directories(minizip PRIVATE zlib)
target_link_libraries(minizip zlib)

# SQLite3 Library
add_library(sqlite3 SHARED
        ${sqlite3_SOURCE_DIR}/sqlite3.c
    )
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})

# LIPC stub Library
add_library(lipc SHARED
        src/stubs/lipc/lipc.c
    )
target_include_directories(lipc PRIVATE openlipc/include)

# Scanner stub Library
add_library(scanner SHARED
        src/stubs/scanner/scanner.c
    )
target_include_directories(scanner PRIVATE cJSON)

# EPub Extractor Library
add_library(epubExtractor SHARED
        src/extractor/main.c
    )
target_include_directories(epubExtractor PRIVATE
        openlipc/include
        cJSON
        src/stubs/scanner
        minizip/minizip
    )
target_link_libraries(epubExtractor PRIVATE
        lipc
        scanner
        cjson
        minizip
    )
add_dependencies(epubExtractor minizip zlib)

# Daemon Executable
add_executable(jbutild)
set_target_properties(jbutild PROPERTIES
        C_VISIBILITY_PRESET hidden
        COMPILE_WARNING_AS_ERROR ON
    )
target_include_directories(jbutild PRIVATE
        openlipc/include
        src/stubs/scanner
        cJSON
        ${sqlite3_SOURCE_DIR}
    )
target_link_libraries(jbutild PRIVATE
        lipc
        sqlite3
        dl
        cjson
        scanner
        pthread
    )
target_sources(jbutild PRIVATE
        src/daemon/main.c
    )
