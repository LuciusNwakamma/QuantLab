"""Python unit tests for statistical research."""
import numpy as np
import pandas as pd
import pytest

from python.statistical_research import hurst_exponent, adf_test, compute_spread, compute_hedge_ratio


def test_hurst_trending_series():
    """Trending series should have H > 0.5."""
    trend = pd.Series(np.cumsum(np.ones(500) * 0.01))
    h = hurst_exponent(trend)
    assert h > 0.5


def test_hurst_mean_reverting_series():
    """Mean-reverting (OU) series should have H < 0.5."""
    np.random.seed(42)
    n, theta, mu, sigma = 2000, 0.8, 0.0, 0.1  # strong mean reversion
    x = np.zeros(n)
    for t in range(1, n):
        x[t] = x[t-1] + theta * (mu - x[t-1]) + sigma * np.random.randn()
    h = hurst_exponent(pd.Series(x))
    assert h < 0.5


def test_adf_stationary_series():
    np.random.seed(0)
    stationary = pd.Series(np.random.randn(200))
    result = adf_test(stationary)
    assert result["stationary"]


def test_hedge_ratio_linear():
    x = pd.Series(np.arange(1, 101, dtype=float))
    y = 2.5 * x + np.random.randn(100) * 0.01
    beta = compute_hedge_ratio(pd.Series(y), x)
    assert abs(beta - 2.5) < 0.1


def test_compute_spread_stationary():
    x = pd.Series(np.arange(1, 201, dtype=float))
    y = 3.0 * x + np.random.randn(200) * 0.1
    beta = compute_hedge_ratio(pd.Series(y), x)
    spread = compute_spread(pd.Series(y), x, beta)
    result = adf_test(spread)
    assert result["stationary"]
