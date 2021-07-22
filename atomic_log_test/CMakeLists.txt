cmake_minimum_required(VERSION 3.14)

project(atomiclog)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out )

include(FindThreads)
find_package(PkgConfig QUIET)
pkg_check_modules(LIBPMEM REQUIRED libpmem)

add_executable(atomic_log_bench " ")
target_sources(
        atomic_log_bench PRIVATE
        "log.cc"
        "log.h"
        "atomic_log_bench.cc"
        )
target_include_directories(atomic_log_bench PUBLIC ${LIBPMEM_INCLUDE_DIRS})
target_link_libraries(atomic_log_bench benchmark ${LIBPMEM_LIBRAIRES})