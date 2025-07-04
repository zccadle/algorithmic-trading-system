cmake_minimum_required(VERSION 3.14)
project(cpp_core)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols in Release mode for profiling
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g")

# For even better profiling, use frame pointers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer")

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(include)

# Create order book library
add_library(order_book src/order_book.cpp)

# Create FIX parser library
add_library(fix_parser src/fix_parser.cpp)

# Create Smart Order Router library
add_library(smart_order_router src/smart_order_router.cpp)

# Create Market Maker library
add_library(market_maker src/market_maker.cpp)

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

# Fetch Boost (Beast and Asio)
FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
    URL_HASH SHA256=2e64e5d79a738d0fa6fb546c6e5c2bd28f88d268a2a080546f74e5ff98f29d0e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BOOST_INCLUDE_LIBRARIES beast asio system thread)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_MakeAvailable(Boost)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Create benchmark executable
add_executable(order_book_benchmark src/order_book_benchmark.cpp)
target_link_libraries(order_book_benchmark order_book benchmark::benchmark)

# Create replay tool executable
add_executable(replay_tool src/replay_tool.cpp)
target_link_libraries(replay_tool order_book)

# Create websocket client executable
add_executable(websocket_client src/websocket_client.cpp)
target_link_libraries(websocket_client 
    order_book
    Boost::beast 
    Boost::asio 
    Boost::system 
    Boost::thread
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)
target_include_directories(websocket_client PRIVATE ${OPENSSL_INCLUDE_DIR})

# Create FIX test executable
add_executable(fix_test src/fix_test.cpp)
target_link_libraries(fix_test fix_parser order_book)

# Create SOR test executable
add_executable(sor_test src/sor_test.cpp)
target_link_libraries(sor_test smart_order_router order_book)

# Create Market Maker test executable
add_executable(mm_test src/mm_test.cpp)
target_link_libraries(mm_test market_maker smart_order_router order_book)

# Create profiling executable
add_executable(profile_matching src/profile_matching.cpp)
target_link_libraries(profile_matching order_book)

# Create detailed performance analysis executable
add_executable(detailed_perf src/detailed_perf.cpp)
target_link_libraries(detailed_perf order_book)