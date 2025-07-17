"""Performance metrics calculation for trading strategies."""

from typing import Dict, Optional, Tuple

import numpy as np
import pandas as pd


class PerformanceAnalyzer:
    """Calculate and analyze trading strategy performance metrics."""
    
    def __init__(self, risk_free_rate: float = 0.02) -> None:
        """
        Initialize performance analyzer.
        
        Args:
            risk_free_rate: Annual risk-free rate for Sharpe ratio calculation
        """
        self.risk_free_rate = risk_free_rate
        self.daily_rf = risk_free_rate / 252  # Convert to daily
    
    def calculate_metrics(
        self,
        portfolio: pd.DataFrame,
        trades: pd.DataFrame
    ) -> Dict[str, float]:
        """
        Calculate comprehensive performance metrics.
        
        Args:
            portfolio: DataFrame with portfolio values and returns
            trades: DataFrame with executed trades
            
        Returns:
            Dictionary of performance metrics
        """
        # Ensure we have returns
        if 'returns' not in portfolio.columns:
            portfolio['returns'] = portfolio['value'].pct_change().fillna(0)
        
        returns = portfolio['returns'].dropna()
        
        # Basic return metrics
        total_return = self._calculate_total_return(portfolio)
        annualized_return = self._calculate_annualized_return(returns)
        
        # Risk metrics
        volatility = self._calculate_volatility(returns)
        sharpe_ratio = self._calculate_sharpe_ratio(returns, volatility)
        sortino_ratio = self._calculate_sortino_ratio(returns)
        
        # Drawdown metrics
        max_dd, max_dd_duration = self._calculate_max_drawdown(portfolio['value'])
        calmar_ratio = annualized_return / abs(max_dd) if max_dd != 0 else 0
        
        # Trade metrics
        trade_metrics = self._calculate_trade_metrics(trades, returns)
        
        return {
            'total_return': total_return,
            'annualized_return': annualized_return,
            'volatility': volatility,
            'sharpe_ratio': sharpe_ratio,
            'sortino_ratio': sortino_ratio,
            'max_drawdown': max_dd,
            'max_drawdown_duration': max_dd_duration,
            'calmar_ratio': calmar_ratio,
            **trade_metrics
        }
    
    def _calculate_total_return(self, portfolio: pd.DataFrame) -> float:
        """Calculate total return from portfolio values."""
        initial_value = portfolio['value'].iloc[0]
        final_value = portfolio['value'].iloc[-1]
        return (final_value - initial_value) / initial_value
    
    def _calculate_annualized_return(self, returns: pd.Series) -> float:
        """Calculate annualized return from daily returns."""
        total_days = len(returns)
        if total_days == 0:
            return 0.0
        
        cumulative_return = (1 + returns).prod() - 1
        years = total_days / 252
        
        if years <= 0:
            return 0.0
        
        return (1 + cumulative_return) ** (1 / years) - 1
    
    def _calculate_volatility(self, returns: pd.Series) -> float:
        """Calculate annualized volatility."""
        return returns.std() * np.sqrt(252)
    
    def _calculate_sharpe_ratio(
        self,
        returns: pd.Series,
        volatility: Optional[float] = None
    ) -> float:
        """Calculate Sharpe ratio."""
        if volatility is None:
            volatility = self._calculate_volatility(returns)
        
        if volatility == 0:
            return 0.0
        
        excess_returns = returns - self.daily_rf
        return (excess_returns.mean() * 252) / volatility
    
    def _calculate_sortino_ratio(self, returns: pd.Series) -> float:
        """Calculate Sortino ratio (uses downside deviation)."""
        excess_returns = returns - self.daily_rf
        downside_returns = excess_returns[excess_returns < 0]
        
        if len(downside_returns) == 0:
            return 0.0
        
        downside_deviation = downside_returns.std() * np.sqrt(252)
        
        if downside_deviation == 0:
            return 0.0
        
        return (excess_returns.mean() * 252) / downside_deviation
    
    def _calculate_max_drawdown(
        self,
        values: pd.Series
    ) -> Tuple[float, int]:
        """
        Calculate maximum drawdown and duration.
        
        Returns:
            Tuple of (max_drawdown, max_duration_days)
        """
        # Calculate running maximum
        running_max = values.expanding().max()
        
        # Calculate drawdown series
        drawdown = (values - running_max) / running_max
        
        # Find maximum drawdown
        max_dd = drawdown.min()
        
        # Calculate drawdown duration
        drawdown_start = None
        max_duration = 0
        current_duration = 0
        
        for i, dd in enumerate(drawdown):
            if dd < 0:
                if drawdown_start is None:
                    drawdown_start = i
                current_duration = i - drawdown_start
            else:
                if current_duration > max_duration:
                    max_duration = current_duration
                drawdown_start = None
                current_duration = 0
        
        # Check final duration
        if current_duration > max_duration:
            max_duration = current_duration
        
        return max_dd, max_duration
    
    def _calculate_trade_metrics(
        self,
        trades: pd.DataFrame,
        returns: pd.Series
    ) -> Dict[str, float]:
        """Calculate trade-specific metrics."""
        if trades.empty:
            return {
                'num_trades': 0,
                'win_rate': 0.0,
                'avg_win': 0.0,
                'avg_loss': 0.0,
                'profit_factor': 0.0,
            }
        
        # Count trades
        num_trades = len(trades)
        
        # Calculate P&L for each trade
        # This is simplified - in reality we'd match buy/sell pairs
        trade_pnl = []
        
        # For now, use returns to estimate win rate
        positive_returns = returns[returns > 0]
        negative_returns = returns[returns < 0]
        
        win_rate = len(positive_returns) / len(returns) if len(returns) > 0 else 0
        avg_win = positive_returns.mean() if len(positive_returns) > 0 else 0
        avg_loss = negative_returns.mean() if len(negative_returns) > 0 else 0
        
        # Profit factor
        total_wins = positive_returns.sum()
        total_losses = abs(negative_returns.sum())
        profit_factor = total_wins / total_losses if total_losses > 0 else 0
        
        return {
            'num_trades': num_trades,
            'win_rate': win_rate,
            'avg_win': avg_win,
            'avg_loss': avg_loss,
            'profit_factor': profit_factor,
        }
    
    def create_summary_report(
        self,
        metrics: Dict[str, float]
    ) -> pd.DataFrame:
        """Create a formatted summary report."""
        # Format metrics for display
        formatted_metrics = {
            'Total Return': f"{metrics['total_return']:.2%}",
            'Annualized Return': f"{metrics['annualized_return']:.2%}",
            'Volatility': f"{metrics['volatility']:.2%}",
            'Sharpe Ratio': f"{metrics['sharpe_ratio']:.2f}",
            'Sortino Ratio': f"{metrics['sortino_ratio']:.2f}",
            'Max Drawdown': f"{metrics['max_drawdown']:.2%}",
            'Max DD Duration': f"{metrics['max_drawdown_duration']} days",
            'Calmar Ratio': f"{metrics['calmar_ratio']:.2f}",
            'Number of Trades': f"{metrics['num_trades']:.0f}",
            'Win Rate': f"{metrics['win_rate']:.2%}",
            'Profit Factor': f"{metrics['profit_factor']:.2f}",
        }
        
        return pd.DataFrame.from_dict(
            formatted_metrics,
            orient='index',
            columns=['Value']
        )