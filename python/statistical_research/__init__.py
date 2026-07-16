"""Statistical research module."""
from .stat_models import (
    test_cointegration,
    adf_test,
    compute_hedge_ratio,
    compute_spread,
    kalman_hedge_ratio,
    fit_arima,
    fit_var,
    hurst_exponent,
)

__all__ = [
    "test_cointegration", "adf_test", "compute_hedge_ratio",
    "compute_spread", "kalman_hedge_ratio", "fit_arima", "fit_var",
    "hurst_exponent",
]
