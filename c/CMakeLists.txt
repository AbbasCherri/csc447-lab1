cmake_minimum_required(VERSION 3.16)
project(csc447_lab1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra")

find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(sequential sequential.c common.h)

add_executable(pthreads pthreads.c common.h)
target_link_libraries(pthreads PRIVATE Threads::Threads)

add_executable(openmp openmp.c common.h)
target_link_libraries(openmp PRIVATE OpenMP::OpenMP_C)
