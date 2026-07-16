"""Polygon.io historical market data provider."""
from __future__ import annotations

from datetime import datetime

import pandas as pd

from .base import BaseDataProvider, Resolution
from .config import MarketDataCredentials


class PolygonProvider(BaseDataProvider):
    """Fetch OHLCV bars from Polygon aggregates endpoint."""

    def __init__(
        self,
        api_key: str | None = None,
        credentials: MarketDataCredentials | None = None,
    ):
        creds = credentials or MarketDataCredentials.from_env()
        self._api_key = api_key or creds.polygon_api_key
        if not self._api_key:
            raise ValueError(
                "PolygonProvider requires POLYGON_API_KEY. "
                "Set it in PowerShell with: $env:POLYGON_API_KEY='your_key'. "
                "Fallbacks also accepted: POLYGON_APT_KEY, POLYGON_KEY."
            )

        try:
            from polygon import RESTClient
        except ImportError as exc:
            raise ImportError(
                "polygon-api-client is not installed. Install dependencies from python/requirements.txt"
            ) from exc

        self._client = RESTClient(api_key=self._api_key)

    @property
    def name(self) -> str:
        return "Polygon"

    def fetch_bars(
        self,
        symbol: str,
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> pd.DataFrame:
        multiplier, timespan = _resolution_to_polygon_window(resolution)

        aggs = list(
            self._client.list_aggs(
                ticker=symbol,
                multiplier=multiplier,
                timespan=timespan,
                from_=start.strftime("%Y-%m-%d"),
                to=end.strftime("%Y-%m-%d"),
                adjusted=True,
                sort="asc",
                limit=50_000,
            )
        )

        if not aggs:
            return pd.DataFrame()

        rows: list[dict[str, float | str]] = []
        index: list[pd.Timestamp] = []
        for agg in aggs:
            ts = pd.to_datetime(agg.timestamp, unit="ms", utc=True)
            index.append(ts)
            rows.append(
                {
                    "open": float(agg.open),
                    "high": float(agg.high),
                    "low": float(agg.low),
                    "close": float(agg.close),
                    "volume": float(agg.volume),
                    "vwap": float(getattr(agg, "vwap", 0.0) or 0.0),
                    "symbol": symbol,
                }
            )

        df = pd.DataFrame(rows, index=index)
        df.index.name = "timestamp"
        return df

    def fetch_multiple(
        self,
        symbols: list[str],
        start: datetime,
        end: datetime,
        resolution: Resolution = "1d",
    ) -> dict[str, pd.DataFrame]:
        return {s: self.fetch_bars(s, start, end, resolution) for s in symbols}


def _resolution_to_polygon_window(resolution: Resolution) -> tuple[int, str]:
    mapping: dict[Resolution, tuple[int, str]] = {
        "1m": (1, "minute"),
        "5m": (5, "minute"),
        "15m": (15, "minute"),
        "1h": (1, "hour"),
        "4h": (4, "hour"),
        "1d": (1, "day"),
        "1wk": (1, "week"),
    }
    return mapping[resolution]
