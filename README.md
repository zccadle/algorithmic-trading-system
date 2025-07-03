# High-Performance Algorithmic Trading System

This project is an exploration into building the core components of a high-performance algorithmic trading system. It features parallel implementations in both C++ and Rust to analyze and compare their performance, safety, and design patterns in a low-latency context.

## Core Components

The system is architected around several key components.

### 1. In-Memory Order Book

A critical data structure that maintains a live, real-time view of all open buy and sell orders for a given instrument.

- **C++ Implementation (`cpp_core`)**: A modern C++17 implementation using `std::map` for sorted price levels and `std::unique_ptr` for memory management.

- **Rust Implementation (`rust_core`)**: An idiomatic Rust implementation using `BTreeMap` for sorted levels and leveraging Rust's ownership model for memory safety.

### 2. Matching Engine

The logic that consumes orders from the book and generates trades when a buy order's price crosses a sell order's price. This logic is integrated directly into the `add_order` method of the `OrderBook`.

## Performance Benchmark Analysis

To provide a quantitative comparison, a benchmarking suite was developed using Google Benchmark for C++ and Criterion for Rust. Both implementations were tasked with identical workloads on the same machine.

### Benchmark Results

The following table summarizes the mean time taken for each core operation after the implementation of the matching engine.

| Benchmark Scenario | Rust Time (µs) | C++ Time (µs) | Winner | Performance Difference |
|-------------------|----------------|---------------|--------|----------------------|
| Matching Engine | ~401 µs | ~105 µs | C++ | C++ is ~3.8x faster |
| Add 10k Orders | ~5,433 µs | ~1,588 µs | C++ | C++ is ~3.4x faster |
| Mixed Operations | ~4,153 µs | ~1,539 µs | C++ | C++ is ~2.7x faster |
| Best Price Queries | ~1.72 µs | ~1.16 µs | C++ | C++ is ~1.5x faster |

*(Tests run on Apple M1 Pro, results are indicative)*

### Analysis

With the addition of the complex, stateful logic required for the matching engine, the performance characteristics shifted significantly in favor of the C++ implementation.

**Complex Logic Performance**: The C++ version demonstrated superior performance in all tests, especially the matching-intensive one. This is likely due to several factors:

- **Direct Memory/Iterator Manipulation**: C++ allows for more direct, albeit less safe, manipulation of map iterators within loops, which can be highly optimized by the compiler.

- **Borrow Checker Overhead**: The Rust implementation, while guaranteeing memory safety, requires more careful handling of ownership and borrowing. To satisfy the borrow checker in the matching loop, it was necessary to collect price keys into a temporary `Vec` before iterating, which introduces a degree of overhead not present in the C++ version.

**The Safety vs. Performance Trade-off**: This project clearly demonstrates the core trade-off between modern C++ and Rust. Rust provides compile-time guarantees against entire classes of bugs (like dangling pointers or data races), but achieving the absolute peak performance for complex algorithms can sometimes require more intricate code to work with the borrow checker. C++ provides raw power and control, which can yield faster code in complex scenarios, at the cost of requiring more discipline from the developer to ensure safety.