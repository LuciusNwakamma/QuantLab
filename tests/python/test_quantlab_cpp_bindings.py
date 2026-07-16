"""Smoke tests for QuantLab C++ pybind11 bindings."""

import pytest

quantlab_cpp = pytest.importorskip("python.quantlab_cpp")


def test_order_book_basic_levels():
    ob = quantlab_cpp.OrderBook("AAPL")
    ob.add_order(id=1, symbol="AAPL", side="buy", order_type="limit", price=100.0, quantity=10.0)
    ob.add_order(id=2, symbol="AAPL", side="sell", order_type="limit", price=101.0, quantity=5.0)

    assert ob.best_bid() == 100.0
    assert ob.best_ask() == 101.0
    assert ob.spread() == 1.0

    bids = ob.bid_levels(depth=5)
    asks = ob.ask_levels(depth=5)
    assert len(bids) >= 1
    assert len(asks) >= 1
    assert bids[0]["price"] == 100.0
    assert asks[0]["price"] == 101.0


def test_order_book_batch_insertion_levels():
    ob = quantlab_cpp.OrderBook("AAPL")
    orders = [
        (1, "buy", 100.0, 10.0),
        (2, "buy", 99.5, 5.0),
        (3, "sell", 101.0, 7.0),
        (4, "sell", 101.5, 4.0),
    ]
    ob.add_orders_batch(orders=orders, symbol="AAPL", order_type="limit", tif="GTC")

    assert ob.best_bid() == 100.0
    assert ob.best_ask() == 101.0
    assert ob.spread() == 1.0


def test_risk_engine_basic_metrics_and_checks():
    limits = quantlab_cpp.RiskLimits()
    limits.max_position_notional = 1_000_000.0
    engine = quantlab_cpp.RiskEngine(limits)

    ok, violation = engine.check_order("AAPL", "buy", 100.0, 150.0)
    assert ok is True
    assert violation == quantlab_cpp.RiskViolation.None_

    engine.update_position("AAPL", "buy", 100.0, 150.0)
    engine.update_market_prices({"AAPL": 152.0})
    metrics = engine.compute_metrics()
    assert metrics.gross_exposure > 0.0

    positions = engine.positions()
    assert "AAPL" in positions
    assert positions["AAPL"]["quantity"] == 100.0


def test_backtester_constant_weight_run_smoke():
    cfg = quantlab_cpp.BacktestConfig()
    cfg.initial_capital = 100_000.0
    cfg.commission_per_share = 0.001
    cfg.commission_min = 0.0
    cfg.slippage_bps = 0.5

    bt = quantlab_cpp.Backtester(cfg)

    bars = []
    px = 100.0
    for _ in range(260):
        px *= 1.0006
        bars.append((px * 0.99, px * 1.01, px * 0.98, px, 1_000_000.0, px))

    bt.add_ohlcv_batch("SPY", bars)
    result = bt.run_constant_weight("SPY", 1.0)

    assert len(result.equity_curve) > 10
    assert result.total_trades > 0
    assert result.total_return > -1.0
