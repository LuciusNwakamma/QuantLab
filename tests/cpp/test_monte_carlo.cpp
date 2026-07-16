#include <gtest/gtest.h>
#include "monte_carlo/monte_carlo.hpp"
#include <cmath>

using namespace quantlab::monte_carlo;

TEST(MonteCarloTest, GBMProducesCorrectNumberOfPaths) {
    MCConfig cfg;
    cfg.simulations  = 1000;
    cfg.horizon_days = 252;
    cfg.seed         = 42;
    MonteCarloSimulator sim(cfg);
    auto result = sim.simulate_gbm(100.0, 0.10, 0.20);
    EXPECT_EQ(result.final_values.size(), 1000u);
}

TEST(MonteCarloTest, GBMMeanApproximatesAnalytical) {
    MCConfig cfg;
    cfg.simulations  = 50000;
    cfg.horizon_days = 252;
    cfg.seed         = 42;
    MonteCarloSimulator sim(cfg);
    double S0 = 100.0, mu = 0.10, sigma = 0.20;
    auto result = sim.simulate_gbm(S0, mu, sigma);
    // E[S_T] = S0 * exp(mu * T)
    double expected = S0 * std::exp(mu * 1.0);
    EXPECT_NEAR(result.mean, expected, expected * 0.03); // within 3%
}

TEST(MonteCarloTest, VaR95LessThanVaR99) {
    MCConfig cfg; cfg.simulations = 5000; cfg.seed = 1;
    MonteCarloSimulator sim(cfg);
    auto result = sim.simulate_gbm(100.0, 0.05, 0.25);
    EXPECT_LE(result.var_95, result.var_99);
}

TEST(MonteCarloTest, CholeskyReconstructsCovMatrix) {
    std::vector<std::vector<double>> cov = {{1.0, 0.5}, {0.5, 1.0}};
    auto L = MonteCarloSimulator::cholesky(cov);
    // L * L^T should equal cov
    EXPECT_NEAR(L[0][0] * L[0][0], 1.0, 1e-9);
    EXPECT_NEAR(L[1][0] * L[1][0] + L[1][1] * L[1][1], 1.0, 1e-9);
    EXPECT_NEAR(L[1][0] * L[0][0], 0.5, 1e-9);
}
