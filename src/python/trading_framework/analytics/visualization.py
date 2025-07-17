"""Visualization utilities for trading performance analysis."""

from typing import Dict, Optional

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns


def create_tearsheet(
    portfolio: pd.DataFrame,
    trades: pd.DataFrame,
    metrics: Dict[str, float],
    signals: Optional[pd.DataFrame] = None,
    title: str = "Strategy Performance Tearsheet",
    save_path: str = "performance_tearsheet.png"
) -> None:
    """
    Create a comprehensive performance tearsheet.
    
    Args:
        portfolio: DataFrame with portfolio values and returns
        trades: DataFrame with executed trades
        metrics: Dictionary of performance metrics
        signals: Optional DataFrame with strategy signals
        title: Title for the tearsheet
        save_path: Path to save the tearsheet image
    """
    # Set style
    plt.style.use('seaborn-v0_8-darkgrid')
    sns.set_palette("husl")
    
    # Create figure with subplots
    fig = plt.figure(figsize=(16, 12))
    
    # Create grid spec for custom layout
    gs = fig.add_gridspec(4, 3, hspace=0.3, wspace=0.25)
    
    # 1. Cumulative Returns (top, full width)
    ax1 = fig.add_subplot(gs[0, :])
    _plot_cumulative_returns(ax1, portfolio)
    
    # 2. Drawdown (second row, left 2/3)
    ax2 = fig.add_subplot(gs[1, :2])
    _plot_drawdown(ax2, portfolio)
    
    # 3. Returns Distribution (second row, right 1/3)
    ax3 = fig.add_subplot(gs[1, 2])
    _plot_returns_distribution(ax3, portfolio)
    
    # 4. Rolling Sharpe (third row, left)
    ax4 = fig.add_subplot(gs[2, 0])
    _plot_rolling_sharpe(ax4, portfolio)
    
    # 5. Monthly Returns Heatmap (third row, middle)
    ax5 = fig.add_subplot(gs[2, 1])
    _plot_monthly_returns(ax5, portfolio)
    
    # 6. Trade Analysis (third row, right)
    ax6 = fig.add_subplot(gs[2, 2])
    _plot_trade_analysis(ax6, trades, portfolio)
    
    # 7. Metrics Table (bottom row, left)
    ax7 = fig.add_subplot(gs[3, 0])
    _plot_metrics_table(ax7, metrics)
    
    # 8. Position & Volume (bottom row, middle and right)
    ax8 = fig.add_subplot(gs[3, 1:])
    _plot_position_and_volume(ax8, portfolio, trades)
    
    # Add main title
    fig.suptitle(title, fontsize=16, fontweight='bold', y=0.995)
    
    # Add timestamp
    timestamp = pd.Timestamp.now().strftime("%Y-%m-%d %H:%M:%S")
    fig.text(0.99, 0.01, f"Generated: {timestamp}", 
             ha='right', va='bottom', fontsize=8, alpha=0.5)
    
    # Save figure
    plt.tight_layout()
    plt.savefig(save_path, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Performance tearsheet saved to {save_path}")


def _plot_cumulative_returns(ax: plt.Axes, portfolio: pd.DataFrame) -> None:
    """Plot cumulative returns."""
    cum_returns = (1 + portfolio['returns']).cumprod() - 1
    
    ax.plot(portfolio.index, cum_returns * 100, linewidth=2, label='Strategy')
    ax.fill_between(portfolio.index, 0, cum_returns * 100, alpha=0.1)
    
    # Add zero line
    ax.axhline(y=0, color='black', linestyle='-', alpha=0.3)
    
    ax.set_title('Cumulative Returns', fontsize=14, fontweight='bold')
    ax.set_xlabel('Date')
    ax.set_ylabel('Cumulative Return (%)')
    ax.legend()
    ax.grid(True, alpha=0.3)


def _plot_drawdown(ax: plt.Axes, portfolio: pd.DataFrame) -> None:
    """Plot drawdown chart."""
    # Calculate drawdown
    running_max = portfolio['value'].expanding().max()
    drawdown = (portfolio['value'] - running_max) / running_max
    
    ax.fill_between(portfolio.index, drawdown * 100, 0, 
                    color='red', alpha=0.3, label='Drawdown')
    ax.plot(portfolio.index, drawdown * 100, color='red', linewidth=1)
    
    # Mark maximum drawdown
    max_dd_idx = drawdown.idxmin()
    max_dd_value = drawdown.min()
    ax.plot(max_dd_idx, max_dd_value * 100, 'ro', markersize=8)
    ax.annotate(f'Max DD: {max_dd_value:.1%}',
                xy=(max_dd_idx, max_dd_value * 100),
                xytext=(10, 10), textcoords='offset points',
                fontsize=9, ha='left')
    
    ax.set_title('Drawdown', fontsize=14, fontweight='bold')
    ax.set_xlabel('Date')
    ax.set_ylabel('Drawdown (%)')
    ax.set_ylim(top=0.5)
    ax.grid(True, alpha=0.3)


def _plot_returns_distribution(ax: plt.Axes, portfolio: pd.DataFrame) -> None:
    """Plot returns distribution histogram."""
    returns = portfolio['returns'].dropna()
    
    # Plot histogram
    n, bins, patches = ax.hist(returns * 100, bins=50, alpha=0.7, 
                               color='blue', edgecolor='black')
    
    # Add normal distribution overlay
    mu, sigma = returns.mean() * 100, returns.std() * 100
    x = np.linspace(returns.min() * 100, returns.max() * 100, 100)
    ax.plot(x, len(returns) * (bins[1] - bins[0]) * 
            (1/np.sqrt(2*np.pi*sigma**2)) * np.exp(-0.5*((x-mu)/sigma)**2),
            'r-', linewidth=2, label='Normal')
    
    # Add vertical line at mean
    ax.axvline(mu, color='red', linestyle='--', alpha=0.7, label=f'Mean: {mu:.2f}%')
    
    ax.set_title('Returns Distribution', fontsize=14, fontweight='bold')
    ax.set_xlabel('Daily Return (%)')
    ax.set_ylabel('Frequency')
    ax.legend()
    ax.grid(True, alpha=0.3)


def _plot_rolling_sharpe(ax: plt.Axes, portfolio: pd.DataFrame, window: int = 252) -> None:
    """Plot rolling Sharpe ratio."""
    returns = portfolio['returns'].dropna()
    
    # Calculate rolling Sharpe
    rolling_mean = returns.rolling(window=window).mean()
    rolling_std = returns.rolling(window=window).std()
    rolling_sharpe = (rolling_mean * 252) / (rolling_std * np.sqrt(252))
    
    ax.plot(rolling_sharpe.index, rolling_sharpe, linewidth=2)
    ax.axhline(y=0, color='black', linestyle='-', alpha=0.3)
    ax.axhline(y=1, color='green', linestyle='--', alpha=0.5, label='Sharpe = 1')
    
    ax.set_title(f'Rolling Sharpe Ratio ({window}-day)', fontsize=14, fontweight='bold')
    ax.set_xlabel('Date')
    ax.set_ylabel('Sharpe Ratio')
    ax.legend()
    ax.grid(True, alpha=0.3)


def _plot_monthly_returns(ax: plt.Axes, portfolio: pd.DataFrame) -> None:
    """Plot monthly returns heatmap."""
    # Calculate monthly returns
    monthly = portfolio['returns'].resample('ME').apply(lambda x: (1 + x).prod() - 1)
    
    # Create matrix for heatmap
    monthly_df = pd.DataFrame(monthly)
    monthly_df['Year'] = monthly_df.index.year
    monthly_df['Month'] = monthly_df.index.month
    
    # Pivot to create heatmap matrix
    heatmap_data = monthly_df.pivot(index='Year', columns='Month', values='returns')
    
    # Plot heatmap
    sns.heatmap(heatmap_data * 100, annot=True, fmt='.1f', 
                cmap='RdYlGn', center=0, ax=ax,
                cbar_kws={'label': 'Return (%)'})
    
    ax.set_title('Monthly Returns (%)', fontsize=14, fontweight='bold')
    ax.set_xlabel('Month')
    ax.set_ylabel('Year')


def _plot_trade_analysis(ax: plt.Axes, trades: pd.DataFrame, portfolio: pd.DataFrame) -> None:
    """Plot trade analysis."""
    if trades.empty:
        ax.text(0.5, 0.5, 'No trades executed', 
                ha='center', va='center', fontsize=12)
        ax.set_title('Trade Analysis', fontsize=14, fontweight='bold')
        ax.axis('off')
        return
    
    # Simple win/loss analysis
    # This is simplified - in reality we'd match buy/sell pairs
    returns = portfolio['returns'].dropna()
    wins = (returns > 0).sum()
    losses = (returns < 0).sum()
    
    # Pie chart of wins vs losses
    sizes = [wins, losses]
    labels = ['Winning Days', 'Losing Days']
    colors = ['green', 'red']
    
    ax.pie(sizes, labels=labels, colors=colors, autopct='%1.1f%%',
           startangle=90, textprops={'fontsize': 10})
    
    ax.set_title('Win/Loss Analysis', fontsize=14, fontweight='bold')


def _plot_metrics_table(ax: plt.Axes, metrics: Dict[str, float]) -> None:
    """Plot metrics table."""
    ax.axis('off')
    
    # Prepare metrics for display
    display_metrics = [
        ('Total Return', f"{metrics.get('total_return', 0):.2%}"),
        ('Sharpe Ratio', f"{metrics.get('sharpe_ratio', 0):.2f}"),
        ('Max Drawdown', f"{metrics.get('max_drawdown', 0):.2%}"),
        ('Win Rate', f"{metrics.get('win_rate', 0):.1%}"),
        ('# Trades', f"{metrics.get('num_trades', 0):.0f}"),
    ]
    
    # Create table
    table_data = [[k, v] for k, v in display_metrics]
    table = ax.table(cellText=table_data,
                     colLabels=['Metric', 'Value'],
                     cellLoc='left',
                     loc='center',
                     colWidths=[0.6, 0.4])
    
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1.2, 1.5)
    
    # Style the table
    for i in range(len(display_metrics) + 1):
        for j in range(2):
            cell = table[(i, j)]
            if i == 0:  # Header
                cell.set_facecolor('#40466e')
                cell.set_text_props(weight='bold', color='white')
            else:
                cell.set_facecolor('#f1f1f2' if i % 2 == 0 else 'white')
                cell.set_text_props(color='black')
    
    ax.set_title('Key Metrics', fontsize=14, fontweight='bold', pad=20)


def _plot_position_and_volume(ax: plt.Axes, portfolio: pd.DataFrame, trades: pd.DataFrame) -> None:
    """Plot position and volume over time."""
    # Create twin axis
    ax2 = ax.twinx()
    
    # Plot position
    ax.plot(portfolio.index, portfolio.get('position', 0), 
            linewidth=2, color='blue', label='Position')
    ax.fill_between(portfolio.index, 0, portfolio.get('position', 0), 
                    alpha=0.2, color='blue')
    
    # Mark trades
    if not trades.empty:
        for _, trade in trades.iterrows():
            trade_time = pd.Timestamp(trade['timestamp'], unit='s')
            if trade_time in portfolio.index:
                idx = portfolio.index.get_loc(trade_time)
                color = 'green' if trade['side'] == 'BUY' else 'red'
                marker = '^' if trade['side'] == 'BUY' else 'v'
                ax.scatter(trade_time, portfolio.iloc[idx].get('position', 0),
                          color=color, marker=marker, s=100, zorder=5)
    
    ax.set_title('Position & Trading Activity', fontsize=14, fontweight='bold')
    ax.set_xlabel('Date')
    ax.set_ylabel('Position', color='blue')
    ax.tick_params(axis='y', labelcolor='blue')
    ax.grid(True, alpha=0.3)
    ax.legend(loc='upper left')