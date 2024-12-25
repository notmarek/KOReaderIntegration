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
    set(CMAKE_BUILD_TYPE Debug)
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
set(ENABLE_CJSON_VERSION_SO OFF CACHE BOOL "Enable cJSON version suffix" FORCE)
add_subdirectory(cJSON EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_STATIC_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
add_subdirectory(zlib EXCLUDE_FROM_ALL)


add_library(miniunz minizip/ioapi.c minizip/unzip.c minizip/zip.c)
target_include_directories(miniunz PRIVATE zlib)
target_link_libraries(miniunz zlib)

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

# xml2 stub Library
add_library(xml2 SHARED
        src/stubs/libxml2/libxml2.c
    )
target_include_directories(xml2 PRIVATE src/stubs/libxml2)
# EPub Extractor Library
add_library(KOIExtractor SHARED
        src/extractor/main.c
        src/extractor/simple_epub_extractor.c
        src/extractor/simple_fb2_extractor.c
        src/extractor/base64.c
        src/extractor/utils.c
    )
target_include_directories(KOIExtractor PRIVATE
        openlipc/include
        cJSON
        src/stubs/scanner
        minizip
        zlib
        src/stubs/libxml2
    )
target_link_libraries(KOIExtractor PRIVATE
        lipc
        scanner
        cjson
        miniunz
        xml2
    )
add_dependencies(KOIExtractor miniunz zlib xml2)

# Koreader helper
add_executable(koreader_helper src/koreader/main.c)
target_include_directories(koreader_helper PRIVATE
        openlipc/include)
target_link_libraries(koreader_helper PRIVATE
        lipc
    )

# SH-IT launcher
add_executable(shit_launcher src/launcher/main.c)
target_include_directories(shit_launcher PRIVATE
        openlipc/include)
target_link_libraries(shit_launcher PRIVATE
        lipc
    )


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
ADD_EXECUTABLE(testExtract src/extractor/simple_epub_extractor.c src/extractor/utils.c)
target_include_directories(testExtract PRIVATE
        openlipc/include
        cJSON
        src/stubs/scanner
        minizip
        zlib
        src/stubs/libxml2
    )
target_link_libraries(testExtract PRIVATE
        lipc
        scanner
        cjson
        miniunz
        xml2
        dl
    )
