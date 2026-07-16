"""Benchmark Python constant-weight backtest loop vs C++ Backtester binding.

Run in WSL after building bindings:
    wsl python3 "/mnt/c/Users/Lucius Nwakamma/QuantLab/scripts/benchmark_backtester_binding.py" --bars 5000
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
class BacktestCfg:
    initial_capital: float = 1_000_000.0
    commission_per_share: float = 0.005
    commission_min: float = 1.0
    slippage_bps: float = 2.0


def make_ohlcv_bars(count: int) -> list[tuple[float, float, float, float, float, float]]:
    bars: list[tuple[float, float, float, float, float, float]] = []
    close = 100.0
    for i in range(count):
        close *= 1.0003 + ((i % 7) - 3) * 0.00003
        open_ = close * (1.0 - 0.0006)
        high = close * (1.0 + 0.0012)
        low = close * (1.0 - 0.0013)
        volume = 900_000.0 + (i % 50) * 2_500.0
        bars.append((open_, high, low, close, volume, close))
    return bars


def python_constant_weight_backtest(
    bars: list[tuple[float, float, float, float, float, float]],
    cfg: BacktestCfg,
    target_weight: float,
) -> tuple[float, float, int]:
    capital = cfg.initial_capital
    position_qty = 0.0
    equity_curve = [capital]
    trade_count = 0

    start = time.perf_counter()
    for i in range(1, len(bars)):
        close = bars[i][3]

        target_value = capital * target_weight
        current_value = position_qty * close
        delta_value = target_value - current_value

        if abs(delta_value) >= 1.0:
            side = 1.0 if delta_value > 0 else -1.0
            qty = abs(delta_value) / close
            slip = close * (cfg.slippage_bps / 10000.0)
            fill = close + slip if side > 0 else close - slip
            commission = max(cfg.commission_min, qty * cfg.commission_per_share)
            slippage_cost = abs(fill - close) * qty

            if side > 0:
                capital -= (fill * qty) + commission + slippage_cost
                position_qty += qty
            else:
                capital += (fill * qty) - commission - slippage_cost
                position_qty -= qty
            trade_count += 1

        equity_curve.append(capital + position_qty * close)

    elapsed = time.perf_counter() - start
    total_return = (equity_curve[-1] - equity_curve[0]) / equity_curve[0] if equity_curve else 0.0
    return elapsed, total_return, trade_count


def cpp_constant_weight_backtest(
    bars: list[tuple[float, float, float, float, float, float]],
    cfg: BacktestCfg,
    target_weight: float,
) -> tuple[float, float, int]:
    from python.quantlab_cpp import BacktestConfig, Backtester

    native_cfg = BacktestConfig()
    native_cfg.initial_capital = cfg.initial_capital
    native_cfg.commission_per_share = cfg.commission_per_share
    native_cfg.commission_min = cfg.commission_min
    native_cfg.slippage_bps = cfg.slippage_bps

    bt = Backtester(native_cfg)
    bt.add_ohlcv_batch("SPY", bars)

    start = time.perf_counter()
    result = bt.run_constant_weight("SPY", target_weight)
    elapsed = time.perf_counter() - start
    return elapsed, result.total_return, result.total_trades


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark Python loop vs C++ Backtester binding")
    parser.add_argument("--bars", type=int, default=5000)
    parser.add_argument("--target-weight", type=float, default=1.0)
    args = parser.parse_args()

    bars = make_ohlcv_bars(args.bars)
    cfg = BacktestCfg()

    py_t, py_ret, py_trades = python_constant_weight_backtest(bars, cfg, args.target_weight)
    cpp_t, cpp_ret, cpp_trades = cpp_constant_weight_backtest(bars, cfg, args.target_weight)

    print("Bar count:", args.bars)
    print(f"Python loop : {py_t:.4f}s  return={py_ret:.4%} trades={py_trades}")
    print(f"C++ binding : {cpp_t:.4f}s  return={cpp_ret:.4%} trades={cpp_trades}")
    if cpp_t > 0:
        print(f"Speedup (C++ vs Python): {py_t / cpp_t:.2f}x")


if __name__ == "__main__":
    main()
