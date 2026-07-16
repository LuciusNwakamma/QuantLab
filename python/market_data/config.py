"""Shared configuration loading for market data providers."""
from __future__ import annotations

from dataclasses import dataclass
import os

try:
    from dotenv import load_dotenv
except ImportError:  # pragma: no cover - optional dependency fallback
    def load_dotenv() -> bool:
        return False


@dataclass(frozen=True)
class MarketDataCredentials:
    """API credentials for external data providers."""

    alpaca_api_key: str = ""
    alpaca_secret_key: str = ""
    polygon_api_key: str = ""

    @classmethod
    def from_env(cls) -> "MarketDataCredentials":
        # Load .env if present, then read process env.
        load_dotenv()
        polygon_key = (
            os.getenv("POLYGON_API_KEY", "")
            or os.getenv("POLYGON_APT_KEY", "")  # common typo fallback
            or os.getenv("POLYGON_KEY", "")
        )
        return cls(
            alpaca_api_key=os.getenv("ALPACA_API_KEY", ""),
            alpaca_secret_key=os.getenv("ALPACA_SECRET_KEY", ""),
            polygon_api_key=polygon_key,
        )

    def require_alpaca(self) -> None:
        if not self.alpaca_api_key or not self.alpaca_secret_key:
            raise ValueError(
                "Missing Alpaca credentials. Set ALPACA_API_KEY and ALPACA_SECRET_KEY."
            )

    def require_polygon(self) -> None:
        if not self.polygon_api_key:
            raise ValueError(
                "Missing Polygon API key. Set POLYGON_API_KEY. "
                "PowerShell: $env:POLYGON_API_KEY='your_key'"
            )
