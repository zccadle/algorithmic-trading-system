#!/bin/bash

# Trading System Demo Script
# This script demonstrates the key features of both C++ and Rust implementations

echo "=== High-Performance Trading System Demo ==="
echo

# Check if we're in the right directory
if [ ! -f "README.md" ]; then
    echo "Error: Please run this script from the trading_system root directory"
    exit 1
fi

echo "1. Building C++ components..."
cd src/cpp_core
mkdir -p build && cd build
cmake .. > /dev/null 2>&1
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "   ✓ C++ build complete"
else
    echo "   ✗ C++ build failed"
    exit 1
fi
cd ../../..

echo
echo "2. Building Rust components..."
cd src/rust_core
cargo build --release --quiet
if [ $? -eq 0 ]; then
    echo "   ✓ Rust build complete"
else
    echo "   ✗ Rust build failed"
    exit 1
fi
cd ../..

echo
echo "3. Running Order Book tests..."
echo "   C++ Order Book test:"
./src/cpp_core/build/orderbook_test
echo
echo "   Rust Order Book test:"
cd src/rust_core && cargo test --quiet order_book && cd ../..

echo
echo "4. Running FIX Protocol test (C++)..."
./src/cpp_core/build/fix_test

echo
echo "5. Running Smart Order Router test..."
echo "   C++ SOR test:"
./src/cpp_core/build/sor_test
echo
echo "   Rust SOR test:"
cd src/rust_core && cargo test --quiet smart_order_router && cd ../..

echo
echo "6. Running Market Maker test..."
echo "   C++ Market Maker:"
./src/cpp_core/build/mm_test
echo
echo "   Rust Market Maker:"
cd src/rust_core && cargo test --quiet market_maker && cd ../..

echo
echo "7. Performance Comparison (sample)..."
echo "   Running C++ benchmark..."
./src/cpp_core/build/matching_benchmark --benchmark_min_time=1
echo
echo "   Running Rust benchmark..."
cd src/rust_core && cargo bench --bench order_book_benchmark && cd ../..

echo
echo "=== Demo Complete ==="
echo "See docs/PERFORMANCE.md for detailed benchmark analysis"
echo "See docs/architecture.md for system design details"