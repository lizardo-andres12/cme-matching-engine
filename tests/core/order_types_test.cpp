#include <gtest/gtest.h>

#include "core/order_types.hpp"
#include "types.hpp"

using namespace aux;
using namespace aux::core;

TEST(OrderTypesTest, FillGreaterThanQty) {
    order o{};
    quantity_t qty = static_cast<quantity_t>(50);
    quantity_t fill = static_cast<quantity_t>(51);

    o.quantity_ = qty;
    ASSERT_EQ(qty, o.fill(fill));
    ASSERT_EQ(o.quantity_, static_cast<quantity_t>(0));
}

TEST(OrderTypesTest, FillLessThanQty) {
    order o{};
    quantity_t qty = static_cast<quantity_t>(50);
    quantity_t fill = static_cast<quantity_t>(49);

    o.quantity_ = qty;
    ASSERT_EQ(fill, o.fill(fill));
    ASSERT_EQ(o.quantity_, static_cast<quantity_t>(1));
}

TEST(OrderTypesTest, OperatorEqTrue) {
    order lhs{1, 1, 100, 50, Side::Bid, 0};
    order rhs{1, 1, 100, 50, Side::Bid, 0};
    EXPECT_TRUE(lhs == rhs);

    /** Timestamps will never be the same for two orders because this 
     is a single threaded application. Thus, timestamps should be ignored. */
    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 1, 100, 50, Side::Bid, 1};
    EXPECT_TRUE(lhs == rhs);
}

TEST(OrderTypesTest, OperatorEqFalse) {
    order lhs{1, 1, 100, 50, Side::Bid, 0};
    order rhs{1, 1, 100, 50, Side::Ask, 0};
    EXPECT_FALSE(lhs == rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 1, 100, 49, Side::Bid, 0};
    EXPECT_FALSE(lhs == rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 1, 99, 50, Side::Bid, 0};
    EXPECT_FALSE(lhs == rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 2, 100, 50, Side::Bid, 0};
    EXPECT_FALSE(lhs == rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {2, 1, 100, 50, Side::Bid, 0};
    EXPECT_FALSE(lhs == rhs);
}

TEST(OrderTypesTest, OperatorNeTrue) {
    order lhs{1, 1, 100, 50, Side::Bid, 0};
    order rhs{1, 1, 100, 50, Side::Ask, 0};
    EXPECT_TRUE(lhs != rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 1, 100, 49, Side::Bid, 0};
    EXPECT_TRUE(lhs != rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 1, 99, 50, Side::Bid, 0};
    EXPECT_TRUE(lhs != rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 2, 100, 50, Side::Bid, 0};
    EXPECT_TRUE(lhs != rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {2, 1, 100, 50, Side::Bid, 0};
    EXPECT_TRUE(lhs != rhs);
}

TEST(OrderTypesTest, OperatorNeFalse) {
    order lhs{1, 1, 100, 50, Side::Bid, 0};
    order rhs{1, 1, 100, 50, Side::Bid, 0};
    EXPECT_FALSE(lhs != rhs);

    lhs = {1, 1, 100, 50, Side::Bid, 0};
    rhs = {1, 1, 100, 50, Side::Bid, 1};
    EXPECT_FALSE(lhs != rhs);
}

