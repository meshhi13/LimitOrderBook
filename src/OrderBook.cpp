#include "lob/OrderBook.hpp"

namespace lob {

template <typename Book, typename CrossPredicate>
Quantity OrderBook::match(Side taker_side, OrderId taker_id, Quantity quantity, Book& book,
                           CrossPredicate crosses) {
    auto level_it = book.begin();
    while (quantity > 0 && level_it != book.end() && crosses(level_it->first)) {
        PriceLevel& level = level_it->second;
        auto order_it = level.orders.begin();
        while (quantity > 0 && order_it != level.orders.end()) {
            Order& resting = *order_it;
            const Quantity fill = std::min(quantity, resting.quantity);

            resting.quantity -= fill;
            quantity -= fill;
            level.total_quantity -= fill;

            if (on_trade_) {
                on_trade_(Trade{.maker_id = resting.id,
                                 .taker_id = taker_id,
                                 .price = level_it->first,
                                 .quantity = fill,
                                 .timestamp = clock_++});
            }

            if (resting.quantity == 0) {
                locations_.erase(resting.id);
                order_it = level.orders.erase(order_it);
            } else {
                ++order_it;
            }
        }

        if (level.orders.empty()) {
            level_it = book.erase(level_it);
        } else {
            ++level_it;
        }
    }
    (void)taker_side;
    return quantity;
}

void OrderBook::rest(OrderId id, Side side, Price price, Quantity quantity) {
    auto& level = (side == Side::Buy) ? bids_[price] : asks_[price];
    level.orders.push_back(Order{.id = id, .side = side, .price = price, .quantity = quantity,
                                  .timestamp = clock_++});
    level.total_quantity += quantity;

    auto it = std::prev(level.orders.end());
    locations_[id] = OrderLocation{.side = side, .price = price, .it = it};
}

Quantity OrderBook::add_limit_order(OrderId id, Side side, Price price, Quantity quantity) {
    Quantity remaining = quantity;
    if (side == Side::Buy) {
        remaining = match(side, id, remaining, asks_,
                           [price](Price level_price) { return level_price <= price; });
    } else {
        remaining = match(side, id, remaining, bids_,
                           [price](Price level_price) { return level_price >= price; });
    }

    if (remaining > 0) {
        rest(id, side, price, remaining);
    }
    return remaining;
}

Quantity OrderBook::add_market_order(OrderId id, Side side, Quantity quantity) {
    Quantity remaining = quantity;
    if (side == Side::Buy) {
        remaining = match(side, id, remaining, asks_, [](Price) { return true; });
    } else {
        remaining = match(side, id, remaining, bids_, [](Price) { return true; });
    }
    return quantity - remaining;
}

bool OrderBook::cancel_order(OrderId id) {
    auto loc_it = locations_.find(id);
    if (loc_it == locations_.end()) {
        return false;
    }
    const OrderLocation loc = loc_it->second;

    auto erase_from = [&](auto& book) {
        auto level_it = book.find(loc.price);
        PriceLevel& level = level_it->second;
        level.total_quantity -= loc.it->quantity;
        level.orders.erase(loc.it);
        if (level.orders.empty()) {
            book.erase(level_it);
        }
    };

    if (loc.side == Side::Buy) {
        erase_from(bids_);
    } else {
        erase_from(asks_);
    }
    locations_.erase(loc_it);
    return true;
}

bool OrderBook::modify_order_quantity(OrderId id, Quantity new_quantity) {
    auto loc_it = locations_.find(id);
    if (loc_it == locations_.end()) {
        return false;
    }
    const OrderLocation& loc = loc_it->second;
    if (new_quantity == 0 || new_quantity >= loc.it->quantity) {
        return false;
    }

    PriceLevel& level = (loc.side == Side::Buy) ? bids_.find(loc.price)->second
                                                 : asks_.find(loc.price)->second;
    const Quantity delta = loc.it->quantity - new_quantity;
    loc.it->quantity = new_quantity;
    level.total_quantity -= delta;
    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Quantity> OrderBook::quantity_at(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it == bids_.end()) return std::nullopt;
        return it->second.total_quantity;
    }
    auto it = asks_.find(price);
    if (it == asks_.end()) return std::nullopt;
    return it->second.total_quantity;
}

} // namespace lob
