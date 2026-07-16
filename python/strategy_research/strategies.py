"""Strategy implementations: mean reversion, momentum, pairs trading, etc."""
from __future__ import annotations

import numpy as np
import pandas as pd
from dataclasses import dataclass, field
from typing import Protocol


@dataclass
class Signal:
    symbol: str
    direction: float    # -1.0 (short) to 1.0 (long)
    target_weight: float
    confidence: float = 1.0
    meta: dict = field(default_factory=dict)


class Strategy(Protocol):
    def generate_signals(self, data: dict[str, pd.DataFrame]) -> list[Signal]:
        ...


# ---- Mean Reversion ----

class MeanReversionStrategy:
    """Bollinger-Band / Z-score mean reversion."""

    def __init__(self, window: int = 20, z_entry: float = 2.0, z_exit: float = 0.5):
        self.window  = window
        self.z_entry = z_entry
        self.z_exit  = z_exit

    def generate_signals(self, data: dict[str, pd.DataFrame]) -> list[Signal]:
        signals = []
        for sym, df in data.items():
            close = df["close"]
            mean  = close.rolling(self.window).mean()
            std   = close.rolling(self.window).std()
            z     = (close - mean) / std.replace(0, np.nan)
            last_z = z.iloc[-1]
            if np.isnan(last_z):
                continue
            if last_z < -self.z_entry:
                signals.append(Signal(sym, 1.0, 0.10, confidence=min(abs(last_z)/4, 1.0)))
            elif last_z > self.z_entry:
                signals.append(Signal(sym, -1.0, -0.10, confidence=min(abs(last_z)/4, 1.0)))
        return signals


# ---- Momentum ----

class MomentumStrategy:
    """Cross-sectional momentum: long top decile, short bottom decile."""

    def __init__(self, lookback: int = 252, hold: int = 21, skip: int = 21):
        self.lookback = lookback
        self.hold     = hold
        self.skip     = skip   # skip most recent month (Jegadeesh-Titman)

    def generate_signals(self, data: dict[str, pd.DataFrame]) -> list[Signal]:
        returns: dict[str, float] = {}
        for sym, df in data.items():
            close = df["close"]
            if len(close) < self.lookback + self.skip:
                continue
            ret = (close.iloc[-(self.skip+1)] / close.iloc[-(self.lookback+self.skip)] - 1)
            returns[sym] = ret

        if not returns:
            return []

        sorted_syms = sorted(returns, key=returns.__getitem__)
        n = len(sorted_syms)
        decile = max(n // 10, 1)
        short_syms = sorted_syms[:decile]
        long_syms  = sorted_syms[-decile:]
        weight = 1.0 / decile

        signals = []
        for sym in long_syms:
            signals.append(Signal(sym, 1.0, weight))
        for sym in short_syms:
            signals.append(Signal(sym, -1.0, -weight))
        return signals


# ---- Pairs Trading ----

class PairsTradingStrategy:
    """Statistical arbitrage via cointegrated pairs."""

    def __init__(
        self,
        symbol_a: str,
        symbol_b: str,
        entry_z: float = 2.0,
        exit_z: float = 0.5,
        window: int = 60,
    ):
        self.symbol_a = symbol_a
        self.symbol_b = symbol_b
        self.entry_z  = entry_z
        self.exit_z   = exit_z
        self.window   = window

    def generate_signals(self, data: dict[str, pd.DataFrame]) -> list[Signal]:
        if self.symbol_a not in data or self.symbol_b not in data:
            return []
        a = data[self.symbol_a]["close"]
        b = data[self.symbol_b]["close"]

        # OLS hedge ratio over rolling window
        beta = (a.rolling(self.window).cov(b) /
                b.rolling(self.window).var().replace(0, np.nan))
        spread = a - beta * b
        z = (spread - spread.rolling(self.window).mean()) / \
            spread.rolling(self.window).std().replace(0, np.nan)
        last_z = z.iloc[-1]
        if np.isnan(last_z):
            return []
        if last_z < -self.entry_z:
            return [Signal(self.symbol_a, 1.0, 0.05),
                    Signal(self.symbol_b, -1.0, -0.05)]
        if last_z > self.entry_z:
            return [Signal(self.symbol_a, -1.0, -0.05),
                    Signal(self.symbol_b, 1.0, 0.05)]
        return []
