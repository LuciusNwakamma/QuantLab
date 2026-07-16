"""Alpaca historical market data provider."""
from __future__ import annotations

from datetime import datetime

import pandas as pd

from .base import BaseDataProvider, Resolution
from .config import MarketDataCredentials


class AlpacaProvider(BaseDataProvider):
    """Fetch OHLCV bars from Alpaca Data API."""

    def __init__(
        self,
        api_key: str | None = None,
        secret_key: str | None = None,
        credentials: MarketDataCredentials | None = None,
    ):
        creds = credentials or MarketDataCredentials.from_env()
        self._api_key = api_key or creds.alpaca_api_key
        self._secret_key = secret_key or creds.alpaca_secret_key
        if not self._api_key or not self._secret_key:
            raise ValueError(
                "AlpacaProvider requires ALPACA_API_KEY and ALPACA_SECRET_KEY."
            )

        try:
            from alpaca.data.historical import StockHistoricalDataClient
        except ImportError as exc:
            raise ImportError(
                "alpaca-py is not installed. Install dependencies from python/requirements.txt"
            ) from exc

        self._client = StockHistoricalDataClient(
            api_key=self._api_key,
            secret_key=self._secret_key,
        )

    @property
    def name(self) -> str:
        return "Alpaca"

    def fetch_bars(
        self,
        symbol: str,
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> pd.DataFrame:
        try:
            from alpaca.data.requests import StockBarsRequest
            from alpaca.data.timeframe import TimeFrame, TimeFrameUnit
        except ImportError as exc:
            raise ImportError(
                "alpaca-py is not installed. Install dependencies from python/requirements.txt"
            ) from exc

        timeframe = _map_resolution_to_timeframe(resolution, TimeFrame, TimeFrameUnit)
        request = StockBarsRequest(
            symbol_or_symbols=symbol,
            timeframe=timeframe,
            start=start,
            end=end,
            adjustment="raw",
            feed="sip",
        )

        bars = self._client.get_stock_bars(request)
        df = bars.df
        if df.empty:
            return pd.DataFrame()

        # Multi-index response: (symbol, timestamp)
        if isinstance(df.index, pd.MultiIndex):
            try:
                df = df.xs(symbol, level=0)
            except KeyError:
                return pd.DataFrame()

        df.index = pd.to_datetime(df.index, utc=True)
        keep_cols = [c for c in ["open", "high", "low", "close", "volume", "vwap"] if c in df.columns]
        out = df[keep_cols].copy()
        out["symbol"] = symbol
        return out

    def fetch_multiple(
        self,
        symbols: list[str],
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> dict[str, pd.DataFrame]:
        return {s: self.fetch_bars(s, start, end, resolution) for s in symbols}


def _map_resolution_to_timeframe(resolution: Resolution, TimeFrame, TimeFrameUnit):
    if resolution == "1m":
        return TimeFrame(1, TimeFrameUnit.Minute)
    if resolution == "5m":
        return TimeFrame(5, TimeFrameUnit.Minute)
    if resolution == "15m":
        return TimeFrame(15, TimeFrameUnit.Minute)
    if resolution == "1h":
        return TimeFrame(1, TimeFrameUnit.Hour)
    if resolution == "4h":
        return TimeFrame(4, TimeFrameUnit.Hour)
    if resolution == "1wk":
        return TimeFrame(1, TimeFrameUnit.Week)
    return TimeFrame(1, TimeFrameUnit.Day)
