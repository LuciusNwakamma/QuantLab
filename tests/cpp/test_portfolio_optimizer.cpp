#include <gtest/gtest.h>
#include "portfolio_optimizer/portfolio_optimizer.hpp"
#include <numeric>
#include <random>

using namespace quantlab::portfolio_optimizer;

namespace {
OptimizationInput make_input(int n_assets = 3, int n_periods = 100) {
    OptimizationInput input;
    for (int i = 0; i < n_assets; ++i)
        input.symbols.push_back("A" + std::to_string(i));

    std::mt19937 rng(42);
    std::normal_distribution<double> nd(0.0001, 0.01);
    for (int t = 0; t < n_periods; ++t) {
        std::vector<double> row;
        for (int a = 0; a < n_assets; ++a) row.push_back(nd(rng));
        input.returns_matrix.push_back(row);
    }
    return input;
}
}

TEST(PortfolioOptimizerTest, WeightsSumToOne) {
    PortfolioOptimizer opt;
    auto input = make_input();
    auto result = opt.optimize(input, OptimizationMethod::MaximumSharpe);
    double sum = std::accumulate(result.weights.begin(), result.weights.end(), 0.0);
    EXPECT_NEAR(sum, 1.0, 1e-6);
}

TEST(PortfolioOptimizerTest, AllWeightsNonNegative) {
    PortfolioOptimizer opt;
    auto input = make_input();
    auto result = opt.optimize(input, OptimizationMethod::MaximumSharpe);
    for (double w : result.weights) EXPECT_GE(w, -1e-9);
}

TEST(PortfolioOptimizerTest, EqualRiskContributionWeightsSumToOne) {
    PortfolioOptimizer opt;
    auto input = make_input(4, 120);
    auto result = opt.optimize(input, OptimizationMethod::EqualRiskContribution);
    double sum = std::accumulate(result.weights.begin(), result.weights.end(), 0.0);
    EXPECT_NEAR(sum, 1.0, 1e-6);
}

TEST(PortfolioOptimizerTest, SampleCovarianceIsSymmetric) {
    auto input = make_input(3, 50);
    auto cov = PortfolioOptimizer::sample_covariance(input.returns_matrix);
    for (std::size_t i = 0; i < cov.size(); ++i)
        for (std::size_t j = 0; j < cov.size(); ++j)
            EXPECT_NEAR(cov[i][j], cov[j][i], 1e-12);
}
