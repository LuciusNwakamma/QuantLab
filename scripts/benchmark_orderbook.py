"""Benchmark a parity Python order book model vs C++ pybind OrderBook.

Run in WSL after building bindings:
    wsl python3 "/mnt/c/Users/Lucius Nwakamma/QuantLab/scripts/benchmark_orderbook.py" --orders 200000
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from dataclasses import dataclass

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)


@dataclass(frozen=True)
class SyntheticOrder:
    id: int
    side: str
    price: float
    qty: float


class PythonParityOrderBook:
    """Python model approximating the same level aggregation path as C++ OrderBook."""

    def __init__(self) -> None:
        self.bids: dict[float, float] = {}
        self.asks: dict[float, float] = {}

    def add_order(self, side: str, price: float, qty: float) -> None:
        if side == "buy":
            self.bids[price] = self.bids.get(price, 0.0) + qty
        else:
            self.asks[price] = self.asks.get(price, 0.0) + qty

    def best_bid(self) -> float:
        return max(self.bids.keys()) if self.bids else 0.0

    def best_ask(self) -> float:
        return min(self.asks.keys()) if self.asks else 0.0

    def spread(self) -> float:
        bid = self.best_bid()
        ask = self.best_ask()
        return ask - bid if bid and ask else 0.0


def make_non_crossing_flow(orders: int) -> list[SyntheticOrder]:
    """Create deterministic order flow where buys/asks do not cross.

    This isolates insertion + level maintenance cost for both implementations.
    """
    flow: list[SyntheticOrder] = []
    for i in range(orders):
        is_buy = (i % 2 == 0)
        side = "buy" if is_buy else "sell"
        # Keep bids below asks to avoid matching path differences.
        price = (99.00 + (i % 50) * 0.01) if is_buy else (101.00 + (i % 50) * 0.01)
        qty = 1.0 + (i % 10) * 0.1
        flow.append(SyntheticOrder(id=i + 1, side=side, price=price, qty=qty))
    return flow


def bench_python_parity(flow: list[SyntheticOrder]) -> tuple[float, float, float, float]:
    ob = PythonParityOrderBook()

    start = time.perf_counter()
    for order in flow:
        ob.add_order(order.side, order.price, order.qty)

    elapsed = time.perf_counter() - start
    return elapsed, ob.best_bid(), ob.best_ask(), ob.spread()


def bench_cpp_orderbook_single(flow: list[SyntheticOrder]) -> tuple[float, float, float, float]:
    from python.quantlab_cpp import OrderBook

    ob = OrderBook("AAPL")
    start = time.perf_counter()
    for order in flow:
        ob.add_order(
            id=order.id,
            symbol="AAPL",
            side=order.side,
            order_type="limit",
            price=order.price,
            quantity=order.qty,
            tif="GTC",
        )

    best_bid = ob.best_bid()
    best_ask = ob.best_ask()
    spread = ob.spread()
    elapsed = time.perf_counter() - start
    return elapsed, best_bid, best_ask, spread


def bench_cpp_orderbook_batch(flow: list[SyntheticOrder]) -> tuple[float, float, float, float]:
    from python.quantlab_cpp import OrderBook

    ob = OrderBook("AAPL")
    payload = [(o.id, o.side, o.price, o.qty) for o in flow]

    start = time.perf_counter()
    ob.add_orders_batch(payload, symbol="AAPL", order_type="limit", tif="GTC")
    best_bid = ob.best_bid()
    best_ask = ob.best_ask()
    spread = ob.spread()
    elapsed = time.perf_counter() - start
    return elapsed, best_bid, best_ask, spread


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark Python vs C++ OrderBook")
    parser.add_argument("--orders", type=int, default=200_000)
    args = parser.parse_args()

    flow = make_non_crossing_flow(args.orders)

    py_t, py_bid, py_ask, py_spread = bench_python_parity(flow)
    cpp_single_t, cpp_single_bid, cpp_single_ask, cpp_single_spread = bench_cpp_orderbook_single(flow)
    cpp_batch_t, cpp_batch_bid, cpp_batch_ask, cpp_batch_spread = bench_cpp_orderbook_batch(flow)

    print("Order count:", args.orders)
    print(f"Python  : {py_t:.4f}s  bid={py_bid:.4f} ask={py_ask:.4f} spread={py_spread:.4f}")
    print(f"C++ single: {cpp_single_t:.4f}s  bid={cpp_single_bid:.4f} ask={cpp_single_ask:.4f} spread={cpp_single_spread:.4f}")
    print(f"C++ batch : {cpp_batch_t:.4f}s  bid={cpp_batch_bid:.4f} ask={cpp_batch_ask:.4f} spread={cpp_batch_spread:.4f}")

    if abs(py_bid - cpp_single_bid) > 1e-9 or abs(py_ask - cpp_single_ask) > 1e-9:
        print("WARNING: parity mismatch between Python and C++ outputs")
    if abs(py_bid - cpp_batch_bid) > 1e-9 or abs(py_ask - cpp_batch_ask) > 1e-9:
        print("WARNING: parity mismatch between Python and C++ batch outputs")

    if cpp_single_t > 0:
        print(f"Speedup (single): {py_t / cpp_single_t:.2f}x")
    if cpp_batch_t > 0:
        print(f"Speedup (batch) : {py_t / cpp_batch_t:.2f}x")


if __name__ == "__main__":
    main()
