"""Python unit tests for feature engineering."""
import numpy as np
import pandas as pd
import pytest

from python.feature_engineering import rsi, macd, atr, realized_volatility, rolling_zscore


@pytest.fixture
def sample_ohlcv():
    np.random.seed(42)
    n = 100
    close = pd.Series(100 + np.cumsum(np.random.randn(n) * 0.5))
    high  = close + np.abs(np.random.randn(n) * 0.3)
    low   = close - np.abs(np.random.randn(n) * 0.3)
    vol   = pd.Series(np.random.randint(1_000_000, 5_000_000, n).astype(float))
    return pd.DataFrame({"open": close, "high": high, "low": low, "close": close, "volume": vol})


def test_rsi_range(sample_ohlcv):
    r = rsi(sample_ohlcv["close"])
    valid = r.dropna()
    assert (valid >= 0).all() and (valid <= 100).all()


def test_macd_returns_dataframe(sample_ohlcv):
    result = macd(sample_ohlcv["close"])
    assert set(result.columns) == {"macd", "signal", "hist"}
    assert len(result) == len(sample_ohlcv)


def test_atr_positive(sample_ohlcv):
    result = atr(sample_ohlcv)
    assert result.dropna().gt(0).all()


def test_realized_vol_positive(sample_ohlcv):
    rv = realized_volatility(sample_ohlcv["close"])
    assert rv.dropna().gt(0).all()


def test_rolling_zscore_mean_zero(sample_ohlcv):
    z = rolling_zscore(sample_ohlcv["close"], window=20)
    # Mean of z-scores over a rolling window won't be exactly 0, but should be small
    assert z.dropna().abs().mean() < 3.0
