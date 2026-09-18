#pragma once

#include <atomic>
#include <vector>

#include "lob/Order.hpp"
#include "lob/OrderBook.hpp"

namespace lob {

// Thin facade over OrderBook that owns order-id assignment and collects
// trades, so callers (simulators, tests, benchmarks) don't manage ids
// themselves.
class MatchingEngine {
public:
    MatchingEngine() {
        book_.set_trade_callback([this](const Trade& trade) { trades_.push_back(trade); });
    }

    // Submits a new limit order, returns its assigned id.
    OrderId submit_limit_order(Side side, Price price, Quantity quantity) {
        const OrderId id = next_id_++;
        book_.add_limit_order(id, side, price, quantity);
        return id;
    }

    // Submits a market order, returns its assigned id.
    OrderId submit_market_order(Side side, Quantity quantity) {
        const OrderId id = next_id_++;
        book_.add_market_order(id, side, quantity);
        return id;
    }

    bool cancel_order(OrderId id) { return book_.cancel_order(id); }

    [[nodiscard]] const OrderBook& book() const { return book_; }
    [[nodiscard]] OrderBook& book() { return book_; }

    [[nodiscard]] const std::vector<Trade>& trades() const { return trades_; }
    void clear_trades() { trades_.clear(); }

private:
    OrderBook book_;
    std::vector<Trade> trades_;
    std::atomic<OrderId> next_id_{1};
};

} // namespace lob
