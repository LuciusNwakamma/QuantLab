"""Smoke check for QuantLab C++ pybind extension under WSL/Linux Python."""
from __future__ import annotations

import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from python.quantlab_cpp import OrderBook


def main() -> None:
    ob = OrderBook("AAPL")
    ob.add_order(
        id=1,
        symbol="AAPL",
        side="buy",
        order_type="limit",
        price=100.0,
        quantity=10.0,
        tif="GTC",
    )
    ob.add_order(
        id=2,
        symbol="AAPL",
        side="sell",
        order_type="limit",
        price=101.0,
        quantity=5.0,
        tif="GTC",
    )

    print(f"best_bid={ob.best_bid()} best_ask={ob.best_ask()} spread={ob.spread()}")


if __name__ == "__main__":
    main()
