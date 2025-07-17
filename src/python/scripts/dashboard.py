#!/usr/bin/env python3
"""
Interactive trading dashboard using Streamlit.

This dashboard allows users to:
- Select and configure trading strategies
- Run backtests with different parameters
- View performance metrics and visualizations
- Download results
"""

import sys
import subprocess
from pathlib import Path
from datetime import datetime, timedelta
import json

# Streamlit imports
try:
    import streamlit as st
    import plotly.graph_objs as go
    import plotly.express as px
except ImportError:
    print("ERROR: Streamlit not installed!")
    print("Please install with: pip install streamlit plotly")
    sys.exit(1)

# Add parent directory to path
sys.path.append(str(Path(__file__).parent.parent))

import pandas as pd
from trading_framework.core.data_loader import DataLoader
from trading_framework.backtesting.engine import SignalBacktestEngine
from trading_framework.analytics.trade_metrics import TradeAnalyzer
from trading_framework.strategies.simple_ma_cross import SimpleMACrossStrategy
from trading_framework.strategies.mean_reversion_rsi import MeanReversionRSIStrategy


# Page configuration
st.set_page_config(
    page_title="Algorithmic Trading Dashboard",
    page_icon="📈",
    layout="wide"
)

# Initialize session state
if 'backtest_results' not in st.session_state:
    st.session_state.backtest_results = None
if 'trades' not in st.session_state:
    st.session_state.trades = None
if 'portfolio' not in st.session_state:
    st.session_state.portfolio = None


def run_backtest(symbol, strategy_name, strategy_params, capital):
    """Run backtest with selected parameters."""
    # Get project root - dashboard.py is in src/python/scripts/
    project_root = Path(__file__).parent.parent.parent.parent  # Goes up to trading_system/
    engine_path = project_root / "src" / "cpp_core" / "build" / "signal_backtest_engine"
    
    if not engine_path.exists():
        st.error(f"C++ engine not found at {engine_path}")
        st.error(f"Current script location: {Path(__file__).resolve()}")
        st.error(f"Calculated project root: {project_root}")
        st.error("Please build the engine first!")
        return None
    
    # Load data
    loader = DataLoader()
    
    if symbol and symbol != "Sample Data":
        try:
            market_data = loader.load(symbol)
        except Exception as e:
            st.error(f"Error loading data for {symbol}: {e}")
            return None
    else:
        market_data = loader.load_sample_data(days=252)
    
    # Initialize strategy
    if strategy_name == "MA Crossover":
        strategy = SimpleMACrossStrategy(
            short_window=strategy_params['short_window'],
            long_window=strategy_params['long_window']
        )
    elif strategy_name == "Mean Reversion (RSI)":
        strategy = MeanReversionRSIStrategy(
            rsi_period=strategy_params['rsi_period'],
            lower_threshold=strategy_params['lower_threshold'],
            upper_threshold=strategy_params['upper_threshold']
        )
    
    # Run backtest
    engine = SignalBacktestEngine(str(engine_path), verbose=False)
    
    try:
        results = engine.run(
            market_data=market_data,
            strategy=strategy,
            initial_capital=capital,
            position_size=0.1
        )
        
        # Analyze trades
        analyzer = TradeAnalyzer(initial_capital=capital)
        analysis = analyzer.analyze_trades(
            trades=results['trades'],
            market_data=market_data
        )
        
        # Calculate metrics
        metrics = analyzer.calculate_performance_metrics(analysis['portfolio'])
        
        return {
            'trades': results['trades'],
            'signals': results['signals'],
            'portfolio': analysis['portfolio'],
            'metrics': metrics,
            'summary': analysis['trade_summary']
        }
        
    except Exception as e:
        st.error(f"Backtest error: {e}")
        return None


def plot_performance(portfolio):
    """Create performance chart."""
    fig = go.Figure()
    
    # Cumulative returns
    fig.add_trace(go.Scatter(
        x=portfolio.index,
        y=(portfolio['cumulative_returns'] * 100),
        mode='lines',
        name='Cumulative Returns (%)',
        line=dict(color='blue', width=2)
    ))
    
    # Add zero line
    fig.add_hline(y=0, line_dash="dash", line_color="gray")
    
    fig.update_layout(
        title="Portfolio Performance",
        xaxis_title="Date",
        yaxis_title="Return (%)",
        height=500,
        template="plotly_white"
    )
    
    return fig


def plot_drawdown(portfolio):
    """Create drawdown chart."""
    # Calculate drawdown
    cumulative = (1 + portfolio['returns']).cumprod()
    running_max = cumulative.expanding().max()
    drawdown = ((cumulative - running_max) / running_max) * 100
    
    fig = go.Figure()
    
    fig.add_trace(go.Scatter(
        x=portfolio.index,
        y=drawdown,
        mode='lines',
        fill='tozeroy',
        name='Drawdown (%)',
        line=dict(color='red'),
        fillcolor='rgba(255,0,0,0.3)'
    ))
    
    fig.update_layout(
        title="Drawdown Analysis",
        xaxis_title="Date",
        yaxis_title="Drawdown (%)",
        height=400,
        template="plotly_white"
    )
    
    return fig


# Main UI
st.title("🚀 Algorithmic Trading Dashboard")
st.markdown("**Signal-Based Architecture**: Python Strategies → C++ Execution → Performance Analytics")

# Sidebar configuration
with st.sidebar:
    st.header("Configuration")
    
    # Data source
    st.subheader("📊 Data Source")
    data_option = st.selectbox(
        "Select data source",
        ["Sample Data", "SPY", "AAPL", "MSFT", "Custom Symbol"]
    )
    
    if data_option == "Custom Symbol":
        symbol = st.text_input("Enter symbol", value="QQQ")
    else:
        symbol = data_option
    
    # Strategy selection
    st.subheader("📈 Strategy")
    strategy_name = st.selectbox(
        "Select strategy",
        ["MA Crossover", "Mean Reversion (RSI)"]
    )
    
    # Strategy parameters
    st.subheader("⚙️ Strategy Parameters")
    
    if strategy_name == "MA Crossover":
        short_window = st.slider("Short MA Window", 5, 50, 10)
        long_window = st.slider("Long MA Window", 20, 200, 30)
        strategy_params = {
            'short_window': short_window,
            'long_window': long_window
        }
    else:  # RSI
        rsi_period = st.slider("RSI Period", 5, 30, 14)
        lower_threshold = st.slider("Lower Threshold (Buy)", 10, 40, 30)
        upper_threshold = st.slider("Upper Threshold (Sell)", 60, 90, 70)
        strategy_params = {
            'rsi_period': rsi_period,
            'lower_threshold': lower_threshold,
            'upper_threshold': upper_threshold
        }
    
    # Capital
    st.subheader("💰 Initial Capital")
    capital = st.number_input(
        "Amount ($)",
        min_value=10000,
        max_value=10000000,
        value=100000,
        step=10000
    )
    
    # Run button
    st.markdown("---")
    run_button = st.button("🏃 Run Backtest", type="primary", use_container_width=True)

# Main content area
if run_button:
    with st.spinner("Running backtest..."):
        results = run_backtest(symbol, strategy_name, strategy_params, capital)
        
        if results:
            st.session_state.backtest_results = results
            st.session_state.trades = results['trades']
            st.session_state.portfolio = results['portfolio']
            st.success("Backtest completed successfully!")

# Display results
if st.session_state.backtest_results:
    results = st.session_state.backtest_results
    
    # Metrics row
    col1, col2, col3, col4 = st.columns(4)
    
    with col1:
        total_return = results['metrics']['total_return'] * 100
        st.metric("Total Return", f"{total_return:.2f}%")
    
    with col2:
        sharpe = results['metrics']['sharpe_ratio']
        st.metric("Sharpe Ratio", f"{sharpe:.2f}")
    
    with col3:
        max_dd = results['metrics']['max_drawdown'] * 100
        st.metric("Max Drawdown", f"{max_dd:.2f}%")
    
    with col4:
        n_trades = len(results['trades'])
        st.metric("Total Trades", n_trades)
    
    # Charts
    st.markdown("---")
    
    # Performance chart
    if not results['portfolio'].empty:
        st.plotly_chart(plot_performance(results['portfolio']), use_container_width=True)
        
        # Drawdown chart
        st.plotly_chart(plot_drawdown(results['portfolio']), use_container_width=True)
    
    # Trade summary
    st.markdown("---")
    st.subheader("📋 Trade Summary")
    
    col1, col2 = st.columns(2)
    
    with col1:
        # Format the summary for display
        summary_display = results['summary'].copy()
        # Format numeric values for display
        for idx in summary_display.index:
            if idx in ['Total Fees', 'Total Slippage', 'Avg Slippage per Trade', 'Total P&L']:
                summary_display.loc[idx, 'Value'] = f"${summary_display.loc[idx, 'Value']:,.2f}"
            elif idx == 'Return %':
                summary_display.loc[idx, 'Value'] = f"{summary_display.loc[idx, 'Value']:.2f}%"
        st.dataframe(summary_display, use_container_width=True)
    
    with col2:
        if not results['trades'].empty:
            # Trade distribution pie chart
            trade_counts = results['trades']['signal_type'].value_counts()
            fig = px.pie(
                values=trade_counts.values,
                names=trade_counts.index,
                title="Trade Types Distribution"
            )
            st.plotly_chart(fig, use_container_width=True)
    
    # Detailed trades
    with st.expander("View Detailed Trades"):
        if not results['trades'].empty:
            st.dataframe(
                results['trades'].style.format({
                    'price': '${:.2f}',
                    'fee': '${:.2f}',
                    'slippage': '${:.4f}'
                }),
                use_container_width=True
            )
    
    # Download results
    st.markdown("---")
    st.subheader("📥 Download Results")
    
    col1, col2 = st.columns(2)
    
    with col1:
        # Download trades as CSV
        if not results['trades'].empty:
            csv = results['trades'].to_csv(index=False)
            st.download_button(
                label="Download Trades (CSV)",
                data=csv,
                file_name=f"trades_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
                mime="text/csv"
            )
    
    with col2:
        # Download metrics as JSON
        metrics_json = json.dumps(results['metrics'], indent=2)
        st.download_button(
            label="Download Metrics (JSON)",
            data=metrics_json,
            file_name=f"metrics_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json",
            mime="application/json"
        )

