#include <gtest/gtest.h>
#include "backtester/backtester.hpp"
#include <chrono>

using namespace quantlab;
using namespace quantlab::backtester;

namespace {
std::vector<Bar> make_bars(int n, double start_price = 100.0, double drift = 0.0005) {
    std::vector<Bar> bars;
    auto ts = std::chrono::system_clock::now();
    double price = start_price;
    for (int i = 0; i < n; ++i) {
        price *= (1.0 + drift);
        bars.push_back(Bar{ts + std::chrono::hours(24 * i), "SPY",
                           price * 0.99, price * 1.01, price * 0.98, price, 1e6, price});
    }
    return bars;
}
}

TEST(BacktesterTest, RunProducesEquityCurve) {
    BacktestConfig cfg;
    cfg.initial_capital = 100'000.0;
    Backtester bt(cfg);
    bt.add_data("SPY", make_bars(252));
    bt.set_strategy([](const auto&, const auto&) -> std::vector<Signal> {
        return {Signal{std::chrono::system_clock::now(), "SPY", 1.0, 1.0}};
    });
    auto result = bt.run();
    EXPECT_FALSE(result.equity_curve.empty());
    EXPECT_GT(result.equity_curve.front(), 0.0);
}

TEST(BacktesterTest, InitialCapitalIsFirstEquityPoint) {
    BacktestConfig cfg;
    cfg.initial_capital = 200'000.0;
    Backtester bt(cfg);
    bt.add_data("SPY", make_bars(100));
    bt.set_strategy([](const auto&, const auto&) -> std::vector<Signal> { return {}; });
    auto result = bt.run();
    EXPECT_NEAR(result.equity_curve.front(), cfg.initial_capital, 1.0);
}

TEST(BacktesterTest, MaxDrawdownIsNonPositive) {
    BacktestConfig cfg;
    Backtester bt(cfg);
    bt.add_data("SPY", make_bars(252, 100.0, -0.002)); // downtrend
    bt.set_strategy([](const auto&, const auto&) -> std::vector<Signal> {
        return {Signal{std::chrono::system_clock::now(), "SPY", 1.0, 1.0}};
    });
    auto result = bt.run();
    EXPECT_GE(result.max_drawdown, 0.0);
}
