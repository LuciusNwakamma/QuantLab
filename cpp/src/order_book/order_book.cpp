#include "order_book/order_book.hpp"
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <mutex>

namespace quantlab::order_book {

OrderBook::OrderBook(Symbol symbol) : symbol_(std::move(symbol)) {}

void OrderBook::add_order(const Order& order) {
    std::unique_lock lock(mutex_);
    Order o = order;
    if (o.id == 0) o.id = next_id_++;
    match_order(o);
    if (o.quantity - o.filled > 1e-9 && o.type == OrderType::Limit) {
        PriceLevel& level = (o.side == Side::Buy ? bids_[o.price] : asks_[o.price]);
        level.price = o.price;
        level.total_quantity += (o.quantity - o.filled);
        level.orders.push_back(o);
    }
}

void OrderBook::cancel_order(OrderId id) {
    std::unique_lock lock(mutex_);
    auto cancel_from = [&](auto& side_map) {
        for (auto& [px, level] : side_map) {
            auto it = std::find_if(level.orders.begin(), level.orders.end(),
                                   [id](const Order& o){ return o.id == id; });
            if (it != level.orders.end()) {
                level.total_quantity -= (it->quantity - it->filled);
                level.orders.erase(it);
                if (level.orders.empty()) side_map.erase(px);
                return true;
            }
        }
        return false;
    };
    if (!cancel_from(bids_)) cancel_from(asks_);
}

void OrderBook::modify_order(OrderId id, Price new_price, Quantity new_qty) {
    cancel_order(id);
    // Re-submit as new order; caller responsible for re-creating Order struct
    (void)new_price; (void)new_qty; // placeholder
}

Price OrderBook::best_bid() const {
    std::shared_lock lock(mutex_);
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

Price OrderBook::best_ask() const {
    std::shared_lock lock(mutex_);
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

Price OrderBook::mid_price() const {
    return (best_bid() + best_ask()) / 2.0;
}

Price OrderBook::spread() const {
    return best_ask() - best_bid();
}

Quantity OrderBook::bid_depth(int levels) const {
    std::shared_lock lock(mutex_);
    Quantity depth = 0.0;
    int n = 0;
    for (auto& [px, level] : bids_) {
        depth += level.total_quantity;
        if (++n >= levels) break;
    }
    return depth;
}

Quantity OrderBook::ask_depth(int levels) const {
    std::shared_lock lock(mutex_);
    Quantity depth = 0.0;
    int n = 0;
    for (auto& [px, level] : asks_) {
        depth += level.total_quantity;
        if (++n >= levels) break;
    }
    return depth;
}

double OrderBook::order_imbalance(int levels) const {
    double bd = bid_depth(levels);
    double ad = ask_depth(levels);
    if (bd + ad < 1e-12) return 0.0;
    return (bd - ad) / (bd + ad);
}

void OrderBook::set_trade_callback(TradeCallback cb) {
    trade_cb_ = std::move(cb);
}

std::vector<PriceLevel> OrderBook::bid_levels(int depth) const {
    std::shared_lock lock(mutex_);
    std::vector<PriceLevel> result;
    int n = 0;
    for (auto& [px, level] : bids_) {
        result.push_back(level);
        if (++n >= depth) break;
    }
    return result;
}

std::vector<PriceLevel> OrderBook::ask_levels(int depth) const {
    std::shared_lock lock(mutex_);
    std::vector<PriceLevel> result;
    int n = 0;
    for (auto& [px, level] : asks_) {
        result.push_back(level);
        if (++n >= depth) break;
    }
    return result;
}

void OrderBook::match_order(Order& taker) {
    // For buy: match against asks (ascending)
    // For sell: match against bids (descending)
    // We iterate using the actual maps
    auto try_match = [&](auto& book) {
        Quantity remaining = taker.quantity - taker.filled;
        for (auto it = book.begin(); it != book.end() && remaining > 1e-9; ) {
            Price maker_px = it->first;
            bool price_ok = (taker.type == OrderType::Market) ||
                            (taker.side == Side::Buy  ? taker.price >= maker_px
                                                      : taker.price <= maker_px);
            if (!price_ok) break;

            PriceLevel& level = it->second;
            while (!level.orders.empty() && remaining > 1e-9) {
                Order& maker = level.orders.front();
                Quantity fill = std::min(remaining, maker.quantity - maker.filled);

                maker.filled   += fill;
                taker.filled   += fill;
                remaining      -= fill;
                level.total_quantity -= fill;

                if (trade_cb_) {
                    trade_cb_(Trade{maker.id, taker.id, symbol_,
                                    maker_px, fill,
                                    std::chrono::system_clock::now()});
                }
                if (maker.filled >= maker.quantity - 1e-9) {
                    level.orders.pop_front();
                }
            }
            if (level.orders.empty()) {
                it = book.erase(it);
            } else {
                ++it;
            }
        }
    };

    if (taker.side == Side::Buy)  try_match(asks_);
    else                          try_match(bids_);
}

} // namespace quantlab::order_book
