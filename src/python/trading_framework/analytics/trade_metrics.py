"""Trade-based performance metrics calculation using executed trades from C++/Rust engine."""

from typing import Dict, Tuple

import pandas as pd
import numpy as np


class TradeAnalyzer:
    """
    Analyzes performance based on actual executed trades from the engine.
    
    This follows the correct architecture where C++/Rust handles execution
    and Python analyzes the results.
    """
    
    def __init__(self, initial_capital: float = 100000.0) -> None:
        """
        Initialize trade analyzer.
        
        Args:
            initial_capital: Starting capital for P&L calculations
        """
        self.initial_capital = initial_capital
    
    def analyze_trades(
        self,
        trades: pd.DataFrame,
        market_data: pd.DataFrame,
        states: pd.DataFrame = None
    ) -> Dict[str, pd.DataFrame]:
        """
        Analyze trades and build portfolio metrics.
        
        Args:
            trades: Executed trades from C++/Rust engine
            market_data: Historical market data
            states: Optional portfolio states from engine
            
        Returns:
            Dictionary containing:
                - portfolio: DataFrame with daily portfolio values
                - trade_summary: Summary of trade statistics
        """
        # Build portfolio from trades
        portfolio = self._build_portfolio_from_trades(trades, market_data)
        
        # Calculate trade summary
        trade_summary = self._calculate_trade_summary(trades, portfolio)
        
        return {
            'portfolio': portfolio,
            'trade_summary': trade_summary
        }
    
    def _build_portfolio_from_trades(
        self,
        trades: pd.DataFrame,
        market_data: pd.DataFrame
    ) -> pd.DataFrame:
        """
        Build daily portfolio values from executed trades.
        
        This reconstructs the portfolio based on actual fills,
        not theoretical signals.
        """
        # Initialize portfolio with proper dtypes
        portfolio = pd.DataFrame(index=market_data.index)
        portfolio['price'] = market_data['close']
        portfolio['cash'] = float(self.initial_capital)
        portfolio['position'] = 0.0
        portfolio['value'] = float(self.initial_capital)
        
        if trades.empty:
            # No trades, portfolio stays in cash
            portfolio['holdings_value'] = 0.0
            portfolio['returns'] = 0.0
            return portfolio
        
        # Sort trades by timestamp
        trades = trades.sort_values('timestamp')
        
        # Apply each trade to portfolio
        for _, trade in trades.iterrows():
            # Find the date of the trade
            trade_date = pd.Timestamp(trade['timestamp'])
            
            # Remove timezone info if present to match portfolio index
            if hasattr(portfolio.index, 'tz') and portfolio.index.tz is not None:
                trade_date = trade_date.tz_localize(portfolio.index.tz)
            elif hasattr(trade_date, 'tz') and trade_date.tz is not None:
                trade_date = trade_date.tz_localize(None)
            
            # Find all dates from this trade onwards
            mask = portfolio.index >= trade_date
            
            # Update position and cash based on trade
            if trade['side'] == 'BUY':
                portfolio.loc[mask, 'position'] += trade['quantity']
                # Cost includes price, fee, and slippage
                total_cost = (trade['price'] * trade['quantity']) + trade['fee']
                portfolio.loc[mask, 'cash'] -= total_cost
            else:  # SELL
                portfolio.loc[mask, 'position'] -= trade['quantity']
                # Proceeds are price minus fee
                total_proceeds = (trade['price'] * trade['quantity']) - trade['fee']
                portfolio.loc[mask, 'cash'] += total_proceeds
        
        # Calculate daily portfolio value
        portfolio['holdings_value'] = portfolio['position'] * portfolio['price']
        portfolio['value'] = portfolio['cash'] + portfolio['holdings_value']
        
        # Calculate returns
        portfolio['returns'] = portfolio['value'].pct_change().fillna(0)
        
        # Add cumulative returns
        portfolio['cumulative_returns'] = (1 + portfolio['returns']).cumprod() - 1
        
        return portfolio
    
    def _calculate_trade_summary(
        self,
        trades: pd.DataFrame,
        portfolio: pd.DataFrame
    ) -> pd.DataFrame:
        """Calculate summary statistics from trades."""
        if trades.empty:
            return pd.DataFrame({
                'Total Trades': [0],
                'Buy Trades': [0],
                'Sell Trades': [0],
                'Total Fees': [0.0],
                'Total Slippage': [0.0],
                'Avg Slippage per Trade': [0.0],
                'Total P&L': [0.0],
                'Return %': [0.0]
            })
        
        # Count trades by type
        buy_trades = len(trades[trades['side'] == 'BUY'])
        sell_trades = len(trades[trades['side'] == 'SELL'])
        
        # Sum costs
        total_fees = trades['fee'].sum()
        total_slippage = trades['slippage'].sum()
        avg_slippage = trades['slippage'].mean()
        
        # Calculate P&L
        final_value = portfolio['value'].iloc[-1]
        total_pnl = final_value - self.initial_capital
        return_pct = (final_value / self.initial_capital - 1) * 100
        
        # Count signal types
        signal_counts = trades['signal_type'].value_counts().to_dict()
        
        summary_data = {
            'Total Trades': [len(trades)],
            'Buy Trades': [buy_trades],
            'Sell Trades': [sell_trades],
            'Entry Signals': [signal_counts.get('ENTRY', 0)],
            'Exit Signals': [signal_counts.get('EXIT', 0)],
            'Rebalance Signals': [signal_counts.get('REBALANCE', 0)],
            'Total Fees': [total_fees],
            'Total Slippage': [total_slippage],
            'Avg Slippage per Trade': [avg_slippage],
            'Total P&L': [total_pnl],
            'Return %': [return_pct]
        }
        
        return pd.DataFrame(summary_data).T.rename(columns={0: 'Value'})
    
    def calculate_performance_metrics(
        self,
        portfolio: pd.DataFrame,
        risk_free_rate: float = 0.02
    ) -> Dict[str, float]:
        """
        Calculate performance metrics from portfolio built with actual trades.
        
        Args:
            portfolio: Portfolio DataFrame from analyze_trades
            risk_free_rate: Annual risk-free rate
            
        Returns:
            Dictionary of performance metrics
        """
        returns = portfolio['returns']
        
        # Basic metrics
        total_return = (portfolio['value'].iloc[-1] / portfolio['value'].iloc[0]) - 1
        
        # Annualized metrics
        days = len(returns)
        years = days / 252
        annualized_return = (1 + total_return) ** (1 / years) - 1 if years > 0 else 0
        
        # Risk metrics
        volatility = returns.std() * np.sqrt(252)
        daily_rf = risk_free_rate / 252
        excess_returns = returns - daily_rf
        
        sharpe_ratio = (excess_returns.mean() * 252) / volatility if volatility > 0 else 0
        
        # Drawdown
        cumulative = (1 + returns).cumprod()
        running_max = cumulative.expanding().max()
        drawdown = (cumulative - running_max) / running_max
        max_drawdown = drawdown.min()
        
        # Sortino ratio (downside deviation)
        downside_returns = returns[returns < daily_rf]
        downside_dev = downside_returns.std() * np.sqrt(252) if len(downside_returns) > 0 else 0
        sortino_ratio = (excess_returns.mean() * 252) / downside_dev if downside_dev > 0 else 0
        
        return {
            'total_return': total_return,
            'annualized_return': annualized_return,
            'volatility': volatility,
            'sharpe_ratio': sharpe_ratio,
            'sortino_ratio': sortino_ratio,
            'max_drawdown': max_drawdown,
            'calmar_ratio': annualized_return / abs(max_drawdown) if max_drawdown != 0 else 0
        }