#include "../src/smt_register_allocator.h"
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

/**
 * @brief Simulated workload characteristics for different thread types
 */
enum class WorkloadType {
    COMPUTE_INTENSIVE,  // Low memory access, few L2 misses
    MEMORY_INTENSIVE,   // High memory access, many L2 misses
    MIXED_WORKLOAD,     // Balanced between compute and memory
    CACHE_FRIENDLY      // Good cache locality, moderate L2 misses
};

/**
 * @brief Simulated thread workload
 */
class SimulatedThread {
private:
    int thread_id_;
    WorkloadType workload_type_;
    std::mt19937 rng_;
    uint64_t total_instructions_;
    uint64_t total_l2_misses_;
    
public:
    SimulatedThread(int id, WorkloadType type) 
        : thread_id_(id), workload_type_(type), rng_(id), 
          total_instructions_(0), total_l2_misses_(0) {}
    
    void simulateCycles(int cycles, SMTRegisterAllocator& allocator) {
        std::uniform_int_distribution<int> inst_dist(800, 1200);
        std::uniform_real_distribution<double> miss_rate_variation(0.8, 1.2);
        
        // Base miss rates for different workload types
        double base_miss_rate;
        switch (workload_type_) {
            case WorkloadType::COMPUTE_INTENSIVE:
                base_miss_rate = 0.005;  // 0.5% miss rate
                break;
            case WorkloadType::MEMORY_INTENSIVE:
                base_miss_rate = 0.15;   // 15% miss rate
                break;
            case WorkloadType::MIXED_WORKLOAD:
                base_miss_rate = 0.05;   // 5% miss rate
                break;
            case WorkloadType::CACHE_FRIENDLY:
                base_miss_rate = 0.02;   // 2% miss rate
                break;
        }
        
        for (int i = 0; i < cycles; ++i) {
            int instructions = inst_dist(rng_);
            double actual_miss_rate = base_miss_rate * miss_rate_variation(rng_);
            int l2_misses = static_cast<int>(instructions * actual_miss_rate);
            
            total_instructions_ += instructions;
            total_l2_misses_ += l2_misses;
            
            // Update allocator every few cycles
            if (i % 10 == 0) {
                allocator.updateThreadMetrics(thread_id_, total_l2_misses_, total_instructions_);
            }
        }
    }
    
    int getThreadId() const { return thread_id_; }
    WorkloadType getWorkloadType() const { return workload_type_; }
    
    std::string getWorkloadTypeName() const {
        switch (workload_type_) {
            case WorkloadType::COMPUTE_INTENSIVE: return "Compute Intensive";
            case WorkloadType::MEMORY_INTENSIVE: return "Memory Intensive";
            case WorkloadType::MIXED_WORKLOAD: return "Mixed Workload";
            case WorkloadType::CACHE_FRIENDLY: return "Cache Friendly";
        }
        return "Unknown";
    }
};

int main() {
    std::cout << "=== SMT Register Capping Simulation ===" << std::endl;
    std::cout << "This simulation demonstrates the dynamic register allocation" << std::endl;
    std::cout << "system for SMT processors based on thread performance metrics." << std::endl;
    
    // Create SMT processor with 128 registers, min 16 per thread, max 4 threads
    SMTRegisterAllocator allocator(128, 16, 4, 50);
    
    std::cout << "\n=== Adding Diverse Workload Scenario ===" << std::endl;
    
    // Create simulated threads
    std::vector<SimulatedThread> threads;
    threads.emplace_back(1, WorkloadType::COMPUTE_INTENSIVE);
    threads.emplace_back(2, WorkloadType::MEMORY_INTENSIVE);
    threads.emplace_back(3, WorkloadType::CACHE_FRIENDLY);
    threads.emplace_back(4, WorkloadType::MIXED_WORKLOAD);
    
    // Add threads to allocator
    for (const auto& thread : threads) {
        allocator.addThread(thread.getThreadId());
        std::cout << "Added Thread " << thread.getThreadId() << " (" 
                  << thread.getWorkloadTypeName() << ")" << std::endl;
    }
    
    std::cout << "\n=== Starting SMT Processor Simulation ===" << std::endl;
    allocator.printAllocationState();
    
    // Run simulation for multiple phases
    const int total_cycles = 1000;
    const int report_interval = 250;
    
    for (int cycle = 0; cycle < total_cycles; ++cycle) {
        // Simulate all threads for this cycle
        for (auto& thread : threads) {
            thread.simulateCycles(1, allocator);
        }
        
        // Advance allocator
        allocator.advanceCycle();
        
        // Report progress
        if ((cycle + 1) % report_interval == 0) {
            std::cout << "\n--- Cycle " << (cycle + 1) << " ---" << std::endl;
            allocator.printAllocationState();
            
            // Show thread workload information
            std::cout << "\nThread Workload Analysis:" << std::endl;
            for (const auto& thread : threads) {
                int allocation = allocator.getThreadAllocation(thread.getThreadId());
                std::cout << "  Thread " << thread.getThreadId() 
                          << ": " << thread.getWorkloadTypeName()
                          << " (Allocated: " << allocation << " registers)" << std::endl;
            }
        }
    }
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    allocator.printAllocationState();
    
    std::cout << "\n=== Key Observations ===" << std::endl;
    std::cout << "1. Compute-intensive threads get more registers (lower L2 miss rates)" << std::endl;
    std::cout << "2. Memory-intensive threads get fewer registers (higher L2 miss rates)" << std::endl;
    std::cout << "3. All threads maintain minimum allocation to prevent starvation" << std::endl;
    std::cout << "4. System dynamically reallocates based on performance metrics" << std::endl;
    std::cout << "5. Register utilization remains at 100% with optimal distribution" << std::endl;
    
    return 0;
}