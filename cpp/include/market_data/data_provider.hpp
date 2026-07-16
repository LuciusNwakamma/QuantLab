#pragma once
#include "common/types.hpp"
#include <vector>
#include <optional>
#include <string>
#include <functional>

namespace quantlab::market_data {

struct DataRequest {
    std::vector<Symbol> symbols;
    Timestamp           start;
    Timestamp           end;
    std::string         resolution; // "1m", "1h", "1d", "tick"
    AssetClass          asset_class{AssetClass::Equity};
};

class IDataProvider {
public:
    virtual ~IDataProvider() = default;

    virtual std::vector<Bar>  fetch_bars(const DataRequest& req) = 0;
    virtual std::vector<Tick> fetch_ticks(const DataRequest& req) = 0;
    virtual std::string       provider_name() const = 0;
};

// Streaming callback types
using BarCallback  = std::function<void(const Bar&)>;
using TickCallback = std::function<void(const Tick&)>;

class IStreamingProvider : public IDataProvider {
public:
    virtual void subscribe(const std::vector<Symbol>& symbols,
                           BarCallback  on_bar,
                           TickCallback on_tick) = 0;
    virtual void unsubscribe(const std::vector<Symbol>& symbols) = 0;
    virtual void start() = 0;
    virtual void stop()  = 0;
};

} // namespace quantlab::market_data
