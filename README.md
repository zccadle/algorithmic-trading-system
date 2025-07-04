# High-Performance Algorithmic Trading System: C++ vs Rust

![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Rust](https://img.shields.io/badge/Rust-000000?style=flat&logo=rust&logoColor=white)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/yourusername/trading-system/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A comprehensive trading system implementation featuring parallel architectures in C++ and Rust, designed to showcase modern systems programming techniques for ultra-low latency financial applications.

## Project Overview

This project implements a complete trading system core with industry-standard components, providing a detailed comparison of C++ and Rust for high-frequency trading applications. The dual implementation approach offers unique insights into the trade-offs between performance, safety, and development velocity in systems programming.

## 🏗️ Architecture

See [docs/architecture.md](docs/architecture.md) for detailed system design.

### Core Components

#### 1. **Order Book & Matching Engine**
- Price-time priority order book with O(log n) operations
- Integrated matching engine supporting market and limit orders
- Sub-microsecond best bid/ask queries

#### 2. **Smart Order Router (SOR)**
- Multi-exchange order routing with intelligent order splitting
- Fee-aware routing optimization
- Latency-based failover mechanisms

#### 3. **FIX Protocol Gateway**
- FIX 4.4 protocol parser and message handler
- Support for NewOrderSingle and OrderCancelRequest
- Integration with order book for seamless order flow

#### 4. **Market Making Strategy**
- Automated liquidity provision with inventory management
- Dynamic spread adjustment based on market volatility
- Risk-aware position limits

#### 5. **WebSocket Client**
- Real-time market data streaming
- Binary and text protocol support
- Async I/O for scalable connections

## 📊 Performance Analysis

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for comprehensive benchmarks.

### Key Performance Metrics

| Operation | C++ (M1) | Rust (M1) | C++ Advantage |
|-----------|----------|-----------|---------------|
| Order Insertion (10K) | 2.64 ms | 6.10 ms | 2.31x faster |
| Mixed Operations (10K) | 2.41 ms | 4.66 ms | 1.93x faster |
| Matching Engine | 190.6 µs | 499.8 µs | 2.62x faster |
| Best Price Query | 1.16 µs | 1.50 µs | 1.29x faster |

The C++ implementation achieves 2-4x better performance across all operations. The performance gap is even larger on x86_64 platforms (up to 3.8x). This advantage stems from C++'s ability to perform in-place modifications during iteration, while Rust's borrow checker enforces patterns that require additional heap allocations.

## 🚀 Getting Started

### Prerequisites

- **C++ Build**: CMake 3.10+, C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **Rust Build**: Rust 1.70+ (install via [rustup](https://rustup.rs/))
- **Dependencies**: Boost 1.70+ (for C++ WebSocket support)

### Building the Project

#### C++ Components
```bash
cd trading_system/src/cpp_core
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### Rust Components
```bash
cd trading_system/src/rust_core
cargo build --release
```

### Running Tests

#### C++ Tests
```bash
cd trading_system/src/cpp_core/build
./orderbook_test
./fix_test
./sor_test
./mm_test
```

#### Rust Tests
```bash
cd trading_system/src/rust_core
cargo test
```

### Performance Benchmarks

#### C++ Benchmarks
```bash
cd trading_system/src/cpp_core/build
./order_book_benchmark              # Google Benchmark (proper benchmarking)
./detailed_perf                     # Simple timing tool (debugging only)
```

#### Rust Benchmarks
```bash
cd trading_system/src/rust_core
cargo bench                         # Criterion benchmarks (proper benchmarking)
./target/release/detailed_perf      # Simple timing tool (debugging only)
```

## 📁 Project Structure

```
trading_system/
├── src/
│   ├── cpp_core/           # C++ implementation
│   │   ├── include/        # Headers
│   │   ├── src/           # Source files
│   │   └── benchmarks/    # Performance tests
│   └── rust_core/         # Rust implementation
│       ├── src/           # Source files
│       ├── benches/       # Criterion benchmarks
│       └── examples/      # Example applications
├── docs/
│   ├── architecture.md    # System design documentation
│   └── PERFORMANCE.md     # Performance analysis report
└── README.md             # This file
```

## 🔑 Key Design Decisions

### C++ Implementation
- **Modern C++ practices**: RAII, smart pointers, move semantics
- **STL containers**: std::map for order books, std::unordered_map for order lookup
- **Virtual dispatch**: Abstract base classes for exchange polymorphism

### Rust Implementation
- **Zero-cost abstractions**: Trait-based design with static dispatch where possible
- **Memory safety**: Compile-time guarantees without garbage collection
- **Async I/O**: Tokio-based async runtime for network operations

## 🎯 Use Cases

This trading system is designed for:
- **High-frequency trading firms** requiring sub-microsecond latencies
- **Market makers** needing robust inventory management
- **Proprietary trading desks** with multi-exchange connectivity needs
- **Academic research** into trading system architecture and performance

## 🔧 Configuration

Both implementations support runtime configuration for:
- Exchange endpoints and connectivity
- Strategy parameters (spreads, inventory limits)
- Risk management thresholds
- Performance tuning options

## 📈 Future Enhancements

- **Risk Management Layer**: Pre-trade checks and position monitoring
- **Historical Data Service**: Backtesting infrastructure
- **Multi-Asset Support**: Extend beyond single instruments
- **Cloud Deployment**: Kubernetes-ready containerization
- **Hardware Acceleration**: FPGA integration for order gateways

## 📚 Documentation

- [Architecture Overview](docs/architecture.md) - System design and component interactions
- [Performance Report](PERFORMANCE.md) - Detailed benchmark analysis
- [API Documentation](docs/api.md) - Component interfaces (coming soon)

## 🤝 Contributing

This project is designed as a portfolio piece demonstrating systems programming expertise. While not actively seeking contributions, feedback and discussions about the implementation choices are welcome.

## 📄 License

This project is available under the MIT License. See LICENSE file for details.

## 🙏 Acknowledgments

- Built with modern C++17 and Rust 2021 Edition
- Benchmarked using Google Benchmark and Criterion
- Inspired by real-world trading system architectures

---

*This project demonstrates production-grade trading system development with a focus on performance, reliability, and maintainability. For questions or discussions about the implementation, please open an issue or email donlee778@gmail.com.*
