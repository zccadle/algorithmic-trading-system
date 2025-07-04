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

### 3. Market Data Replay Tool

A command-line application that simulates a live market feed by reading a sequence of orders from a CSV file and processing them through the matching engine. This allows for end-to-end system performance testing.

## Performance Benchmark Analysis

To provide a quantitative comparison, a benchmarking suite was developed using Google Benchmark for C++ and Criterion for Rust. Both implementations were tasked with identical workloads on the same machine.

### Micro-Benchmark Results (Algorithmic Performance)

The following table summarizes the mean time taken for core in-memory operations after the implementation of the matching engine. This test focuses on pure, CPU-bound algorithmic efficiency.

| Benchmark Scenario | Rust Time (µs) | C++ Time (µs) | Winner | Performance Difference |
|-------------------|----------------|---------------|--------|----------------------|
| Matching Engine | ~401 µs | ~105 µs | C++ | C++ is ~3.8x faster |
| Add 10k Orders | ~5,433 µs | ~1,588 µs | C++ | C++ is ~3.4x faster |
| Mixed Operations | ~4,153 µs | ~1,539 µs | C++ | C++ is ~2.7x faster |
| Best Price Queries | ~1.72 µs | ~1.16 µs | C++ | C++ is ~1.5x faster |

*(Tests run on Apple M1 Pro, results are indicative)*

### End-to-End Replay Performance (System Performance)

The replay tool measures the total time taken to process a sequence of 15 orders from a CSV file, including file I/O, data parsing, and execution through the matching engine.

| Replay Tool | Total Processing Time |
|-------------|--------------------|
| C++ | 283 µs |
| Rust | 83 µs |

## Final Project Analysis

This project reveals a crucial distinction between algorithmic performance and overall system performance.

**CPU-Bound Performance**: In the isolated micro-benchmarks, the C++ implementation was significantly faster. This is primarily due to C++'s ability to perform direct, in-place manipulation of data structures within complex loops. The Rust version, while guaranteeing memory safety, required a safer but less direct implementation pattern (collecting keys before iteration) to satisfy the borrow checker, which introduced measurable overhead.

**System-Level Performance**: In the end-to-end replay test, which includes file I/O and data parsing, the Rust implementation was ~3.4x faster. This highlights the strength and efficiency of Rust's modern ecosystem, particularly its highly optimized libraries for common tasks like CSV parsing and file handling.

**Conclusion**: Both C++ and Rust are elite languages for high-performance systems. This project demonstrates that C++ can offer a more direct path to raw performance in complex, CPU-bound algorithms, while Rust's safety guarantees and modern ecosystem can lead to extremely robust and performant systems when considering the entire application lifecycle. The choice between them depends on the specific trade-offs a team is willing to make between raw algorithmic speed, provable safety, and ecosystem maturity.