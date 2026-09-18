#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "lob/Order.hpp"

namespace lob {

// A single price level, holding resting orders in strict FIFO (price-time
// priority) order. Orders are kept in a std::list so that cancellation is
// O(1) given an iterator, and new orders append in O(1).
struct PriceLevel {
    std::list<Order> orders;
    Quantity total_quantity = 0;
};

// Where a live order lives, so cancel()/modify() are O(1) average instead of
// O(levels) scans.
struct OrderLocation {
    Side side;
    Price price;
    std::list<Order>::iterator it;
};

// A single-instrument limit order book with continuous matching.
//
// Bids are stored highest-price-first, asks lowest-price-first, so the book
// top (best bid / best ask) is always begin() on either map. Within a price
// level, orders fill in FIFO arrival order (strict price-time priority).
class OrderBook {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    explicit OrderBook(TradeCallback on_trade = nullptr) : on_trade_(std::move(on_trade)) {}

    void set_trade_callback(TradeCallback cb) { on_trade_ = std::move(cb); }

    // Submits a new limit order. It first matches against resting orders on
    // the opposite side while prices cross, then rests any remaining
    // quantity on the book. Returns the quantity that was left resting.
    Quantity add_limit_order(OrderId id, Side side, Price price, Quantity quantity);

    // Submits a market order: matches immediately against whatever liquidity
    // is available, without a price limit. Unfilled quantity is dropped
    // (market orders never rest on the book). Returns the quantity filled.
    Quantity add_market_order(OrderId id, Side side, Quantity quantity);

    // Cancels a live order. Returns false if the id is unknown (already
    // filled or already canceled).
    bool cancel_order(OrderId id);

    // Reduces the quantity of a resting order in place, preserving its
    // queue position (price-time priority is not reset). Returns false if
    // the id is unknown or new_quantity is not smaller than the current one.
    bool modify_order_quantity(OrderId id, Quantity new_quantity);

    [[nodiscard]] std::optional<Price> best_bid() const;
    [[nodiscard]] std::optional<Price> best_ask() const;
    [[nodiscard]] std::optional<Quantity> quantity_at(Side side, Price price) const;
    [[nodiscard]] std::size_t order_count() const { return locations_.size(); }
    [[nodiscard]] bool contains(OrderId id) const { return locations_.contains(id); }

private:
    // Bids: highest price first. Asks: lowest price first.
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderLocation> locations_;
    TradeCallback on_trade_;
    Timestamp clock_ = 0;

    template <typename Book, typename CrossPredicate>
    Quantity match(Side taker_side, OrderId taker_id, Quantity quantity, Book& book,
                   CrossPredicate crosses);

    void rest(OrderId id, Side side, Price price, Quantity quantity);
};

} // namespace lob
