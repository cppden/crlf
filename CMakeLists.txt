cmake_minimum_required(VERSION 2.8)

set(THIS_NAME search)

project(${THIS_NAME})
enable_language(CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

include_directories(
  ${GTEST_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/
)

find_package(Benchmark REQUIRED)
if (BENCHMARK_FOUND)
  include_directories( ${BENCHMARK_INCLUDE_DIRS})
else (BENCHMARK_FOUND)
  message(FATAL_ERROR "Google benchmark not found. Please install it.")
endif (BENCHMARK_FOUND)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
if (COMPILER_SUPPORTS_CXX17)
	add_definitions(-std=c++17)
else ()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
endif ()


# Library path
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -L\".\" ")

set(UT_SRCS
  search.cpp
)

add_compile_options(
    -Werror 
    -Wall 
    -Wextra 
    -Waddress
    -Warray-bounds
    -Winit-self
    -Wunreachable-code
    -pedantic
    -pedantic-errors
)

add_executable(bm_${THIS_NAME} ${UT_SRCS})
#  set_target_properties(bm_med PROPERTIES
#    INTERPROCEDURAL_OPTIMIZATION ON)
target_link_libraries(bm_${THIS_NAME}
  ${BENCHMARK_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)

if(DEFINED ENV{BUILD_FLAGS})
    set(BUILD_FLAGS "$ENV{BUILD_FLAGS}")
else ()
    set(BUILD_FLAGS "-O3")
endif ()

set_target_properties(bm_${THIS_NAME} PROPERTIES COMPILE_FLAGS 
    ${BUILD_FLAGS}
)

