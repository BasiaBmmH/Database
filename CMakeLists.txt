cmake_minimum_required(VERSION 3.27)
project(2425L_11_s24660_OZ)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 11.0.2
)
FetchContent_MakeAvailable(fmt)

add_executable(2425L_11_s24660_OZ
        main.cpp
        database.cpp
)

target_link_libraries(2425L_11_s24660_OZ fmt)
