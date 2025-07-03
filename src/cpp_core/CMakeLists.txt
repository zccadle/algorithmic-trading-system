cmake_minimum_required(VERSION 3.14)
project(cpp_core)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Create order book library
add_library(order_book src/order_book.cpp)

# Create test executable
add_executable(order_book_test src/main.cpp)
target_link_libraries(order_book_test order_book)

# Fetch Google Benchmark
include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# Create benchmark executable
add_executable(order_book_benchmark src/order_book_benchmark.cpp)
target_link_libraries(order_book_benchmark order_book benchmark::benchmark)