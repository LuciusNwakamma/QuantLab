#pragma once
#include <cstdint>
#include <string>
#include <chrono>

namespace quantlab {

using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
using Price     = double;
using Quantity  = double;
using OrderId   = std::uint64_t;
using Symbol    = std::string;

enum class Side { Buy, Sell };
enum class OrderType { Market, Limit, Stop, StopLimit };
enum class TimeInForce { GTC, DAY, IOC, FOK };
enum class AssetClass { Equity, ETF, Future, Crypto, Option };

struct Bar {
    Timestamp timestamp;
    Symbol    symbol;
    Price     open;
    Price     high;
    Price     low;
    Price     close;
    Quantity  volume;
    Price     vwap{0.0};
};

struct Tick {
    Timestamp timestamp;
    Symbol    symbol;
    Price     bid;
    Price     ask;
    Price     last;
    Quantity  bid_size;
    Quantity  ask_size;
    Quantity  last_size;
};

} // namespace quantlab
