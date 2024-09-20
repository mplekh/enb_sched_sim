#include <gtest/gtest.h>
#include "scheduler.h"

class SchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Code here will run before each test.
    }

    void TearDown() override {
        // Code here will run after each test.
    }
};

TEST_F(SchedulerTest, ReserveBasicTest) {
    Scheduler scheduler(10, 5, 3);
    // Try reserving resources for 2 subframes, data length 1
    unsigned reserved = scheduler.reserve(0, 1, 2);
    // Check if the number of reserved blocks matches expectations
    EXPECT_EQ(reserved, 2);
    EXPECT_EQ(scheduler.success, 2);
    EXPECT_EQ(scheduler.total, 2);
}

TEST_F(SchedulerTest, ReserveInsufficientResourcesTest) {
    Scheduler scheduler(10, 5, 1);
    // Only one resource block per subframe, try to reserve 2 data blocks for 3 UEs
    unsigned reserved = scheduler.reserve(0, 2, 3);
    // It should not be able to reserve 2 blocks for third UE
    EXPECT_EQ(reserved, 2);
    EXPECT_EQ(scheduler.success, 2);
    EXPECT_EQ(scheduler.total, 3);
}

// Test case for avgBlockPerSf method
TEST_F(SchedulerTest, AvgBlockPerSfBasicTest) {
    Scheduler scheduler(10, 5, 3);
    scheduler.reserve(0, 1, 3);
    // Check the average number of reserved blocks per subframe in the first 5 subframes
    double avgBlocks = scheduler.avgBlockPerSf(0, 5);
    EXPECT_GT(avgBlocks, 0.0);  // Should be greater than 0 since some resources were reserved
}

// Test case for avgBlockPerSf with no reservations
TEST_F(SchedulerTest, AvgBlockPerSfNoReservationsTest) {
    Scheduler scheduler(10, 5, 3);
    // Check average blocks when no reservations have been made
    double avgBlocks = scheduler.avgBlockPerSf(0, 5);
    EXPECT_EQ(avgBlocks, 0.0);  // No blocks should have been reserved
}

// Test case for avgBlockPerSf with full reservation
TEST_F(SchedulerTest, AvgBlockPerSfFullReservationTest) {
    Scheduler scheduler(10, 5, 3);

    // Reserve all 3 blocks in 5 subframes
    scheduler.reserve(0, 5, 3);
    // Check average blocks when no reservations have been made
    double avgBlocks = scheduler.avgBlockPerSf(0, 5);
    EXPECT_EQ(avgBlocks, 3.0);  // All 3 blocks in each sf should have been reserved
}

// Test case for printWindow (not much to check programmatically, but check it doesn't crash)
TEST_F(SchedulerTest, PrintWindowTest) {
    Scheduler scheduler(10, 5, 3);
    // Reserve some subframes
    scheduler.reserve(0, 1, 3);
    // Print the window (manually inspect the output)
    scheduler.printWindow(0, 5);
}

// Test case for reserve with multiple data_len
TEST_F(SchedulerTest, ReserveMultipleDataLenTest) {
    Scheduler scheduler(10, 5, 3);

    // Try reserving with different data lengths
    unsigned reserved1 = scheduler.reserve(0, 2, 2);
    EXPECT_EQ(reserved1, 2);  // Should reserve 2
    EXPECT_EQ(scheduler.success, 2);
    EXPECT_EQ(scheduler.total, 2);

    // Try reserving with larger data_len that doesn't fit in the window
    unsigned reserved2 = scheduler.reserve(0, 6, 2);
    EXPECT_EQ(reserved2, 0);  // Should reserve 0 because 6 won't fit
    EXPECT_EQ(scheduler.success, 2);
    EXPECT_EQ(scheduler.total, 4);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
