# Trading System Architecture

## 1. High-Level Overview

This trading system implements a sophisticated hybrid architecture combining Python for strategy development, C++/Rust for high-performance execution, and a comprehensive analytics layer. The system features both traditional order-book simulation and modern signal-based backtesting approaches.

## 2. Core Components

### Order Book & Matching Engine
The foundational data structure that maintains price-time priority for all orders, implementing a dual-sided book with configurable matching algorithms for both market and limit orders.

### Smart Order Router (SOR)
An intelligent routing layer that distributes orders across multiple exchanges based on liquidity, fees, and latency, with support for order splitting and failover scenarios.

### FIX Protocol Gateway
A Financial Information eXchange (FIX) 4.4 protocol parser and handler that translates industry-standard messages into internal order representations for seamless integration with traditional trading infrastructure.

### WebSocket Client
Real-time market data and order management client supporting both text and binary protocols, enabling connectivity to modern cryptocurrency exchanges and streaming data feeds.

### Market Making Strategy
An automated trading strategy that provides liquidity by continuously quoting bid and ask prices, with inventory management and dynamic spread adjustment based on market volatility.

## 3. C++ Design Philosophy

### Style: Classic Object-Oriented Programming
The C++ implementation embraces traditional OOP patterns with abstract base classes (e.g., `IExchange`) defining interfaces for polymorphic behavior. Virtual functions enable runtime dispatch, allowing strategies to work with different exchange implementations seamlessly.

### Memory Management: Modern C++ with RAII
Leverages C++14/17 features including `std::unique_ptr` and `std::shared_ptr` for automatic memory management, ensuring deterministic destruction and eliminating manual memory management errors while maintaining performance.

### Dependencies: Industry-Standard Libraries
Built on proven libraries including Boost.Beast for WebSocket networking, nlohmann/json for JSON parsing, and standard library containers (`std::map`, `std::unordered_map`) for core data structures, prioritizing stability and widespread adoption.

## 4. Rust Design Philosophy

### Style: Trait-Based Composition
Rust's implementation uses traits (e.g., `trait Exchange`) to define behavioral contracts, with trait objects (`Box<dyn Exchange>`) providing dynamic dispatch without classical inheritance, emphasizing composition over inheritance patterns.

### Memory & Concurrency Safety: Compile-Time Guarantees
The ownership model and borrow checker ensure memory safety without garbage collection, preventing data races at compile time. Lifetimes explicitly track data dependencies, making concurrent access patterns safe by construction.

### Dependencies: Modern Async Ecosystem
Utilizes tokio for asynchronous runtime, futures-util for stream processing, serde for serialization, and tokio-tungstenite for WebSocket connectivity, embracing Rust's async/await paradigm for scalable concurrent operations.

## 5. Key Architectural Trade-Off: A Tale of Two Polymorphisms

The Smart Order Router implementation perfectly illustrates the fundamental design differences between C++ and Rust approaches to polymorphism:

**C++ Virtual Functions:**
```cpp
class IExchange {
    virtual OrderBook* get_order_book() = 0;
    virtual void send_order(...) = 0;
};
```
Runtime polymorphism through vtable dispatch, offering maximum flexibility at the cost of potential virtual call overhead.

**Rust Trait Objects:**
```rust
trait Exchange: Send + Sync {
    fn get_order_book(&self) -> &OrderBook;
    fn send_order(&self, ...) -> Result<(), String>;
}
```
Trait objects provide similar runtime polymorphism but with compile-time safety guarantees, preventing common pitfalls like object slicing while maintaining zero-cost abstractions where possible.

Both approaches achieve the same architectural goal—decoupling strategies from exchange implementations—but represent different language philosophies on the balance between safety, performance, and flexibility. The C++ approach prioritizes familiar patterns and maximum runtime flexibility, while Rust enforces correctness at compile time, preventing entire classes of bugs at the cost of additional implementation complexity for certain patterns (as evidenced in the matching engine's performance characteristics).

## 6. Data Flow Architecture

### Order Lifecycle
1. **Order Entry**: FIX messages or WebSocket commands are parsed into internal `Order` structures
2. **Routing Decision**: The SOR analyzes available liquidity across exchanges and determines optimal routing
3. **Order Placement**: Orders are placed into the appropriate exchange's order book
4. **Matching**: The matching engine processes orders based on price-time priority
5. **Execution Reporting**: Fills are reported back through the originating protocol

### Market Data Flow
- Real-time market data streams via WebSocket connections
- Order book updates trigger strategy recalculations
- Market makers adjust quotes based on inventory and market conditions

## 7. Performance Considerations

### Latency Optimization
- **Zero-copy parsing**: Both implementations minimize data copying during message parsing
- **Pre-allocated memory pools**: Reduce allocation overhead in hot paths
- **Lock-free data structures**: Where applicable, particularly in the Rust implementation

### Scalability Design
- **Exchange abstraction**: Allows horizontal scaling by adding new exchange connections
- **Stateless SOR**: Routing decisions don't require persistent state, enabling parallel processing
- **Independent strategy instances**: Market makers can run multiple instances with different parameters

## 8. Testing Architecture

### Unit Testing
- Comprehensive test coverage for each component
- Mock exchange implementations for isolated testing
- Property-based testing for order book invariants

### Integration Testing
- End-to-end order flow validation
- Multi-exchange scenario testing
- Performance regression tests

### Benchmarking
- Microbenchmarks for critical operations (order insertion, matching)
- Latency percentile analysis (P50, P95, P99)
- Memory allocation profiling

## 9. Deployment Considerations

### Build System
- **C++**: CMake-based build with support for multiple compilers (GCC, Clang, MSVC)
- **Rust**: Cargo with optimized release profiles for production deployment

### Configuration
- Runtime-configurable exchange endpoints
- Adjustable strategy parameters without recompilation
- Environment-based configuration for different deployment scenarios

### Monitoring
- Performance metrics exposed for external monitoring systems
- Order flow audit trail for compliance
- Real-time strategy performance tracking

## 10. Backtesting Infrastructure

### Two Backtesting Approaches

#### 1. Full Order-Book Simulation (`backtest_engine.cpp`)
- Complete market microstructure simulation
- Includes market maker and smart order router
- Realistic order matching with configurable latency
- Suitable for testing market-making strategies

#### 2. Signal-Based Execution (`signal_backtest_engine.cpp`)
- Streamlined execution based on position signals (-1, 0, 1)
- Direct integration with Python strategy framework
- Lower computational overhead for strategy research
- Realistic slippage and market impact modeling

### Python Integration Architecture

The system employs a clean separation of concerns:

```
Python Strategy Layer              C++ Execution Core
┌─────────────────┐               ┌──────────────────┐
│ Strategy.calc() │─── Signals ──→│ SignalBacktest   │
│ - MA Cross      │               │ Engine           │
│ - RSI Mean Rev  │               │ - Execute trades │
└─────────────────┘               │ - Track P&L     │
        ↑                         │ - Apply costs   │
        │                         └──────────────────┘
        │                                  │
    Market Data                        Trade Logs
        │                                  │
        └──────────────┬──────────────────┘
                       ↓
              ┌─────────────────┐
              │ Python Analytics │
              │ - Metrics calc   │
              │ - Visualization  │
              │ - Dashboard      │
              └─────────────────┘
```

## 11. Market Data Pipeline

### Data Flow
1. **Download**: yfinance integration for historical data
2. **Storage**: Parquet format for efficient columnar storage
3. **Loading**: DataLoader class with automatic format detection
4. **Processing**: Pandas DataFrames for strategy calculations

### Interactive Dashboard
- Built with Streamlit for web-based access
- Real-time strategy parameter tuning
- Performance visualization with Plotly
- Trade analysis and export functionality

## 12. Performance-Driven Design Decisions

### Why C++ for Execution?
Based on rigorous benchmarking (see [PERFORMANCE.md](PERFORMANCE.md)):
- 2-4x faster than Rust for order operations
- In-place memory modifications vs. Rust's heap allocations
- Critical for sub-microsecond execution requirements

### Why Python for Strategies?
- Rapid prototyping with NumPy/Pandas
- Rich ecosystem of technical indicators
- Easy integration with ML libraries
- Clean separation from execution layer

## 13. Future Architecture Enhancements

### In Progress
- Extended Rust backtesting engine with signal support
- Additional trading strategies (pairs trading, stat arb)
- Real-time data feed integration

### Planned
- Machine learning strategy framework
- Cloud deployment with monitoring
- Multi-asset portfolio optimization
- Risk management layer