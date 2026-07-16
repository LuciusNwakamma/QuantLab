"""Unit tests for market data provider config and resolution mapping."""
from __future__ import annotations

import os

import pytest

from python.market_data.alpaca_provider import _map_resolution_to_timeframe
from python.market_data.polygon_provider import _resolution_to_polygon_window
from python.market_data.config import MarketDataCredentials


class _DummyTimeFrameUnit:
    Minute = "minute"
    Hour = "hour"
    Day = "day"
    Week = "week"


class _DummyTimeFrame:
    def __init__(self, amount: int, unit: str):
        self.amount = amount
        self.unit = unit


def test_polygon_resolution_mapping():
    assert _resolution_to_polygon_window("1m") == (1, "minute")
    assert _resolution_to_polygon_window("5m") == (5, "minute")
    assert _resolution_to_polygon_window("1h") == (1, "hour")
    assert _resolution_to_polygon_window("1d") == (1, "day")
    assert _resolution_to_polygon_window("1wk") == (1, "week")


def test_alpaca_resolution_mapping_uses_expected_window_values():
    tf = _map_resolution_to_timeframe("15m", _DummyTimeFrame, _DummyTimeFrameUnit)
    assert tf.amount == 15
    assert tf.unit == _DummyTimeFrameUnit.Minute

    tf = _map_resolution_to_timeframe("4h", _DummyTimeFrame, _DummyTimeFrameUnit)
    assert tf.amount == 4
    assert tf.unit == _DummyTimeFrameUnit.Hour


def test_credentials_validation():
    creds = MarketDataCredentials(alpaca_api_key="", alpaca_secret_key="", polygon_api_key="")

    with pytest.raises(ValueError):
        creds.require_alpaca()

    with pytest.raises(ValueError):
        creds.require_polygon()


def test_credentials_from_env(monkeypatch: pytest.MonkeyPatch):
    monkeypatch.setenv("ALPACA_API_KEY", "a")
    monkeypatch.setenv("ALPACA_SECRET_KEY", "b")
    monkeypatch.setenv("POLYGON_API_KEY", "c")
    creds = MarketDataCredentials.from_env()

    assert creds.alpaca_api_key == "a"
    assert creds.alpaca_secret_key == "b"
    assert creds.polygon_api_key == "c"
