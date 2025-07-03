# High-Performance Algorithmic Trading System

This project is an exploration into building the core components of a high-performance algorithmic trading system. It features parallel implementations in both C++ and Rust to analyze and compare their performance, safety, and design patterns in a low-latency context.

## Core Components

The system is architected around several key components, with the initial focus on the in-memory order book.

### 1. In-Memory Order Book

A critical data structure that maintains a live, real-time view of all open buy and sell orders for a given instrument.

- **C++ Implementation (`cpp_core`)**: A modern C++17 implementation using `std::map` for sorted price levels and `std::unique_ptr` for memory management.

- **Rust Implementation (`rust_core`)**: An idiomatic Rust implementation using `BTreeMap` for sorted levels and leveraging Rust's ownership model for memory safety.

## Performance Benchmark: C++ vs. Rust Order Book

To provide a quantitative comparison, a benchmarking suite was developed using Google Benchmark for C++ and Criterion for Rust. Both implementations were tasked with identical workloads on the same machine.

### Benchmark Results

The following table summarizes the mean time taken for each core operation:

| Benchmark Scenario | Rust Time (µs) | C++ Time (µs) | Winner | Performance Difference |
|-------------------|----------------|---------------|--------|----------------------|
| Add 10k Orders | ~700 µs | ~1,320 µs | Rust | Rust is ~1.88x faster |
| Mixed 10k Operations | ~800 µs | ~1,537 µs | Rust | Rust is ~1.92x faster |
| Best Price Queries | ~1.73 µs | ~1.16 µs | C++ | C++ is ~1.49x faster |

*(Tests run on Apple M1 Pro, results are indicative)*

### Analysis

- **Modifications (Add/Cancel)**: The Rust implementation demonstrated significantly better performance for workloads involving additions and cancellations. This is primarily attributed to the use of fixed-point integer representation for price keys in the `BTreeMap`, allowing for faster comparisons compared to the `double` keys used in the C++ `std::map`.

- **Lookups**: The C++ implementation showed a slight advantage in simple, repeated lookups of the best price, likely due to lower function call overhead and the absence of a need to convert the price key back from an integer to a float.

These results highlight a key trade-off: while both languages are capable of high performance, specific implementation choices (like data representation) can have a dramatic impact on performance characteristics for different workloads.