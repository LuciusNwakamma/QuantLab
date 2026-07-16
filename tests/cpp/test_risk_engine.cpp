#include <gtest/gtest.h>
#include "risk_engine/risk_engine.hpp"

using namespace quantlab;
using namespace quantlab::risk_engine;

TEST(RiskEngineTest, KellyCriterionBasic) {
    RiskEngine engine;
    // 60% win rate, avg win $1, avg loss $1 -> kelly = 0.20
    double k = engine.kelly_fraction(0.60, 1.0, 1.0);
    EXPECT_NEAR(k, 0.20, 1e-9);
}

TEST(RiskEngineTest, HistoricalVaR) {
    RiskEngine engine;
    std::vector<double> returns(100);
    for (int i = 0; i < 100; ++i) returns[i] = (i - 50) / 1000.0; // -5% to +4.9%
    double var = engine.historical_var(returns, 0.95);
    EXPECT_GT(var, 0.0);
}

TEST(RiskEngineTest, HistoricalCVaRGeqVaR) {
    RiskEngine engine;
    std::vector<double> returns;
    for (int i = 0; i < 500; ++i) returns.push_back((i - 250) / 5000.0);
    double var  = engine.historical_var(returns);
    double cvar = engine.historical_cvar(returns);
    EXPECT_GE(cvar, var);
}

TEST(RiskEngineTest, PositionUpdateTracked) {
    RiskEngine engine;
    engine.update_position("AAPL", Side::Buy, 100.0, 150.0);
    auto& positions = engine.positions();
    ASSERT_TRUE(positions.count("AAPL") > 0);
    EXPECT_NEAR(positions.at("AAPL").quantity, 100.0, 1e-9);
    EXPECT_NEAR(positions.at("AAPL").avg_cost, 150.0, 1e-9);
}

TEST(RiskEngineTest, OrderCheckPassesWithinLimits) {
    RiskLimits limits;
    limits.max_position_notional = 1'000'000.0;
    RiskEngine engine(limits);
    auto [ok, violation] = engine.check_order("AAPL", Side::Buy, 100.0, 150.0);
    EXPECT_TRUE(ok);
    EXPECT_EQ(violation, RiskViolation::None);
}

TEST(RiskEngineTest, OrderCheckFailsWhenExceedsPositionLimit) {
    RiskLimits limits;
    limits.max_position_notional = 10.0; // very small limit
    RiskEngine engine(limits);
    auto [ok, violation] = engine.check_order("AAPL", Side::Buy, 100.0, 150.0);
    EXPECT_FALSE(ok);
    EXPECT_EQ(violation, RiskViolation::PositionLimitBreached);
}
