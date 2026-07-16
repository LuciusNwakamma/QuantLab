#pragma once
#include "common/types.hpp"
#include <map>
#include <deque>
#include <functional>
#include <shared_mutex>
#include <atomic>

namespace quantlab::order_book {

struct Order {
    OrderId     id;
    Symbol      symbol;
    Side        side;
    OrderType   type;
    Price       price;
    Quantity    quantity;
    Quantity    filled{0.0};
    TimeInForce tif{TimeInForce::GTC};
    Timestamp   timestamp;
};

struct PriceLevel {
    Price    price;
    Quantity total_quantity{0.0};
    std::deque<Order> orders;
};

struct Trade {
    OrderId   maker_id;
    OrderId   taker_id;
    Symbol    symbol;
    Price     price;
    Quantity  quantity;
    Timestamp timestamp;
};

using TradeCallback = std::function<void(const Trade&)>;

class OrderBook {
public:
    explicit OrderBook(Symbol symbol);

    void add_order(const Order& order);
    void cancel_order(OrderId id);
    void modify_order(OrderId id, Price new_price, Quantity new_qty);

    [[nodiscard]] Price    best_bid() const;
    [[nodiscard]] Price    best_ask() const;
    [[nodiscard]] Price    mid_price() const;
    [[nodiscard]] Price    spread() const;
    [[nodiscard]] Quantity bid_depth(int levels = 5) const;
    [[nodiscard]] Quantity ask_depth(int levels = 5) const;
    [[nodiscard]] double   order_imbalance(int levels = 5) const;

    void set_trade_callback(TradeCallback cb);

    // Level-2 snapshot
    std::vector<PriceLevel> bid_levels(int depth = 10) const;
    std::vector<PriceLevel> ask_levels(int depth = 10) const;

private:
    Symbol symbol_;
    // bids: descending (highest first), asks: ascending (lowest first)
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel>                      asks_;
    mutable std::shared_mutex                        mutex_;
    TradeCallback                                    trade_cb_;
    std::atomic<OrderId>                             next_id_{1};

    void match_order(Order& taker);
};

} // namespace quantlab::order_book
