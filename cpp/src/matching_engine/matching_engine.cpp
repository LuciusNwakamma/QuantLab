#include "matching_engine/matching_engine.hpp"
#include <stdexcept>

namespace quantlab::matching_engine {

MatchingEngine::MatchingEngine(MatchingAlgorithm algo) : algo_(algo) {}

void MatchingEngine::add_symbol(const Symbol& symbol) {
    books_.emplace(symbol, std::make_unique<order_book::OrderBook>(symbol));
}

void MatchingEngine::remove_symbol(const Symbol& symbol) {
    books_.erase(symbol);
}

ExecutionReport MatchingEngine::submit_order(order_book::Order order) {
    auto it = books_.find(order.symbol);
    if (it == books_.end()) {
        throw std::runtime_error("Symbol not registered: " + order.symbol);
    }

    order_count_++;
    Quantity qty_before = order.filled;

    it->second->add_order(order);

    Quantity filled = order.filled - qty_before;
    ExecutionReport report{
        order.id,
        order.symbol,
        order.side,
        filled > 1e-9 ? order.price : 0.0,
        filled,
        order.quantity - order.filled,
        order.filled >= order.quantity - 1e-9,
        std::chrono::system_clock::now()
    };

    if (filled > 1e-9) trade_count_++;
    if (exec_cb_) exec_cb_(report);
    return report;
}

void MatchingEngine::cancel_order(const Symbol& symbol, OrderId id) {
    auto it = books_.find(symbol);
    if (it != books_.end()) it->second->cancel_order(id);
}

const order_book::OrderBook* MatchingEngine::get_book(const Symbol& symbol) const {
    auto it = books_.find(symbol);
    return it != books_.end() ? it->second.get() : nullptr;
}

void MatchingEngine::set_execution_callback(ExecutionCallback cb) {
    exec_cb_ = std::move(cb);
}

std::uint64_t MatchingEngine::total_trades() const { return trade_count_.load(); }
std::uint64_t MatchingEngine::total_orders() const { return order_count_.load(); }

} // namespace quantlab::matching_engine
