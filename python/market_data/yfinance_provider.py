"""Yahoo Finance data provider (via yfinance)."""
from __future__ import annotations

from datetime import datetime
from typing import Literal

import pandas as pd
import yfinance as yf

from .base import BaseDataProvider, Resolution

_YF_INTERVAL_MAP: dict[str, str] = {
    "1m": "1m", "5m": "5m", "15m": "15m",
    "1h": "1h", "4h": "1h",  # yfinance has no 4h
    "1d": "1d", "1wk": "1wk",
}


class YFinanceProvider(BaseDataProvider):
    """Download historical OHLCV data from Yahoo Finance."""

    @property
    def name(self) -> str:
        return "YFinance"

    def fetch_bars(
        self,
        symbol: str,
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> pd.DataFrame:
        interval = _YF_INTERVAL_MAP.get(resolution, "1d")
        ticker = yf.Ticker(symbol)
        df = ticker.history(
            start=start.strftime("%Y-%m-%d"),
            end=end.strftime("%Y-%m-%d"),
            interval=interval,
            auto_adjust=True,
        )
        if df.empty:
            return pd.DataFrame()
        df.index = pd.to_datetime(df.index, utc=True)
        df.columns = [c.lower() for c in df.columns]
        df = df[["open", "high", "low", "close", "volume"]].copy()
        df["symbol"] = symbol
        return df

    def fetch_multiple(
        self,
        symbols: list[str],
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> dict[str, pd.DataFrame]:
        return {s: self.fetch_bars(s, start, end, resolution) for s in symbols}
