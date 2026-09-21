cmake_minimum_required(VERSION 3.30.0)
project(md5 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if (DEFINED CLANG_GCC_TOOLCHAIN_PATH)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --gcc-toolchain=${CLANG_GCC_TOOLCHAIN_PATH}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --gcc-toolchain=${CLANG_GCC_TOOLCHAIN_PATH}")
    endif()
    if(DEFINED CLANG_CXX_COMPILER_FLAGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_CXX_COMPILER_FLAGS}")
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(DEFINED GCC_CXX_COMPILER_FLAGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_CXX_COMPILER_FLAGS}")
    endif()
endif()

find_package(Boost REQUIRED COMPONENTS iostreams)

add_executable(md5++ main.cpp static_tests.cpp)
target_link_libraries(md5++ PRIVATE Boost::iostreams)

add_executable(md5++_v2 main2.cpp static_tests.cpp)
