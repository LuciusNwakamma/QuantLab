"""Statistical research: cointegration, regime detection, time-series models."""
from __future__ import annotations

import numpy as np
import pandas as pd
from statsmodels.tsa.stattools import coint, adfuller
from statsmodels.tsa.arima.model import ARIMA
from statsmodels.tsa.vector_ar.var_model import VAR


# ---- Cointegration ----

def test_cointegration(
    series_a: pd.Series,
    series_b: pd.Series,
    significance: float = 0.05,
) -> dict:
    """Engle-Granger cointegration test."""
    score, pvalue, critical_values = coint(series_a, series_b)
    return {
        "cointegrated": pvalue < significance,
        "pvalue": pvalue,
        "score": score,
        "critical_values": critical_values,
    }


def adf_test(series: pd.Series) -> dict:
    """Augmented Dickey-Fuller test for stationarity."""
    result = adfuller(series.dropna())
    return {
        "stationary": result[1] < 0.05,
        "adf_stat": result[0],
        "pvalue": result[1],
        "critical_values": result[4],
    }


# ---- Spread / Pairs Trading ----

def compute_hedge_ratio(series_a: pd.Series, series_b: pd.Series) -> float:
    """OLS hedge ratio beta for pairs trading."""
    from numpy.linalg import lstsq
    X = np.column_stack([series_b.values, np.ones(len(series_b))])
    beta, _, _, _ = lstsq(X, series_a.values, rcond=None)
    return float(beta[0])


def compute_spread(
    series_a: pd.Series,
    series_b: pd.Series,
    hedge_ratio: float | None = None,
) -> pd.Series:
    if hedge_ratio is None:
        hedge_ratio = compute_hedge_ratio(series_a, series_b)
    return series_a - hedge_ratio * series_b


# ---- Kalman Filter (rolling hedge ratio) ----

def kalman_hedge_ratio(
    series_a: pd.Series,
    series_b: pd.Series,
    delta: float = 1e-4,
) -> pd.Series:
    """Kalman-filter-based dynamic hedge ratio estimation."""
    n = len(series_a)
    x = series_b.values
    y = series_a.values

    R = np.zeros((2, 2))
    P = np.zeros((2, 2))
    Q = delta / (1 - delta) * np.eye(2)
    beta = np.zeros(2)

    hedge_ratios = np.full(n, np.nan)
    for t in range(n):
        xt = np.array([x[t], 1.0])
        R = P + Q
        K = R @ xt / (xt @ R @ xt + 1.0)  # Kalman gain
        resid = y[t] - xt @ beta
        beta = beta + K * resid
        P = (np.eye(2) - np.outer(K, xt)) @ R
        hedge_ratios[t] = beta[0]

    return pd.Series(hedge_ratios, index=series_a.index)


# ---- ARIMA ----

def fit_arima(
    series: pd.Series,
    order: tuple[int, int, int] = (1, 1, 1),
) -> dict:
    model = ARIMA(series.dropna(), order=order)
    result = model.fit()
    return {
        "model": result,
        "aic": result.aic,
        "bic": result.bic,
        "forecast": result.forecast(steps=5),
    }


# ---- VAR ----

def fit_var(df: pd.DataFrame, maxlags: int = 5) -> dict:
    model = VAR(df.dropna())
    result = model.fit(maxlags=maxlags, ic="aic")
    return {
        "model": result,
        "selected_order": result.k_ar,
        "forecast": result.forecast(df.values[-result.k_ar:], steps=5),
    }


# ---- Hurst Exponent ----

def hurst_exponent(series: pd.Series, max_lag: int = 100) -> float:
    """Estimate Hurst exponent via variance-of-increments method.

    H < 0.5 : mean-reverting
    H = 0.5 : random walk
    H > 0.5 : trending / persistent
    """
    ts = series.dropna().values
    n = len(ts)
    if n < 20:
        return 0.5
    max_lag = min(max_lag, n // 2)
    lags = range(2, max_lag)
    tau = []
    for lag in lags:
        diffs = ts[lag:] - ts[:-lag]
        tau.append(np.sqrt(np.var(diffs)))
    # Var(X_{t+lag} - X_t) ~ lag^{2H}  =>  std ~ lag^H
    log_lags = np.log(list(lags))
    log_tau  = np.log(tau)
    poly = np.polyfit(log_lags, log_tau, 1)
    return float(poly[0])
