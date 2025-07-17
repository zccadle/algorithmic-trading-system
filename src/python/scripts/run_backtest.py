#!/usr/bin/env python3
"""
Main script to run backtests using the trading framework.

This script demonstrates the complete workflow:
1. Load market data
2. Initialize a trading strategy
3. Generate signals
4. Run backtest through C++/Rust engine
5. Analyze performance
6. Generate performance tearsheet
"""

import argparse
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.append(str(Path(__file__).parent.parent))

import pandas as pd

from trading_framework.analytics.metrics import PerformanceAnalyzer
from trading_framework.analytics.trade_metrics import TradeAnalyzer
from trading_framework.analytics.visualization import create_tearsheet
from trading_framework.backtesting.engine import SignalBacktestEngine
from trading_framework.strategies.simple_ma_cross import SimpleMACrossStrategy
from trading_framework.strategies.mean_reversion_rsi import MeanReversionRSIStrategy
from trading_framework.core.data_loader import DataLoader


def main():
    """Main execution function."""
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description='Run trading strategy backtest'
    )
    parser.add_argument(
        '--symbol',
        default=None,
        help='Symbol to backtest (e.g., SPY). If not provided, uses sample data'
    )
    parser.add_argument(
        '--strategy',
        default='ma_cross',
        choices=['ma_cross', 'rsi'],
        help='Strategy to run'
    )
    parser.add_argument(
        '--capital',
        type=float,
        default=100000.0,
        help='Initial capital'
    )
    parser.add_argument(
        '--short-window',
        type=int,
        default=10,
        help='Short MA window for MA Cross strategy'
    )
    parser.add_argument(
        '--long-window',
        type=int,
        default=30,
        help='Long MA window for MA Cross strategy'
    )
    parser.add_argument(
        '--rsi-period',
        type=int,
        default=14,
        help='RSI period for RSI strategy'
    )
    parser.add_argument(
        '--rsi-lower',
        type=float,
        default=30.0,
        help='RSI lower threshold for buy signals'
    )
    parser.add_argument(
        '--rsi-upper',
        type=float,
        default=70.0,
        help='RSI upper threshold for sell signals'
    )
    
    args = parser.parse_args()
    
    # Configuration
    project_root = Path(__file__).parent.parent.parent.parent
    
    # Engine configuration - use the signal-based engine
    engine_path = project_root / "src" / "cpp_core" / "build" / "signal_backtest_engine"
    if not engine_path.exists():
        print(f"Error: Could not find signal backtest engine at {engine_path}")
        print("Please compile the C++ signal backtest engine first:")
        print("  cd src/cpp_core")
        print("  mkdir build && cd build")
        print("  cmake .. && make signal_backtest_engine")
        return 1
    
    print("=== Trading Framework Backtest ===\n")
    
    # Load market data
    print("Loading market data...")
    loader = DataLoader()
    
    if args.symbol:
        print(f"Loading data for {args.symbol}...")
        market_data = loader.load(args.symbol)
    else:
        print("Using sample data...")
        market_data = loader.load_sample_data(days=252)
    
    print(f"Loaded {len(market_data)} days of market data")
    print(f"Date range: {market_data.index[0]} to {market_data.index[-1]}")
    
    # Initialize strategy
    print("\nInitializing strategy...")
    if args.strategy == 'ma_cross':
        strategy = SimpleMACrossStrategy(
            short_window=args.short_window, 
            long_window=args.long_window
        )
        print(f"Strategy: {strategy.name} (Short: {strategy.short_window}, Long: {strategy.long_window})")
    elif args.strategy == 'rsi':
        strategy = MeanReversionRSIStrategy(
            rsi_period=args.rsi_period,
            lower_threshold=args.rsi_lower,
            upper_threshold=args.rsi_upper
        )
        print(f"Strategy: {strategy}")
    
    # Run backtest with correct architecture
    print("\nRunning backtest (Python signals → C++ execution)...")
    try:
        engine = SignalBacktestEngine(str(engine_path), engine_type="cpp", verbose=True)
        results = engine.run(
            market_data=market_data,
            strategy=strategy,
            initial_capital=args.capital,
            position_size=0.1
        )
        
        trades = results['trades']
        signals = results['signals']
        
        print(f"\nBacktest complete:")
        print(f"  - Signals generated: {len(signals)}")
        print(f"  - Trades executed: {len(trades)}")
        
    except Exception as e:
        print(f"Error running backtest: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    # Analyze trades using TradeAnalyzer
    print("\nAnalyzing executed trades...")
    try:
        trade_analyzer = TradeAnalyzer(initial_capital=args.capital)
        analysis = trade_analyzer.analyze_trades(
            trades=trades,
            market_data=market_data
        )
        
        portfolio = analysis['portfolio']
        
        # Calculate performance metrics
        metrics = trade_analyzer.calculate_performance_metrics(portfolio)
        
        # Print trade summary
        print("\n=== Trade Execution Summary ===")
        print(analysis['trade_summary'].to_string())
        
        # Print performance metrics
        print("\n=== Performance Metrics ===")
        for metric, value in metrics.items():
            if isinstance(value, float):
                if 'return' in metric or 'ratio' in metric:
                    print(f"{metric.replace('_', ' ').title()}: {value:.4f}")
                else:
                    print(f"{metric.replace('_', ' ').title()}: {value:.2%}")
            else:
                print(f"{metric.replace('_', ' ').title()}: {value}")
        
    except Exception as e:
        print(f"Error analyzing performance: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    # Generate tearsheet
    print("\nGenerating performance tearsheet...")
    try:
        tearsheet_path = project_root / "performance_tearsheet.png"
        create_tearsheet(
            portfolio=portfolio,
            trades=trades,
            metrics=metrics,
            signals=signals,
            title=f"{strategy.name} Strategy Performance",
            save_path=str(tearsheet_path)
        )
        
        print(f"\nSuccess! Performance tearsheet saved to {tearsheet_path}")
        
    except Exception as e:
        print(f"Error creating tearsheet: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    print("\n✓ Architecture validation complete!")
    print("  - Python generated trading signals")
    print("  - C++ engine simulated execution with realistic slippage/fees")
    print("  - Python analyzed the actual executed trades")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())