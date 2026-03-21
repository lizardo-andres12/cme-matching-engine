#include <gtest/gtest.h>
#include <stdexcept>

#include "containers/order_book.hpp"
#include "core/order_types.hpp"
#include "types.hpp"

using namespace aux::containers;
using namespace aux::core;
using aux::core::stub_order;


class OrderBookTest : public ::testing::Test, public order_book<stub_order> {
protected:
    aux::id_t id;
    aux::id_t priority;
    aux::price_t price;
    aux::quantity_t qty;

    void SetUp() override {
	stub_order::active_instances = 0;
	id = 123;
	priority = 1;
	price = 1000;
	qty = 50;
    }

    const flat_book_array_t& get_books(const order_book<stub_order>& other) const noexcept {
	return order_book::get_books(other);
    }

    const order_map_t& get_order_map(const order_book<stub_order>& other) const noexcept {
	return order_book::get_order_map(other);
    }
};

TEST_F(OrderBookTest, InitialState) {
    order_book<stub_order> book;
    const auto& books = get_books(book);
    const auto& order_map = get_order_map(book);

    ASSERT_EQ(books[side_to_uint8(Side::Bid)].size(), 0);
    ASSERT_EQ(books[side_to_uint8(Side::Ask)].size(), 0);
    ASSERT_EQ(order_map.size(), 0);
    ASSERT_EQ(stub_order::active_instances, 0); // No price queues
}

TEST_F(OrderBookTest, OrderAddValid) {
    order_book<stub_order> book;
    const auto& books = get_books(book);
    const auto& order_map = get_order_map(book);
    const auto& bid_book = books[side_to_uint8(Side::Bid)];
    const auto& ask_book = books[side_to_uint8(Side::Ask)];

    {
	stub_order order({id, priority, price, qty, Side::Bid, 0});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(order_map.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3); // 2 for bid_book::price_queue, 1 for inserted order

    auto order_map_it = order_map.find(123);
    ASSERT_NE(order_map_it, order_map.end());
    EXPECT_EQ(order_map_it->second->data.order_inst.price(), price);

    auto price_level_it = bid_book.find(price);
    ASSERT_NE(price_level_it, bid_book.end());

    {
	stub_order res;
	price_level_it->second.front(res);
	EXPECT_EQ(res.id(), id);
    }

    id = 321;
    {
	stub_order order = stub_order({id, priority, price, qty, Side::Ask, 0});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(ask_book.size(), 1);
    EXPECT_EQ(order_map.size(), 2);
    EXPECT_EQ(stub_order::active_instances, 6); // 2 for bid_book::price_queue, 2 for ask_book::price_queue,
					       // 1 for inserted bid order, 1 for inserted ask order

    order_map_it = order_map.find(id);
    ASSERT_NE(order_map_it, order_map.end());
    EXPECT_EQ(order_map_it->second->data.order_inst.price(), price);

    price_level_it = ask_book.find(price);
    ASSERT_NE(price_level_it, bid_book.end());

    {
	stub_order res;
	price_level_it->second.front(res);
	EXPECT_EQ(res.id(), id);
    }
}

TEST_F(OrderBookTest, OrderAddInvalid) {
    order_book<stub_order> book;
    const auto& books = get_books(book);
    const auto& order_map = get_order_map(book);
    const auto& bid_book = books[side_to_uint8(Side::Bid)];

    {
	stub_order order({id, priority, price, qty, Side::Bid});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(order_map.size(), 1);

    auto order_map_it = order_map.find(id);
    ASSERT_NE(order_map_it, order_map.end());
    EXPECT_EQ(order_map_it->second->data.order_inst.price(), price);

    auto price_level_it = bid_book.find(price);
    ASSERT_NE(price_level_it, bid_book.end());

    {
	stub_order res;
	price_level_it->second.front(res);
	EXPECT_EQ(res.id(), id);
    }

    /** Side does not matter, no duplicate ID should be registered. */
    ASSERT_THROW(book.add(stub_order{ {id, priority, price, qty, Side::Ask} }), std::invalid_argument);
    ASSERT_THROW(book.add(stub_order{ {id, priority, price, qty, Side::Bid} }), std::invalid_argument);
}

TEST_F(OrderBookTest, OrderCancelValid) {
    order_book<stub_order> book;
    const auto& books = get_books(book);
    const auto& order_map = get_order_map(book);
    const auto& bid_book = books[side_to_uint8(Side::Bid)];
    const auto& ask_book = books[side_to_uint8(Side::Ask)];

    {
	stub_order order({id, priority, price, qty, Side::Bid});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(order_map.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3);

    ASSERT_NO_THROW(book.cancel(id));

    EXPECT_EQ(bid_book.size(), 0);
    EXPECT_EQ(order_map.size(), 0);
    EXPECT_EQ(stub_order::active_instances, 0);

    {
	for (aux::id_t i{id}; i < static_cast<aux::id_t>(id + 3); ++i) {
	    stub_order order({i, priority, price, qty, Side::Ask});
	    ASSERT_NO_THROW(book.add(std::move(order)));
	}

	stub_order order({static_cast<aux::id_t>(id - 1), priority, price, qty, Side::Bid});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(ask_book.size(), 1);
    EXPECT_EQ(order_map.size(), 4);
    EXPECT_EQ(stub_order::active_instances, 8);

    ASSERT_NO_THROW(book.cancel(static_cast<id_t>(id + 1)));
    ASSERT_NO_THROW(book.cancel(static_cast<id_t>(id + 2)));

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(ask_book.size(), 1);
    EXPECT_EQ(order_map.size(), 2);
    EXPECT_EQ(stub_order::active_instances, 6);
}

TEST_F(OrderBookTest, OrderCancelInvalid) {
    order_book<stub_order> book;
    const auto& books = get_books(book);
    const auto& order_map = get_order_map(book);
    const auto& bid_book = books[side_to_uint8(Side::Bid)];
    const auto& ask_book = books[side_to_uint8(Side::Ask)];

    {
	stub_order order({id, priority, price, qty, Side::Bid});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(order_map.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3);

    ASSERT_THROW(book.cancel(static_cast<id_t>(id + 1)), std::invalid_argument);

    {
	stub_order order({static_cast<aux::id_t>(id - 1), priority, price, qty, Side::Ask});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(ask_book.size(), 1);
    EXPECT_EQ(order_map.size(), 2);
    EXPECT_EQ(stub_order::active_instances, 6);

    ASSERT_NO_THROW(book.cancel(static_cast<id_t>(id - 1)));

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(ask_book.size(), 0);
    EXPECT_EQ(order_map.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3);

    ASSERT_THROW(book.cancel(static_cast<id_t>(id - 1)), std::invalid_argument);
}

TEST_F(OrderBookTest, OrderUpdateValid) {
    order_book<stub_order> book;
    const auto& books = get_books(book);
    const auto& order_map = get_order_map(book);
    const auto& bid_book = books[side_to_uint8(Side::Bid)];
    const auto& ask_book = books[side_to_uint8(Side::Ask)];

    {
	stub_order order({id, priority, price, qty, Side::Bid});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(order_map.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3);

    ASSERT_NO_THROW(book.update(id, static_cast<aux::id_t>(priority + 1), static_cast<aux::quantity_t>(qty)));

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(order_map.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3);
    
    {
	for (aux::id_t i{id + 1}; i <= static_cast<aux::id_t>(id + 3); ++i) {
	    stub_order order({i, priority, price, qty, Side::Ask});
	    ASSERT_NO_THROW(book.add(std::move(order)));
	}
    }

    EXPECT_EQ(ask_book.size(), 1);
    EXPECT_EQ(order_map.size(), 4);
    EXPECT_EQ(stub_order::active_instances, 8);

    {
	priority += 1;
	qty += 1;

	auto front_it = ask_book.begin();
	stub_order front_before_update;
	front_it->second.front(front_before_update);

	ASSERT_NO_THROW(book.update(static_cast<aux::id_t>(id + 1), priority, qty));

	stub_order front_after_update;
	front_it->second.front(front_after_update);

	EXPECT_NE(front_before_update.id(), front_after_update.id());
	EXPECT_EQ(front_before_update.id(), static_cast<aux::id_t>(id + 1));
	EXPECT_EQ(front_after_update.id(), static_cast<aux::id_t>(id + 2));
    }
}

TEST_F(OrderBookTest, OrderUpdateInvalid) {
    order_book<stub_order> book;
    const auto& books = get_books(book);
    const auto& order_map = get_order_map(book);
    const auto& bid_book = books[side_to_uint8(Side::Bid)];

    {
	stub_order order({id, priority, price, qty, Side::Bid});
	ASSERT_NO_THROW(book.add(std::move(order)));
    }

    EXPECT_EQ(bid_book.size(), 1);
    EXPECT_EQ(order_map.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3);

    ASSERT_THROW(book.update(static_cast<aux::id_t>(id + 1), priority, qty), std::invalid_argument);
}

