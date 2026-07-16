"""Abstract base class for market data providers."""
from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from datetime import datetime
from typing import Literal

import pandas as pd


Resolution = Literal["1m", "5m", "15m", "1h", "4h", "1d", "1wk"]


@dataclass
class OHLCV:
    """Single OHLCV bar."""
    symbol: str
    timestamp: datetime
    open: float
    high: float
    low: float
    close: float
    volume: float
    vwap: float = 0.0


class BaseDataProvider(ABC):
    """Abstract interface every data provider must implement."""

    @abstractmethod
    def fetch_bars(
        self,
        symbol: str,
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> pd.DataFrame:
        """Return OHLCV DataFrame indexed by timestamp."""

    @abstractmethod
    def fetch_multiple(
        self,
        symbols: list[str],
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> dict[str, pd.DataFrame]:
        """Return per-symbol OHLCV DataFrames."""

    @property
    @abstractmethod
    def name(self) -> str:
        """Provider identifier."""
