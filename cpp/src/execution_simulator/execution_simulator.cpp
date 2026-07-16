#include "execution_simulator/execution_simulator.hpp"
#include <cmath>
#include <random>

namespace quantlab::execution_simulator {

ExecutionSimulator::ExecutionSimulator(SimConfig config)
    : config_(std::move(config)) {}

OrderId ExecutionSimulator::submit_order(order_book::Order order) {
    std::lock_guard lock(mutex_);
    OrderId id = next_id_++;
    order.id = id;

    SimOrder sim;
    sim.order        = order;
    sim.submitted_at = std::chrono::system_clock::now();

    // Schedule processing after latency
    auto process_at = sim.submitted_at +
        std::chrono::milliseconds(static_cast<long>(config_.latency_ms));
    orders_[id] = sim;
    pending_queue_.push({process_at, id});
    return id;
}

void ExecutionSimulator::cancel_order(OrderId id) {
    std::lock_guard lock(mutex_);
    auto it = orders_.find(id);
    if (it != orders_.end() && it->second.status == OrderStatus::Pending)
        it->second.status = OrderStatus::Cancelled;
}

void ExecutionSimulator::process_tick(const Tick& tick) {
    std::lock_guard lock(mutex_);
    auto now = std::chrono::system_clock::now();

    while (!pending_queue_.empty() && pending_queue_.top().first <= now) {
        OrderId id = pending_queue_.top().second;
        pending_queue_.pop();

        auto it = orders_.find(id);
        if (it == orders_.end()) continue;
        SimOrder& sim = it->second;
        if (sim.status != OrderStatus::Pending) continue;
        if (sim.order.symbol != tick.symbol) continue;

        try_fill(sim, tick);
        if (fill_cb_ && (sim.status == OrderStatus::Filled ||
                          sim.status == OrderStatus::PartiallyFilled))
            fill_cb_(sim);
    }
}

void ExecutionSimulator::try_fill(SimOrder& sim, const Tick& tick) {
    static thread_local std::mt19937 rng{std::random_device{}()};

    Price ref_price = (sim.order.side == Side::Buy) ? tick.ask : tick.bid;

    bool price_ok = (sim.order.type == OrderType::Market) ||
                    (sim.order.side == Side::Buy  ? sim.order.price >= ref_price
                                                  : sim.order.price <= ref_price);
    if (!price_ok) return;

    Price fill_px = apply_slippage(sim.order.side, ref_price);
    Quantity available = sim.order.quantity - sim.order.filled;

    // Partial fills
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (config_.simulate_partial_fills && dist(rng) < config_.partial_fill_probability) {
        std::uniform_real_distribution<double> frac_dist(0.3, 0.9);
        available *= frac_dist(rng);
    }

    sim.order.filled += available;
    sim.avg_fill_price = fill_px;
    sim.commission     = compute_commission(available, fill_px);
    sim.filled_at      = std::chrono::system_clock::now();
    sim.status         = (sim.order.filled >= sim.order.quantity - 1e-9)
                         ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
}

double ExecutionSimulator::compute_commission(Quantity qty, Price price) const {
    return std::max(config_.commission_min, qty * config_.commission_per_share);
}

Price ExecutionSimulator::apply_slippage(Side side, Price price) const {
    double slip = price * (config_.slippage_bps / 10000.0);
    return (side == Side::Buy) ? price + slip : price - slip;
}

void ExecutionSimulator::set_fill_callback(FillCallback cb) {
    fill_cb_ = std::move(cb);
}

const SimOrder* ExecutionSimulator::get_order(OrderId id) const {
    std::lock_guard lock(mutex_);
    auto it = orders_.find(id);
    return it != orders_.end() ? &it->second : nullptr;
}

std::vector<SimOrder> ExecutionSimulator::open_orders() const {
    std::lock_guard lock(mutex_);
    std::vector<SimOrder> result;
    for (auto& [id, sim] : orders_)
        if (sim.status == OrderStatus::Pending || sim.status == OrderStatus::PartiallyFilled)
            result.push_back(sim);
    return result;
}

} // namespace quantlab::execution_simulator
