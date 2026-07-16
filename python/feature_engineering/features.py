"""Institution-level feature engineering."""
from __future__ import annotations

import numpy as np
import pandas as pd


def vwap(df: pd.DataFrame) -> pd.Series:
    """Volume-weighted average price."""
    typical = (df["high"] + df["low"] + df["close"]) / 3.0
    return (typical * df["volume"]).cumsum() / df["volume"].cumsum()


def rsi(close: pd.Series, period: int = 14) -> pd.Series:
    delta = close.diff()
    gain = delta.clip(lower=0).rolling(period).mean()
    loss = (-delta.clip(upper=0)).rolling(period).mean()
    rs = gain / loss.replace(0, np.nan)
    return 100.0 - 100.0 / (1.0 + rs)


def macd(
    close: pd.Series,
    fast: int = 12,
    slow: int = 26,
    signal: int = 9,
) -> pd.DataFrame:
    ema_fast = close.ewm(span=fast, adjust=False).mean()
    ema_slow = close.ewm(span=slow, adjust=False).mean()
    macd_line = ema_fast - ema_slow
    signal_line = macd_line.ewm(span=signal, adjust=False).mean()
    histogram = macd_line - signal_line
    return pd.DataFrame({"macd": macd_line, "signal": signal_line, "hist": histogram})


def atr(df: pd.DataFrame, period: int = 14) -> pd.Series:
    hl = df["high"] - df["low"]
    hc = (df["high"] - df["close"].shift()).abs()
    lc = (df["low"] - df["close"].shift()).abs()
    tr = pd.concat([hl, hc, lc], axis=1).max(axis=1)
    return tr.rolling(period).mean()


def realized_volatility(close: pd.Series, window: int = 21) -> pd.Series:
    log_ret = np.log(close / close.shift(1))
    return log_ret.rolling(window).std() * np.sqrt(252)


def rolling_zscore(series: pd.Series, window: int = 20) -> pd.Series:
    mean = series.rolling(window).mean()
    std = series.rolling(window).std()
    return (series - mean) / std.replace(0, np.nan)


def order_imbalance(bid_vol: pd.Series, ask_vol: pd.Series) -> pd.Series:
    total = bid_vol + ask_vol
    return (bid_vol - ask_vol) / total.replace(0, np.nan)


def rolling_correlation(
    series_a: pd.Series,
    series_b: pd.Series,
    window: int = 60,
) -> pd.Series:
    return series_a.rolling(window).corr(series_b)


def lagged_returns(close: pd.Series, lags: list[int] | None = None) -> pd.DataFrame:
    if lags is None:
        lags = [1, 2, 3, 5, 10, 21]
    ret = close.pct_change()
    return pd.DataFrame({f"lag_{l}": ret.shift(l) for l in lags})


def add_all_features(df: pd.DataFrame) -> pd.DataFrame:
    """Compute and append all standard features to an OHLCV DataFrame."""
    df = df.copy()
    df["vwap"]           = vwap(df)
    df["rsi_14"]         = rsi(df["close"])
    df["atr_14"]         = atr(df)
    df["realized_vol"]   = realized_volatility(df["close"])
    df["zscore_20"]      = rolling_zscore(df["close"])

    macd_df = macd(df["close"])
    df["macd"]           = macd_df["macd"]
    df["macd_signal"]    = macd_df["signal"]
    df["macd_hist"]      = macd_df["hist"]

    lag_df = lagged_returns(df["close"])
    df = pd.concat([df, lag_df], axis=1)
    return df
