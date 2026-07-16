#include "monte_carlo/monte_carlo.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdexcept>

namespace quantlab::monte_carlo {

MonteCarloSimulator::MonteCarloSimulator(MCConfig cfg)
    : cfg_(cfg), rng_(cfg.seed) {}

MCResult MonteCarloSimulator::simulate_gbm(double S0, double mu, double sigma) const {
    std::normal_distribution<double> norm(0.0, 1.0);
    std::vector<double> finals;
    finals.reserve(cfg_.simulations);

    std::vector<std::vector<double>> sample_paths;
    int store_paths = std::min(cfg_.simulations, 100);

    for (int s = 0; s < cfg_.simulations; ++s) {
        double S = S0;
        std::vector<double> path;
        bool record = s < store_paths;
        if (record) path.push_back(S);

        for (int t = 0; t < cfg_.horizon_days; ++t) {
            double z = norm(rng_);
            S *= std::exp((mu - 0.5 * sigma * sigma) * cfg_.dt + sigma * std::sqrt(cfg_.dt) * z);
            if (record) path.push_back(S);
        }
        finals.push_back(S);
        if (record) sample_paths.push_back(std::move(path));
    }

    MCResult result;
    result.final_values = finals;
    result.paths        = sample_paths;

    // Stats
    result.mean    = std::accumulate(finals.begin(), finals.end(), 0.0) / finals.size();
    double sq_sum  = 0.0;
    for (double v : finals) sq_sum += (v - result.mean) * (v - result.mean);
    result.std_dev = std::sqrt(sq_sum / finals.size());

    std::vector<double> returns;
    returns.reserve(finals.size());
    for (double v : finals) returns.push_back((v - S0) / S0);
    std::sort(returns.begin(), returns.end());

    auto var_at = [&](double conf) -> double {
        std::size_t idx = static_cast<std::size_t>((1.0 - conf) * returns.size());
        return -returns[std::min(idx, returns.size() - 1)];
    };
    auto cvar_at = [&](double conf) -> double {
        std::size_t cut = static_cast<std::size_t>((1.0 - conf) * returns.size());
        if (cut == 0) return -returns[0];
        double s = std::accumulate(returns.begin(), returns.begin() + cut, 0.0);
        return -s / static_cast<double>(cut);
    };

    result.var_95  = var_at(0.95);
    result.cvar_95 = cvar_at(0.95);
    result.var_99  = var_at(0.99);
    result.cvar_99 = cvar_at(0.99);
    result.prob_loss = static_cast<double>(
        std::count_if(finals.begin(), finals.end(), [S0](double v){ return v < S0; })
    ) / finals.size();

    return result;
}

std::vector<std::vector<double>>
MonteCarloSimulator::cholesky(const std::vector<std::vector<double>>& cov) {
    std::size_t n = cov.size();
    std::vector<std::vector<double>> L(n, std::vector<double>(n, 0.0));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double sum = cov[i][j];
            for (std::size_t k = 0; k < j; ++k) sum -= L[i][k] * L[j][k];
            if (i == j) {
                if (sum < 0) throw std::runtime_error("Matrix not positive definite");
                L[i][j] = std::sqrt(sum);
            } else {
                L[i][j] = (L[j][j] > 1e-12) ? sum / L[j][j] : 0.0;
            }
        }
    }
    return L;
}

MCResult MonteCarloSimulator::simulate_portfolio(
    const std::vector<double>& initial_prices,
    const std::vector<double>& drifts,
    const std::vector<double>& vols,
    const std::vector<std::vector<double>>& corr_matrix,
    const std::vector<double>& weights) const
{
    std::size_t n_assets = initial_prices.size();
    // Build cov matrix: sigma_i * sigma_j * rho_ij
    std::vector<std::vector<double>> cov(n_assets, std::vector<double>(n_assets));
    for (std::size_t i = 0; i < n_assets; ++i)
        for (std::size_t j = 0; j < n_assets; ++j)
            cov[i][j] = vols[i] * vols[j] * corr_matrix[i][j];

    auto L = cholesky(cov);
    std::normal_distribution<double> norm(0.0, 1.0);
    std::vector<double> finals;
    finals.reserve(cfg_.simulations);

    for (int s = 0; s < cfg_.simulations; ++s) {
        double portfolio_val = 0.0;
        for (std::size_t a = 0; a < n_assets; ++a)
            portfolio_val += weights[a] * initial_prices[a];

        std::vector<double> prices = initial_prices;
        for (int t = 0; t < cfg_.horizon_days; ++t) {
            std::vector<double> z(n_assets);
            for (auto& zi : z) zi = norm(rng_);
            // Correlated draw: L * z
            std::vector<double> corr_z(n_assets, 0.0);
            for (std::size_t i = 0; i < n_assets; ++i)
                for (std::size_t j = 0; j <= i; ++j)
                    corr_z[i] += L[i][j] * z[j];

            for (std::size_t a = 0; a < n_assets; ++a) {
                prices[a] *= std::exp((drifts[a] - 0.5 * vols[a] * vols[a]) * cfg_.dt
                                       + corr_z[a] * std::sqrt(cfg_.dt));
            }
        }
        double final_val = 0.0;
        for (std::size_t a = 0; a < n_assets; ++a)
            final_val += weights[a] * prices[a];
        finals.push_back(final_val);
    }

    MCResult result;
    result.final_values = finals;
    result.mean = std::accumulate(finals.begin(), finals.end(), 0.0) / finals.size();
    return result;
}

MCResult MonteCarloSimulator::bootstrap_returns(const std::vector<double>& historical_returns,
                                                double initial_capital) const {
    if (historical_returns.empty()) return {};
    std::uniform_int_distribution<std::size_t> idx(0, historical_returns.size() - 1);
    std::vector<double> finals;
    finals.reserve(cfg_.simulations);

    for (int s = 0; s < cfg_.simulations; ++s) {
        double val = initial_capital;
        for (int t = 0; t < cfg_.horizon_days; ++t)
            val *= (1.0 + historical_returns[idx(rng_)]);
        finals.push_back(val);
    }

    MCResult result;
    result.final_values = finals;
    result.mean = std::accumulate(finals.begin(), finals.end(), 0.0) / finals.size();

    std::vector<double> sorted = finals;
    std::sort(sorted.begin(), sorted.end());
    std::size_t idx_95 = static_cast<std::size_t>(0.05 * sorted.size());
    result.var_95 = initial_capital - sorted[std::min(idx_95, sorted.size()-1)];
    result.prob_loss = static_cast<double>(
        std::count_if(finals.begin(), finals.end(),
                      [initial_capital](double v){ return v < initial_capital; })
    ) / finals.size();
    return result;
}

} // namespace quantlab::monte_carlo
