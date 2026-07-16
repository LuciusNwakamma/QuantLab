# QuantLab

An institutional-grade Quantitative Research and Algorithmic Trading Platform.

## Overview

QuantLab is a professional-level research platform built by Lucius to mirror the internal tools used at quantitative trading firms. It combines high-performance C++20 components with Python research notebooks, covering the full lifecycle from market data ingestion to live paper trading.

## Architecture

```
Market Data → Data Cleaner → Feature Engineering → Research Engine
    → Strategy Engine → Risk Engine → Backtester
    → Portfolio Optimizer → Execution Simulator
    → Performance Analytics → Live Paper Trading
```

## Tech Stack

| Layer         | Technology                                      |
|---------------|-------------------------------------------------|
| Core Engine   | C++20, CMake                                    |
| Research      | Python (pandas, numpy, scipy, statsmodels, PyTorch) |
| Database      | PostgreSQL / DuckDB                             |
| Market Data   | Polygon, Alpaca, AlphaVantage, Binance, Yahoo Finance |
| Testing       | Google Test, pytest                             |
| CI/CD         | GitHub Actions, Docker                          |

## Modules

| # | Module                 | Language   |
|---|------------------------|------------|
| 1 | Market Data Engine     | C++ / Python |
| 2 | Feature Engineering    | C++ / Python |
| 3 | Statistical Research   | Python       |
| 4 | Machine Learning       | Python       |
| 5 | Strategy Engine        | C++ / Python |
| 6 | Risk Engine            | C++          |
| 7 | Portfolio Optimizer    | C++          |
| 8 | Backtesting Engine     | C++          |
| 9 | Execution Simulator    | C++          |
| 10| Performance Analytics  | Python       |
| 11| Live Paper Trading     | Python       |
| 12| Order Book & Matching  | C++          |

## Project Structure

```
QuantLab/
├── cpp/                        # C++ core components
│   ├── include/                # Public headers
│   │   ├── common/
│   │   ├── market_data/
│   │   ├── order_book/
│   │   ├── matching_engine/
│   │   ├── risk_engine/
│   │   ├── backtester/
│   │   ├── portfolio_optimizer/
│   │   ├── execution_simulator/
│   │   └── monte_carlo/
│   └── src/                   # Implementation
├── python/                     # Python research layer
│   ├── market_data/
│   ├── feature_engineering/
│   ├── statistical_research/
│   ├── machine_learning/
│   ├── strategy_research/
│   ├── performance_analytics/
│   └── live_trading/
├── research/                   # Jupyter research notebooks
│   └── notebooks/
├── tests/                      # Unit & integration tests
│   ├── cpp/
│   └── python/
├── docker/                     # Container configuration
├── .github/workflows/          # CI/CD pipelines
├── docs/                       # Documentation
└── scripts/                    # Build & utility scripts
```

## Building

### Prerequisites

- CMake >= 3.20
- C++20-compatible compiler (GCC 11+ or Clang 13+)
- Python 3.10+
- Docker (optional)

### C++ Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Python Environment

```bash
cd python
pip install -r requirements.txt
```

### Docker

```bash
docker-compose -f docker/docker-compose.yml up --build
```

## Testing

```bash
# C++ tests
cd build && ctest --output-on-failure

# Python tests
pytest tests/python/ -v
```

## License

MIT
