#include <gtest/gtest.h>

#include "containers/price_queue.hpp"

using namespace aux::containers;

// Helper class to track construction and destruction
struct ResourceTracker {
    static inline int active_instances = 0;
    int id;

    ResourceTracker() noexcept : id(-1) { active_instances++; }
    ResourceTracker(int val) noexcept : id(val) { active_instances++; }
    
    // Requirements for price_queue: nothrow move and default constructible
    ResourceTracker(ResourceTracker&& other) noexcept : id(other.id) {
        active_instances++;
    }

    ResourceTracker& operator=(ResourceTracker&& other) noexcept {
        id = other.id;
        return *this;
    }

    ~ResourceTracker() { active_instances--; }
};

class PriceQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        ResourceTracker::active_instances = 0;
    }
};

// --- Core Functionality ---

TEST_F(PriceQueueTest, InitialState) {
    price_queue<int> pq;
    EXPECT_EQ(pq.size(), 0);
    EXPECT_EQ(pq.front(), 0);
}

TEST_F(PriceQueueTest, EnqueueAndLinkage) {
    price_queue<int> pq;
    auto* node1 = pq.enqueue(10);
    auto* node2 = pq.enqueue(20);

    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);
    
    // Verify Double Linkage via public Node struct 
    EXPECT_EQ(node1->data, 10);
    EXPECT_EQ(node2->data, 20);
    EXPECT_EQ(node1->next, node2);
    EXPECT_EQ(node2->prev, node1);
}

TEST_F(PriceQueueTest, RemoveMiddleNode) {
    price_queue<ResourceTracker> pq;
    auto* first = pq.enqueue(ResourceTracker(10));
    auto* mid = pq.enqueue(ResourceTracker(20));
    auto* last = pq.enqueue(ResourceTracker(30));
    EXPECT_EQ(ResourceTracker::active_instances, 5);

    pq.remove(mid);

    // Verify Double Linkage via public Node struct 
    EXPECT_EQ(first->data.id, 10);
    EXPECT_EQ(last->data.id, 30);
    EXPECT_EQ(first->next, last);
    EXPECT_EQ(last->prev, first);
    EXPECT_EQ(pq.size(), 2);
    EXPECT_EQ(ResourceTracker::active_instances, 4);
}

// --- Memory & Resource Management ---

TEST_F(PriceQueueTest, DestructorCleansAllNodes) {
    {
        price_queue<ResourceTracker> pq;
        pq.enqueue(ResourceTracker(1));
        pq.enqueue(ResourceTracker(2));
        pq.enqueue(ResourceTracker(3));
        
        // 3 data nodes + 2 sentinels = 5 active instances
        EXPECT_EQ(ResourceTracker::active_instances, 5);
    }
    // After destruction, all instances (including sentinels) must be 0 
    EXPECT_EQ(ResourceTracker::active_instances, 0);
}

// --- Move Semantics ---

TEST_F(PriceQueueTest, MoveConstructor) {
    price_queue<int> pq1;
    pq1.enqueue(100);
    
    price_queue<int> pq2(std::move(pq1));
    
    EXPECT_EQ(pq2.size(), 1);
    EXPECT_EQ(pq1.size(), 0);
}

TEST_F(PriceQueueTest, MoveAssignmentSelfAssignment) {
    price_queue<int> pq;
    pq.enqueue(10);
    
    // Should not crash or lose data 
    pq = std::move(pq);
    SUCCEED();
}

TEST_F(PriceQueueTest, MoveAssignmentCleanup) {
    price_queue<ResourceTracker> pq1;
    pq1.enqueue(ResourceTracker(1));
    EXPECT_EQ(ResourceTracker::active_instances, 3); // 2 data + 2 sentinels

    {
        price_queue<ResourceTracker> pq2;
        pq2.enqueue(ResourceTracker(2));
        pq2.enqueue(ResourceTracker(3));
	EXPECT_EQ(ResourceTracker::active_instances, 7); // 2 data + 2 sentinels

        pq1 = std::move(pq2); // pq1's old node (ID 1) should be destroyed here 

	ASSERT_EQ(pq2.size(), 0);
	EXPECT_EQ(ResourceTracker::active_instances, 4); // 2 data + 2 sentinels
    }
    // Only pq2's new nodes and sentinels should remain
    EXPECT_EQ(ResourceTracker::active_instances, 4); // 2 data + 2 sentinels
    EXPECT_EQ(pq1.size(), 2);
}

TEST_F(PriceQueueTest, MoveConstructionSentinelDeletionSafety) {
    price_queue<ResourceTracker> pq1;
    price_queue<ResourceTracker> pq2;
    price_queue<ResourceTracker> pq3;
    EXPECT_EQ(ResourceTracker::active_instances, 6); // 2 sentinels each queue

    pq3 = std::move(pq1); // pq1 is in moved state (no sentinels)
    EXPECT_EQ(ResourceTracker::active_instances, 4); // 2 sentinels destroyed in pq1, 4 remain

    pq3 = std::move(pq2); // pq2 is also in moved state
    EXPECT_EQ(ResourceTracker::active_instances, 2); // 2 sentinels destroyed in pq2, 2 remain

    pq1 = std::move(pq2); // pq2 is already moved, no sentinels (resources) should be destroyed.
    EXPECT_EQ(ResourceTracker::active_instances, 2); // no change

    price_queue<ResourceTracker> pq4 = std::move(pq1); // move construction with moved object 
						       // shouldn't make any new elements
    EXPECT_EQ(ResourceTracker::active_instances, 2); // no change
}
