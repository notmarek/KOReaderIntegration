cmake_minimum_required(VERSION 3.28)
project(jbutil LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


#file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/lipc.c "#include \"openlipc.h\"// int LipcOpenEx(void) { return 0; }")

add_library(lipc SHARED ${CMAKE_CURRENT_BINARY_DIR}/lipc.c)

include(FetchContent)
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2018/sqlite-amalgamation-3260000.zip
)
FetchContent_MakeAvailable(sqlite3)

add_library(sqlite3 SHARED _deps/sqlite3-src/sqlite3.c _deps/sqlite3-src/sqlite3.h)

target_include_directories(lipc PRIVATE openlipc/include)

add_executable(jbutil)
set_target_properties(jbutil PROPERTIES C_VISIBILITY_PRESET hidden)
target_include_directories(jbutil PRIVATE openlipc/include _deps/sqlite3-src)
target_link_libraries(jbutil PRIVATE lipc sqlite3 dl pthread)
target_sources(jbutil PRIVATE src/main.c)
