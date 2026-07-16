"""Market data download and streaming adapters."""
from .base import BaseDataProvider, OHLCV
from .config import MarketDataCredentials

try:
	from .yfinance_provider import YFinanceProvider
except ImportError:  # pragma: no cover - optional dependency not installed
	YFinanceProvider = None

try:
	from .alpaca_provider import AlpacaProvider
except ImportError:  # pragma: no cover - optional dependency not installed
	AlpacaProvider = None

try:
	from .polygon_provider import PolygonProvider
except ImportError:  # pragma: no cover - optional dependency not installed
	PolygonProvider = None

__all__ = [
	"BaseDataProvider",
	"OHLCV",
	"MarketDataCredentials",
	"YFinanceProvider",
	"AlpacaProvider",
	"PolygonProvider",
]
