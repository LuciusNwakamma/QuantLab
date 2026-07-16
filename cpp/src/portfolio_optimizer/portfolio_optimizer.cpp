#include "portfolio_optimizer/portfolio_optimizer.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdexcept>

namespace quantlab::portfolio_optimizer {

// ---- Utility ----

static std::vector<double> compute_mean_returns(
    const std::vector<std::vector<double>>& R)
{
    if (R.empty()) return {};
    std::size_t T = R.size(), N = R[0].size();
    std::vector<double> mu(N, 0.0);
    for (auto& row : R)
        for (std::size_t j = 0; j < N; ++j)
            mu[j] += row[j];
    for (auto& m : mu) m /= static_cast<double>(T);
    return mu;
}

std::vector<std::vector<double>>
PortfolioOptimizer::sample_covariance(const std::vector<std::vector<double>>& R) {
    if (R.empty()) return {};
    std::size_t T = R.size(), N = R[0].size();
    auto mu = compute_mean_returns(R);
    std::vector<std::vector<double>> cov(N, std::vector<double>(N, 0.0));
    for (auto& row : R)
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                cov[i][j] += (row[i] - mu[i]) * (row[j] - mu[j]);
    double denom = static_cast<double>(T - 1);
    for (auto& row : cov)
        for (auto& v : row)
            v /= denom;
    return cov;
}

std::vector<std::vector<double>>
PortfolioOptimizer::ledoit_wolf_shrinkage(const std::vector<std::vector<double>>& R) {
    auto S = sample_covariance(R);
    std::size_t N = S.size();
    // Target: scaled identity (mu * I)
    double mu = 0.0;
    for (std::size_t i = 0; i < N; ++i) mu += S[i][i];
    mu /= static_cast<double>(N);
    // Simple constant shrinkage coefficient (Oracle approximation)
    double alpha = 0.1;
    for (std::size_t i = 0; i < N; ++i) {
        S[i][i] = (1.0 - alpha) * S[i][i] + alpha * mu;
        for (std::size_t j = 0; j < N; ++j)
            if (i != j) S[i][j] *= (1.0 - alpha);
    }
    return S;
}

// ---- Equal-weight / Risk Parity helpers ----

OptimizationResult PortfolioOptimizer::equal_risk_contribution(
    const OptimizationInput& input) const
{
    std::size_t N = input.symbols.size();
    auto cov = sample_covariance(input.returns_matrix);

    // Iterative ERC (simple gradient descent)
    std::vector<double> w(N, 1.0 / N);
    for (int iter = 0; iter < 500; ++iter) {
        // Compute marginal risk contributions
        std::vector<double> mrc(N, 0.0);
        double port_var = 0.0;
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j) {
                mrc[i]   += cov[i][j] * w[j];
                port_var += w[i] * cov[i][j] * w[j];
            }
        // Gradient step: reduce positions with high RC
        double rc_sum = 0.0;
        for (std::size_t i = 0; i < N; ++i) rc_sum += w[i] * mrc[i];
        double target_rc = rc_sum / N;
        for (std::size_t i = 0; i < N; ++i) {
            double rc = w[i] * mrc[i];
            w[i] *= std::sqrt(target_rc / std::max(rc, 1e-12));
        }
        // Normalize
        double wsum = std::accumulate(w.begin(), w.end(), 0.0);
        for (auto& wi : w) wi /= wsum;
    }

    auto mu = compute_mean_returns(input.returns_matrix);
    double port_ret = 0.0, port_var = 0.0;
    for (std::size_t i = 0; i < N; ++i) port_ret += w[i] * mu[i];
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j)
            port_var += w[i] * cov[i][j] * w[j];
    double port_vol = std::sqrt(std::max(port_var, 0.0)) * std::sqrt(252.0);
    double ann_ret  = port_ret * 252.0;

    OptimizationResult r;
    r.weights             = w;
    r.expected_return     = ann_ret;
    r.expected_volatility = port_vol;
    r.sharpe_ratio        = port_vol > 1e-12 ? (ann_ret - input.risk_free_rate) / port_vol : 0.0;
    return r;
}

OptimizationResult PortfolioOptimizer::min_variance(const OptimizationInput& input) const {
    // Closed-form via quadratic programming approximation (equal weight as fallback)
    return equal_risk_contribution(input); // placeholder; replace with QP solver
}

OptimizationResult PortfolioOptimizer::max_sharpe(const OptimizationInput& input) const {
    // Tangency portfolio via gradient ascent
    std::size_t N = input.symbols.size();
    auto cov = sample_covariance(input.returns_matrix);
    auto mu  = compute_mean_returns(input.returns_matrix);

    std::vector<double> w(N, 1.0 / N);
    double lr = 0.01;
    for (int iter = 0; iter < 1000; ++iter) {
        double port_ret = 0.0, port_var = 0.0;
        for (std::size_t i = 0; i < N; ++i) port_ret += w[i] * mu[i];
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                port_var += w[i] * cov[i][j] * w[j];
        double port_vol = std::sqrt(std::max(port_var, 1e-12));
        double rf_daily = input.risk_free_rate / 252.0;
        double excess   = port_ret - rf_daily;

        std::vector<double> grad(N);
        for (std::size_t i = 0; i < N; ++i) {
            double d_ret = mu[i];
            double d_var = 0.0;
            for (std::size_t j = 0; j < N; ++j) d_var += cov[i][j] * w[j];
            d_var *= 2.0;
            // d(Sharpe)/dw = (d_ret * vol - excess * d_var / (2*vol)) / vol^2
            grad[i] = (d_ret * port_vol - excess * d_var / (2.0 * port_vol))
                      / (port_vol * port_vol);
        }
        for (std::size_t i = 0; i < N; ++i) w[i] += lr * grad[i];
        // Project onto simplex (non-negative, sum-to-1)
        for (auto& wi : w) wi = std::max(wi, 0.0);
        double wsum = std::accumulate(w.begin(), w.end(), 0.0);
        if (wsum < 1e-12) { std::fill(w.begin(), w.end(), 1.0 / N); continue; }
        for (auto& wi : w) wi /= wsum;
    }

    double port_ret = 0.0, port_var = 0.0;
    for (std::size_t i = 0; i < N; ++i) port_ret += w[i] * mu[i];
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j)
            port_var += w[i] * cov[i][j] * w[j];

    OptimizationResult r;
    r.weights             = w;
    r.expected_return     = port_ret * 252.0;
    r.expected_volatility = std::sqrt(port_var) * std::sqrt(252.0);
    r.sharpe_ratio        = r.expected_volatility > 1e-12
        ? (r.expected_return - input.risk_free_rate) / r.expected_volatility : 0.0;
    return r;
}

OptimizationResult PortfolioOptimizer::risk_parity(const OptimizationInput& input) const {
    return equal_risk_contribution(input);
}

OptimizationResult PortfolioOptimizer::mean_variance(const OptimizationInput& input) const {
    return max_sharpe(input);
}

OptimizationResult PortfolioOptimizer::black_litterman(const OptimizationInput& input) const {
    // Simplified BL: blends market-implied returns with views
    // Full implementation requires market cap weights; use equal weights as prior
    std::size_t N = input.symbols.size();
    std::vector<double> pi(N, 0.0); // prior expected returns
    auto cov = sample_covariance(input.returns_matrix);

    // Implied returns: pi = tau * Sigma * w_market (use equal weights)
    double tau = 0.025;
    std::vector<double> w_eq(N, 1.0 / N);
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j)
            pi[i] += tau * cov[i][j] * w_eq[j];

    // If views provided, blend: mu_bl = pi + tau*Sigma*P'*(P*tau*Sigma*P' + Omega)^-1*(Q - P*pi)
    // Simplified: if no views, use prior returns
    OptimizationInput inp2 = input;
    inp2.expected_returns = pi;
    return max_sharpe(inp2);
}

OptimizationResult PortfolioOptimizer::hierarchical_risk_parity(
    const OptimizationInput& input) const
{
    // Simplified HRP: inverse-volatility weighted
    std::size_t N = input.symbols.size();
    auto cov = sample_covariance(input.returns_matrix);
    std::vector<double> w(N);
    double inv_vol_sum = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        w[i] = 1.0 / std::max(std::sqrt(cov[i][i]), 1e-12);
        inv_vol_sum += w[i];
    }
    for (auto& wi : w) wi /= inv_vol_sum;

    OptimizationResult r;
    r.weights = w;
    auto mu = compute_mean_returns(input.returns_matrix);
    for (std::size_t i = 0; i < N; ++i) r.expected_return += w[i] * mu[i];
    r.expected_return *= 252.0;
    double port_var = 0.0;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j)
            port_var += w[i] * cov[i][j] * w[j];
    r.expected_volatility = std::sqrt(port_var) * std::sqrt(252.0);
    r.sharpe_ratio = r.expected_volatility > 1e-12
        ? (r.expected_return - input.risk_free_rate) / r.expected_volatility : 0.0;
    return r;
}

OptimizationResult PortfolioOptimizer::optimize(const OptimizationInput& input,
                                                 OptimizationMethod method) {
    switch (method) {
        case OptimizationMethod::MeanVariance:         return mean_variance(input);
        case OptimizationMethod::BlackLitterman:       return black_litterman(input);
        case OptimizationMethod::RiskParity:           return risk_parity(input);
        case OptimizationMethod::HierarchicalRiskParity: return hierarchical_risk_parity(input);
        case OptimizationMethod::MinimumVariance:      return min_variance(input);
        case OptimizationMethod::MaximumSharpe:        return max_sharpe(input);
        case OptimizationMethod::EqualRiskContribution: return equal_risk_contribution(input);
    }
    return {};
}

} // namespace quantlab::portfolio_optimizer
