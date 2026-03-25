#include <gtest/gtest.h>

#include "containers/price_queue.hpp"
#include "core/order_types.hpp"
#include "types.hpp"

using namespace aux::containers;
using namespace aux::core;
using aux::core::stub_order;

class PriceQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        stub_order::active_instances = 0;
    }
};

// --- Core Functionality ---

TEST_F(PriceQueueTest, InitialState) {
    price_queue<stub_order> pq;
    ASSERT_EQ(stub_order::active_instances, 2);
    stub_order res, expected;
    ASSERT_EQ(stub_order::active_instances, 4);
    pq.front(res);

    EXPECT_EQ(pq.size(), 0);
    EXPECT_EQ(res, expected);
}

TEST_F(PriceQueueTest, EnqueueAndLinkage) {
    price_queue<stub_order> pq;
    stub_order first{ order{1} };
    stub_order second{ order{2} };

    auto node1 = pq.enqueue(std::move(first));
    auto node2 = pq.enqueue(std::move(second));

    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);
    
    // Verify Double Linkage via public Node struct 
    EXPECT_EQ(node1->data.id(), 1);
    EXPECT_EQ(node2->data.id(), 2);
    EXPECT_EQ(node1->next, node2);
    EXPECT_EQ(node2->prev, node1);
}

TEST_F(PriceQueueTest, RemoveMiddleNode) {
    price_queue<stub_order> pq;
    auto* first = pq.enqueue(stub_order(order{1}));
    auto* mid = pq.enqueue(stub_order(order{2}));
    auto* last = pq.enqueue(stub_order(order{3}));
    EXPECT_EQ(stub_order::active_instances, 5);

    pq.remove(mid);

    // Verify Double Linkage via public Node struct 
    EXPECT_EQ(first->data.id(), 1);
    EXPECT_EQ(last->data.id(), 3);
    EXPECT_EQ(first->next, last);
    EXPECT_EQ(last->prev, first);
    EXPECT_EQ(pq.size(), 2);
    EXPECT_EQ(stub_order::active_instances, 4);
}

// --- Memory & Resource Management ---

TEST_F(PriceQueueTest, DestructorCleansAllNodes) {
    {
        price_queue<stub_order> pq;
        pq.enqueue(stub_order(order{1}));
        pq.enqueue(stub_order(order{2}));
        pq.enqueue(stub_order(order{3}));
        
        // 3 data nodes + 2 sentinels = 5 active instances
        EXPECT_EQ(stub_order::active_instances, 5);
    }
    // After destruction, all instances (including sentinels) must be 0 
    EXPECT_EQ(stub_order::active_instances, 0);
}

// --- Move Semantics ---

TEST_F(PriceQueueTest, MoveConstructor) {
    price_queue<stub_order> pq1;
    pq1.enqueue(stub_order{ order{100} });
    
    price_queue<stub_order> pq2(std::move(pq1));
    
    EXPECT_EQ(pq2.size(), 1);
    EXPECT_EQ(pq1.size(), 0);
}

TEST_F(PriceQueueTest, MoveAssignmentSelfAssignment) {
    price_queue<stub_order> pq;
    stub_order res;
    pq.enqueue(stub_order{ order{10} });
    
    // Should not crash or lose data 
    pq = std::move(pq);

    ASSERT_EQ(pq.size(), 1);
    pq.front(res);
    ASSERT_EQ(res.id(), 10);
}

TEST_F(PriceQueueTest, MoveAssignmentCleanup) {
    price_queue<stub_order> pq1;
    pq1.enqueue(stub_order(order{1}));
    EXPECT_EQ(stub_order::active_instances, 3); // 2 data + 2 sentinels

    {
        price_queue<stub_order> pq2;
        pq2.enqueue(stub_order(order{2}));
        pq2.enqueue(stub_order(order{3}));
	EXPECT_EQ(stub_order::active_instances, 7); // 2 data + 2 sentinels

        pq1 = std::move(pq2); // pq1's old node (ID 1) should be destroyed here 

	ASSERT_EQ(pq2.size(), 0);
	EXPECT_EQ(stub_order::active_instances, 4); // 2 data + 2 sentinels
    }
    // Only pq2's new nodes and sentinels should remain
    EXPECT_EQ(stub_order::active_instances, 4); // 2 data + 2 sentinels
    EXPECT_EQ(pq1.size(), 2);
}

TEST_F(PriceQueueTest, MoveConstructionSentinelDeletionSafety) {
    price_queue<stub_order> pq1;
    price_queue<stub_order> pq2;
    price_queue<stub_order> pq3;
    EXPECT_EQ(stub_order::active_instances, 6); // 2 sentinels each queue

    pq3 = std::move(pq1); // pq1 is in moved state (no sentinels)
    EXPECT_EQ(stub_order::active_instances, 4); // 2 sentinels destroyed in pq1, 4 remain

    pq3 = std::move(pq2); // pq2 is also in moved state
    EXPECT_EQ(stub_order::active_instances, 2); // 2 sentinels destroyed in pq2, 2 remain

    pq1 = std::move(pq2); // pq2 is already moved, no sentinels (resources) should be destroyed.
    EXPECT_EQ(stub_order::active_instances, 2); // no change

    price_queue<stub_order> pq4 = std::move(pq1); // move construction with moved object 
						       // shouldn't make any new elements
    EXPECT_EQ(stub_order::active_instances, 2); // no change
}

TEST_F(PriceQueueTest, PriceQueueIterator) {
    price_queue<stub_order> pq;
    using pqit = price_queue<stub_order>::iterator;

    {
	stub_order first{ order{1} };
	pqit it{ pq.enqueue(std::move(first)) };

	EXPECT_EQ(static_cast<aux::id_t>(1), (*it).data.id());
    }

    EXPECT_EQ(pq.size(), 1);
    EXPECT_EQ(stub_order::active_instances, 3);

    {
	stub_order second{ order{2} };
	pqit it{ pq.enqueue(std::move(second)) };

	EXPECT_EQ(static_cast<aux::id_t>(2), (*it).data.id());
    }
}

TEST_F(PriceQueueTest, PriceQueueBeginEnd) {
    price_queue<stub_order> pq;
    auto it = pq.begin();

    EXPECT_EQ(pq.end(), it);

    {
	stub_order o{ order{1} };
	pq.enqueue(std::move(o));
    }

    EXPECT_EQ(stub_order::active_instances, 3);
    EXPECT_EQ(pq.size(), 1);
    EXPECT_NE(pq.begin(), pq.end());

    EXPECT_EQ(pq.begin()->data.id(), 1);

    {
	for (aux::id_t i{2}; i <= 4; ++i) {
	    pq.enqueue(stub_order{ order{i} });
	}

	aux::id_t i{0};
	for (auto& val : pq) {
	    EXPECT_EQ(val.data.id(), ++i);
	}
    }
}

