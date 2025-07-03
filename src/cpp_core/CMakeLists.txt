cmake_minimum_required(VERSION 3.10)
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