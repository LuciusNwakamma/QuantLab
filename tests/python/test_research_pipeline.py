"""Tests for research pipeline backtest core."""
from __future__ import annotations

from dataclasses import dataclass
import numpy as np
import pandas as pd

from python.research_pipeline import run_signal_backtest
from python.strategy_research.strategies import Signal


@dataclass
class _AlwaysLongStrategy:
    symbol: str
    weight: float = 0.5

    def generate_signals(self, data: dict[str, pd.DataFrame]) -> list[Signal]:
        return [Signal(symbol=self.symbol, direction=1.0, target_weight=self.weight)]


def _make_data(symbol: str, n: int = 160) -> pd.DataFrame:
    idx = pd.date_range("2022-01-01", periods=n, freq="D", tz="UTC")
    close = pd.Series(np.linspace(100.0, 120.0, n), index=idx)
    df = pd.DataFrame(
        {
            "open": close * 0.999,
            "high": close * 1.002,
            "low": close * 0.998,
            "close": close,
            "volume": 1_000_000.0,
            "symbol": symbol,
        },
        index=idx,
    )
    return df


def test_run_signal_backtest_returns_non_empty_series():
    data = {
        "SPY": _make_data("SPY"),
        "QQQ": _make_data("QQQ"),
    }
    strategy = _AlwaysLongStrategy(symbol="SPY", weight=0.4)

    returns, equity, weights = run_signal_backtest(
        data,
        strategy,
        warmup_bars=30,
        max_leverage=1.0,
        transaction_cost_bps=1.0,
    )

    assert len(returns) > 0
    assert len(equity) == len(returns)
    assert not weights.empty


def test_run_signal_backtest_respects_max_leverage():
    data = {
        "SPY": _make_data("SPY"),
        "QQQ": _make_data("QQQ"),
    }

    class _LeveredStrategy:
        def generate_signals(self, data: dict[str, pd.DataFrame]) -> list[Signal]:
            return [
                Signal(symbol="SPY", direction=1.0, target_weight=1.5),
                Signal(symbol="QQQ", direction=1.0, target_weight=1.0),
            ]

    returns, equity, weights = run_signal_backtest(
        data,
        _LeveredStrategy(),
        warmup_bars=30,
        max_leverage=1.0,
        transaction_cost_bps=0.0,
    )

    assert len(returns) > 0
    gross_exposure = weights.abs().sum(axis=1)
    assert (gross_exposure <= 1.000001).all()
