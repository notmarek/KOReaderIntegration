cmake_minimum_required(VERSION 3.28)
project(jbutil LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2018/sqlite-amalgamation-3260000.zip
    URL_HASH SHA3_256=275dec17c07a8d8a5d05644420159cd19c79fa49556bf898e7cfbb3d204bb416
)
FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3 SHARED
    ${sqlite3_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})

add_library(lipc SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/lipc.c
)
target_include_directories(lipc PRIVATE openlipc/include)

add_executable(jbutil)
set_target_properties(jbutil PROPERTIES
    C_VISIBILITY_PRESET hidden
    COMPILE_WARNING_AS_ERROR ON
)

target_include_directories(jbutil PRIVATE
    openlipc/include
    ${sqlite3_SOURCE_DIR}
)

target_link_libraries(jbutil PRIVATE
    lipc
    sqlite3
    dl
    pthread
)

target_sources(jbutil PRIVATE
    src/main.c
)
