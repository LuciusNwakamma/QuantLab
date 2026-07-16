"""Python unit tests for performance analytics."""
import numpy as np
import pandas as pd
import pytest

from python.performance_analytics import compute_metrics, rolling_sharpe


@pytest.fixture
def sample_returns():
    np.random.seed(1)
    return pd.Series(np.random.randn(252) * 0.01 + 0.0003)


def test_sharpe_reasonable(sample_returns):
    m = compute_metrics(sample_returns)
    assert -5.0 < m.sharpe_ratio < 5.0


def test_max_drawdown_non_positive(sample_returns):
    m = compute_metrics(sample_returns)
    assert m.max_drawdown <= 0.0


def test_win_rate_in_range(sample_returns):
    m = compute_metrics(sample_returns)
    assert 0.0 <= m.win_rate <= 1.0


def test_rolling_sharpe_length(sample_returns):
    rs = rolling_sharpe(sample_returns, window=60)
    assert len(rs) == len(sample_returns)


def test_zero_vol_returns():
    constant = pd.Series([0.001] * 100)
    m = compute_metrics(constant)
    assert m.sharpe_ratio == 0.0 or not np.isfinite(m.sharpe_ratio) or m.sharpe_ratio > 0
