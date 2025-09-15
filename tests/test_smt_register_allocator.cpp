#include "../src/smt_register_allocator.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <random>

/**
 * @brief Test suite for SMT Register Allocator
 */
class SMTRegisterAllocatorTest {
private:
    int total_tests_;
    int passed_tests_;
    
    void assertEqual(int expected, int actual, const std::string& test_name) {
        total_tests_++;
        if (expected == actual) {
            passed_tests_++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            std::cout << "[FAIL] " << test_name << " - Expected: " << expected 
                      << ", Got: " << actual << std::endl;
        }
    }
    
    void assertTrue(bool condition, const std::string& test_name) {
        total_tests_++;
        if (condition) {
            passed_tests_++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            std::cout << "[FAIL] " << test_name << " - Condition was false" << std::endl;
        }
    }
    
public:
    SMTRegisterAllocatorTest() : total_tests_(0), passed_tests_(0) {}
    
    void testBasicAllocation() {
        std::cout << "\n--- Testing Basic Allocation ---" << std::endl;
        
        // Create allocator with 64 registers, minimum 8 per thread, max 4 threads
        SMTRegisterAllocator allocator(64, 8, 4);
        
        // Test adding threads
        assertTrue(allocator.addThread(1), "Add thread 1");
        assertTrue(allocator.addThread(2), "Add thread 2");
        assertEqual(2, allocator.getActiveThreadCount(), "Active thread count");
        
        // Check that threads get at least minimum allocation
        assertTrue(allocator.getThreadAllocation(1) >= 8, "Thread 1 at least minimum allocation");
        assertTrue(allocator.getThreadAllocation(2) >= 8, "Thread 2 at least minimum allocation");
        
        // Check that minimum caps are maintained
        assertTrue(allocator.validateMinimumCaps(), "Minimum caps validation");
        
        // Test removing threads
        assertTrue(allocator.removeThread(1), "Remove thread 1");
        assertEqual(1, allocator.getActiveThreadCount(), "Active thread count after removal");
        assertEqual(-1, allocator.getThreadAllocation(1), "Removed thread allocation");
    }
    
    void testPerformanceBasedAllocation() {
        std::cout << "\n--- Testing Performance-Based Allocation ---" << std::endl;
        
        SMTRegisterAllocator allocator(64, 8, 4);
        
        // Add three threads
        allocator.addThread(1);
        allocator.addThread(2);
        allocator.addThread(3);
        
        // Update thread metrics - thread 1 has low miss rate (good performance)
        allocator.updateThreadMetrics(1, 100, 10000);  // 1% miss rate
        allocator.updateThreadMetrics(2, 500, 10000);  // 5% miss rate
        allocator.updateThreadMetrics(3, 1000, 10000); // 10% miss rate
        
        // Force reallocation based on performance
        allocator.forceReallocation();
        
        // Thread 1 should get the most registers (best performance)
        int thread1_alloc = allocator.getThreadAllocation(1);
        int thread2_alloc = allocator.getThreadAllocation(2);
        int thread3_alloc = allocator.getThreadAllocation(3);
        
        assertTrue(thread1_alloc >= 8, "Thread 1 minimum allocation maintained");
        assertTrue(thread2_alloc >= 8, "Thread 2 minimum allocation maintained");
        assertTrue(thread3_alloc >= 8, "Thread 3 minimum allocation maintained");
        
        assertTrue(thread1_alloc >= thread2_alloc, "Thread 1 gets more than thread 2");
        assertTrue(thread2_alloc >= thread3_alloc, "Thread 2 gets more than thread 3");
        
        std::cout << "Allocations: Thread1=" << thread1_alloc 
                  << ", Thread2=" << thread2_alloc 
                  << ", Thread3=" << thread3_alloc << std::endl;
    }
    
    void testStarvationPrevention() {
        std::cout << "\n--- Testing Starvation Prevention ---" << std::endl;
        
        SMTRegisterAllocator allocator(32, 4, 4);
        
        // Add maximum threads
        allocator.addThread(1);
        allocator.addThread(2);
        allocator.addThread(3);
        allocator.addThread(4);
        
        // One thread has extremely poor performance
        allocator.updateThreadMetrics(1, 1, 10000);     // 0.01% miss rate (excellent)
        allocator.updateThreadMetrics(2, 10, 10000);    // 0.1% miss rate (good)
        allocator.updateThreadMetrics(3, 100, 10000);   // 1% miss rate (ok)
        allocator.updateThreadMetrics(4, 5000, 10000);  // 50% miss rate (terrible)
        
        allocator.forceReallocation();
        
        // Even the worst performing thread should get minimum allocation
        int thread4_alloc = allocator.getThreadAllocation(4);
        assertTrue(thread4_alloc >= 4, "Worst thread gets minimum allocation");
        assertTrue(allocator.validateMinimumCaps(), "All threads maintain minimum caps");
        
        // Total allocation should not exceed total registers
        auto [allocated, total] = allocator.getUtilizationStats();
        assertTrue(allocated <= total, "Total allocation within bounds");
        assertEqual(32, total, "Total registers correct");
    }
    
    void testCycleBasedReallocation() {
        std::cout << "\n--- Testing Cycle-Based Reallocation ---" << std::endl;
        
        // Short reallocation interval for testing
        SMTRegisterAllocator allocator(64, 8, 4, 10);
        
        allocator.addThread(1);
        allocator.addThread(2);
        
        // Initial allocation
        int initial_alloc1 = allocator.getThreadAllocation(1);
        int initial_alloc2 = allocator.getThreadAllocation(2);
        
        // Update metrics
        allocator.updateThreadMetrics(1, 100, 10000);  // Good performance
        allocator.updateThreadMetrics(2, 1000, 10000); // Poor performance
        
        // Advance cycles but not enough to trigger reallocation
        for (int i = 0; i < 5; ++i) {
            allocator.advanceCycle();
        }
        
        // Allocation should be the same
        assertEqual(initial_alloc1, allocator.getThreadAllocation(1), "No reallocation yet");
        assertEqual(initial_alloc2, allocator.getThreadAllocation(2), "No reallocation yet");
        
        // Advance enough cycles to trigger reallocation
        for (int i = 0; i < 10; ++i) {
            allocator.advanceCycle();
        }
        
        // Allocation should have changed based on performance
        int new_alloc1 = allocator.getThreadAllocation(1);
        int new_alloc2 = allocator.getThreadAllocation(2);
        
        assertTrue(new_alloc1 >= new_alloc2, "Better performing thread gets more registers");
    }
    
    void testEdgeCases() {
        std::cout << "\n--- Testing Edge Cases ---" << std::endl;
        
        // Test adding duplicate thread
        SMTRegisterAllocator allocator(64, 8, 4);
        assertTrue(allocator.addThread(1), "Add thread 1 first time");
        assertTrue(!allocator.addThread(1), "Cannot add duplicate thread");
        
        // Test removing non-existent thread
        assertTrue(!allocator.removeThread(999), "Cannot remove non-existent thread");
        
        // Test getting allocation for non-existent thread
        assertEqual(-1, allocator.getThreadAllocation(999), "Non-existent thread allocation");
        
        // Test maximum thread capacity
        allocator.addThread(2);
        allocator.addThread(3);
        allocator.addThread(4);
        assertTrue(!allocator.addThread(5), "Cannot exceed maximum threads");
        
        // Test with zero instructions (edge case for miss rate)
        allocator.updateThreadMetrics(1, 0, 0);
        allocator.forceReallocation();
        assertTrue(allocator.validateMinimumCaps(), "Handles zero instructions gracefully");
    }
    
    void testSystemUtilization() {
        std::cout << "\n--- Testing System Utilization ---" << std::endl;
        
        SMTRegisterAllocator allocator(128, 16, 4);
        
        // Test empty system
        auto [allocated0, total0] = allocator.getUtilizationStats();
        assertEqual(0, allocated0, "Empty system allocation");
        assertEqual(128, total0, "Total registers unchanged");
        
        // Add threads and check utilization
        allocator.addThread(1);
        allocator.addThread(2);
        
        auto [allocated1, total1] = allocator.getUtilizationStats();
        assertTrue(allocated1 > 0, "Non-zero allocation with threads");
        assertTrue(allocated1 <= 128, "Allocation within bounds");
        assertEqual(128, total1, "Total registers unchanged");
    }
    
    void runAllTests() {
        std::cout << "=== SMT Register Allocator Test Suite ===" << std::endl;
        
        testBasicAllocation();
        testPerformanceBasedAllocation();
        testStarvationPrevention();
        testCycleBasedReallocation();
        testEdgeCases();
        testSystemUtilization();
        
        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Passed: " << passed_tests_ << "/" << total_tests_ << std::endl;
        
        if (passed_tests_ == total_tests_) {
            std::cout << "All tests PASSED!" << std::endl;
        } else {
            std::cout << "Some tests FAILED!" << std::endl;
        }
    }
};

/**
 * @brief Demonstration of the SMT Register Allocator in action
 */
void demonstrateAllocator() {
    std::cout << "\n=== SMT Register Allocator Demonstration ===" << std::endl;
    
    // Create allocator with 128 registers, minimum 16 per thread, max 4 threads
    SMTRegisterAllocator allocator(128, 16, 4, 100);
    
    // Add threads
    allocator.addThread(1);
    allocator.addThread(2);
    allocator.addThread(3);
    
    std::cout << "Initial state:" << std::endl;
    allocator.printAllocationState();
    
    // Simulate different workload characteristics
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> miss_dist(50, 2000);
    std::uniform_int_distribution<> inst_dist(8000, 12000);
    
    // Simulate execution cycles
    for (int cycle = 0; cycle < 500; ++cycle) {
        // Update thread metrics with different performance patterns
        if (cycle % 50 == 0) {
            // Thread 1: Consistently good performance
            allocator.updateThreadMetrics(1, miss_dist(gen) / 10, inst_dist(gen));
            
            // Thread 2: Medium performance
            allocator.updateThreadMetrics(2, miss_dist(gen), inst_dist(gen));
            
            // Thread 3: Poor performance
            allocator.updateThreadMetrics(3, miss_dist(gen) * 2, inst_dist(gen));
            
            if (cycle == 100 || cycle == 200 || cycle == 400) {
                std::cout << "\nAfter " << cycle << " cycles:" << std::endl;
                allocator.printAllocationState();
            }
        }
        
        allocator.advanceCycle();
    }
    
    std::cout << "\nFinal state:" << std::endl;
    allocator.printAllocationState();
}

int main() {
    // Run test suite
    SMTRegisterAllocatorTest test_suite;
    test_suite.runAllTests();
    
    // Run demonstration
    demonstrateAllocator();
    
    return 0;
}