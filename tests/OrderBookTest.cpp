#include "lob/OrderBook.hpp"

#include <gtest/gtest.h>

#include "lob/MatchingEngine.hpp"

using namespace lob;

TEST(OrderBook, RestsWhenNoCross) {
    OrderBook book;
    const Quantity remaining = book.add_limit_order(1, Side::Buy, 100, 10);
    EXPECT_EQ(remaining, 10u);
    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBook, MatchesCrossingOrdersFully) {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    book.add_limit_order(1, Side::Sell, 100, 10);
    const Quantity remaining = book.add_limit_order(2, Side::Buy, 100, 10);

    EXPECT_EQ(remaining, 0u);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].maker_id, 1u);
    EXPECT_EQ(trades[0].taker_id, 2u);
    EXPECT_EQ(trades[0].quantity, 10u);
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBook, PartialFillRestsRemainder) {
    OrderBook book;
    book.add_limit_order(1, Side::Sell, 100, 5);
    const Quantity remaining = book.add_limit_order(2, Side::Buy, 100, 8);

    EXPECT_EQ(remaining, 3u);
    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_EQ(book.quantity_at(Side::Buy, 100), 3u);
}

TEST(OrderBook, PriceTimePriorityFifoWithinLevel) {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    book.add_limit_order(1, Side::Sell, 100, 5);
    book.add_limit_order(2, Side::Sell, 100, 5);
    book.add_limit_order(3, Side::Buy, 100, 6);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_id, 1u);
    EXPECT_EQ(trades[0].quantity, 5u);
    EXPECT_EQ(trades[1].maker_id, 2u);
    EXPECT_EQ(trades[1].quantity, 1u);
    EXPECT_EQ(book.quantity_at(Side::Sell, 100), 4u);
}

TEST(OrderBook, BestPriceMatchesAcrossLevels) {
    OrderBook book;
    book.add_limit_order(1, Side::Sell, 101, 5);
    book.add_limit_order(2, Side::Sell, 100, 5);

    EXPECT_EQ(book.best_ask(), 100);

    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });
    book.add_limit_order(3, Side::Buy, 101, 10);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_id, 2u); // cheaper ask fills first
    EXPECT_EQ(trades[0].price, 100);
    EXPECT_EQ(trades[1].maker_id, 1u);
    EXPECT_EQ(trades[1].price, 101);
}

TEST(OrderBook, CancelRemovesOrder) {
    OrderBook book;
    book.add_limit_order(1, Side::Buy, 100, 10);
    EXPECT_TRUE(book.contains(1));

    EXPECT_TRUE(book.cancel_order(1));
    EXPECT_FALSE(book.contains(1));
    EXPECT_FALSE(book.best_bid().has_value());

    EXPECT_FALSE(book.cancel_order(1)); // already gone
}

TEST(OrderBook, ModifyQuantityReducesInPlace) {
    OrderBook book;
    book.add_limit_order(1, Side::Buy, 100, 10);

    EXPECT_TRUE(book.modify_order_quantity(1, 4));
    EXPECT_EQ(book.quantity_at(Side::Buy, 100), 4u);

    // Can't "modify" up, and can't set to zero (that's a cancel).
    EXPECT_FALSE(book.modify_order_quantity(1, 10));
    EXPECT_FALSE(book.modify_order_quantity(1, 0));
}

TEST(OrderBook, MarketOrderSweepsBookAndDoesNotRest) {
    OrderBook book;
    book.add_limit_order(1, Side::Sell, 100, 5);
    book.add_limit_order(2, Side::Sell, 101, 5);

    const Quantity filled = book.add_market_order(3, Side::Buy, 8);
    EXPECT_EQ(filled, 8u);
    EXPECT_EQ(book.quantity_at(Side::Sell, 101), 2u);
    EXPECT_FALSE(book.contains(3)); // market orders never rest
}

TEST(OrderBook, MarketOrderDropsUnfilledQuantityWhenBookEmpty) {
    OrderBook book;
    const Quantity filled = book.add_market_order(1, Side::Buy, 100);
    EXPECT_EQ(filled, 0u);
    EXPECT_EQ(book.order_count(), 0u);
}

TEST(MatchingEngineTest, AssignsSequentialIdsAndCollectsTrades) {
    MatchingEngine engine;
    const OrderId a = engine.submit_limit_order(Side::Sell, 100, 10);
    const OrderId b = engine.submit_limit_order(Side::Buy, 100, 10);

    EXPECT_EQ(b, a + 1);
    ASSERT_EQ(engine.trades().size(), 1u);
    EXPECT_EQ(engine.trades()[0].quantity, 10u);
}
