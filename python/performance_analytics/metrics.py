"""Performance analytics: Sharpe, drawdown, attribution, rolling metrics."""
from __future__ import annotations

import numpy as np
import pandas as pd
from typing import NamedTuple


class PerformanceMetrics(NamedTuple):
    total_return: float
    annualized_return: float
    annualized_volatility: float
    sharpe_ratio: float
    sortino_ratio: float
    calmar_ratio: float
    max_drawdown: float
    max_drawdown_duration: int   # bars
    win_rate: float
    profit_factor: float
    information_ratio: float
    alpha: float
    beta: float
    total_trades: int
    avg_trade_return: float


def compute_metrics(
    returns: pd.Series,
    benchmark: pd.Series | None = None,
    risk_free_rate: float = 0.045,
    periods_per_year: int = 252,
) -> PerformanceMetrics:
    r = returns.dropna()
    if r.empty:
        return PerformanceMetrics(*([0.0] * 14 + [0]))

    total_ret = (1 + r).prod() - 1
    n_years   = len(r) / periods_per_year
    ann_ret   = (1 + total_ret) ** (1 / max(n_years, 1e-6)) - 1
    ann_vol   = r.std() * np.sqrt(periods_per_year)
    rf_daily  = risk_free_rate / periods_per_year

    sharpe = (r.mean() - rf_daily) / r.std() * np.sqrt(periods_per_year) if r.std() > 0 else 0.0

    downside = r[r < 0].std() * np.sqrt(periods_per_year)
    sortino  = (ann_ret - risk_free_rate) / downside if downside > 0 else 0.0

    # Drawdown
    equity    = (1 + r).cumprod()
    peak      = equity.cummax()
    drawdown  = (equity - peak) / peak
    max_dd    = drawdown.min()

    # Drawdown duration
    in_dd = drawdown < 0
    max_dur = 0
    cur_dur = 0
    for v in in_dd:
        cur_dur = cur_dur + 1 if v else 0
        max_dur = max(max_dur, cur_dur)

    calmar = ann_ret / abs(max_dd) if max_dd != 0 else 0.0

    # Win rate
    wins        = (r > 0).sum()
    total_trades = len(r)
    win_rate    = wins / total_trades if total_trades > 0 else 0.0
    avg_win     = r[r > 0].mean() if (r > 0).any() else 0.0
    avg_loss    = r[r < 0].mean() if (r < 0).any() else 0.0
    profit_factor = (wins * avg_win) / (-(total_trades - wins) * avg_loss) \
                    if avg_loss != 0 else float("inf")

    # Alpha / Beta vs benchmark
    alpha, beta = 0.0, 1.0
    info_ratio  = 0.0
    if benchmark is not None:
        bm = benchmark.reindex(r.index).dropna()
        common = r.index.intersection(bm.index)
        if len(common) > 10:
            rr, bb = r.loc[common], bm.loc[common]
            cov_matrix = np.cov(rr, bb)
            beta  = cov_matrix[0, 1] / cov_matrix[1, 1] if cov_matrix[1, 1] != 0 else 1.0
            alpha = (rr.mean() - beta * bb.mean()) * periods_per_year
            active = rr - bb
            info_ratio = (active.mean() / active.std() * np.sqrt(periods_per_year)
                          if active.std() > 0 else 0.0)

    return PerformanceMetrics(
        total_return=total_ret,
        annualized_return=ann_ret,
        annualized_volatility=ann_vol,
        sharpe_ratio=sharpe,
        sortino_ratio=sortino,
        calmar_ratio=calmar,
        max_drawdown=max_dd,
        max_drawdown_duration=max_dur,
        win_rate=win_rate,
        profit_factor=profit_factor,
        information_ratio=info_ratio,
        alpha=alpha,
        beta=beta,
        total_trades=total_trades,
        avg_trade_return=r.mean(),
    )


def rolling_sharpe(returns: pd.Series, window: int = 252, rf: float = 0.045) -> pd.Series:
    rf_daily = rf / 252.0
    excess = returns - rf_daily
    return excess.rolling(window).mean() / returns.rolling(window).std() * np.sqrt(252)


def rolling_drawdown(equity: pd.Series) -> pd.Series:
    peak = equity.cummax()
    return (equity - peak) / peak


def print_tearsheet(metrics: PerformanceMetrics) -> None:
    print(f"{'='*50}")
    print(f"{'PERFORMANCE TEARSHEET':^50}")
    print(f"{'='*50}")
    print(f"  Total Return          : {metrics.total_return:>10.2%}")
    print(f"  Annualized Return     : {metrics.annualized_return:>10.2%}")
    print(f"  Annualized Volatility : {metrics.annualized_volatility:>10.2%}")
    print(f"  Sharpe Ratio          : {metrics.sharpe_ratio:>10.3f}")
    print(f"  Sortino Ratio         : {metrics.sortino_ratio:>10.3f}")
    print(f"  Calmar Ratio          : {metrics.calmar_ratio:>10.3f}")
    print(f"  Max Drawdown          : {metrics.max_drawdown:>10.2%}")
    print(f"  Max DD Duration (bars): {metrics.max_drawdown_duration:>10d}")
    print(f"  Win Rate              : {metrics.win_rate:>10.2%}")
    print(f"  Profit Factor         : {metrics.profit_factor:>10.3f}")
    print(f"  Information Ratio     : {metrics.information_ratio:>10.3f}")
    print(f"  Alpha (ann.)          : {metrics.alpha:>10.4f}")
    print(f"  Beta                  : {metrics.beta:>10.3f}")
    print(f"{'='*50}")
