#ifndef SMT_REGISTER_ALLOCATOR_H
#define SMT_REGISTER_ALLOCATOR_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

/**
 * @brief Thread performance metrics for register allocation decisions
 */
struct ThreadMetrics {
    uint64_t l2_misses;          // Number of L2 cache misses
    uint64_t instructions;       // Number of instructions executed
    double miss_rate;            // L2 miss rate (misses/instructions)
    int allocated_registers;     // Currently allocated rename registers
    int thread_id;               // Thread identifier
    
    ThreadMetrics(int id) : l2_misses(0), instructions(0), miss_rate(0.0), 
                           allocated_registers(0), thread_id(id) {}
};

/**
 * @brief SMT Register Allocator for dynamic rename register distribution
 * 
 * Implements a performance-based register allocation strategy where threads
 * with better performance (fewer L2 misses) receive larger portions of the
 * rename register file, while maintaining minimum caps to prevent starvation.
 */
class SMTRegisterAllocator {
private:
    int total_registers_;                                    // Total rename registers available
    int min_registers_per_thread_;                          // Minimum registers guaranteed per thread
    int max_threads_;                                       // Maximum number of threads supported
    std::vector<std::unique_ptr<ThreadMetrics>> threads_;   // Thread metrics and state
    std::unordered_map<int, int> thread_id_to_index_;      // Map thread ID to index
    
    // Performance tracking
    uint64_t allocation_cycles_;                            // Cycles since last reallocation
    uint64_t reallocation_interval_;                        // How often to reallocate (in cycles)
    
    // Helper methods
    void calculateMissRates();
    void reallocateRegisters();
    bool isReallocationNeeded() const;
    int calculateOptimalAllocation(const ThreadMetrics& thread) const;
    
public:
    /**
     * @brief Constructor for SMT Register Allocator
     * @param total_registers Total number of rename registers available
     * @param min_registers_per_thread Minimum registers guaranteed per thread
     * @param max_threads Maximum number of threads supported
     * @param reallocation_interval Cycles between reallocation decisions
     */
    SMTRegisterAllocator(int total_registers, int min_registers_per_thread, 
                        int max_threads, uint64_t reallocation_interval = 1000);
    
    /**
     * @brief Add a new thread to the system
     * @param thread_id Unique thread identifier
     * @return true if thread was successfully added, false if system is full
     */
    bool addThread(int thread_id);
    
    /**
     * @brief Remove a thread from the system
     * @param thread_id Thread identifier to remove
     * @return true if thread was successfully removed, false if not found
     */
    bool removeThread(int thread_id);
    
    /**
     * @brief Update performance metrics for a thread
     * @param thread_id Thread identifier
     * @param l2_misses Number of L2 cache misses
     * @param instructions Number of instructions executed
     */
    void updateThreadMetrics(int thread_id, uint64_t l2_misses, uint64_t instructions);
    
    /**
     * @brief Get current register allocation for a thread
     * @param thread_id Thread identifier
     * @return Number of registers allocated to the thread, -1 if thread not found
     */
    int getThreadAllocation(int thread_id) const;
    
    /**
     * @brief Advance the allocator by one cycle and check for reallocation
     */
    void advanceCycle();
    
    /**
     * @brief Force immediate reallocation of registers
     */
    void forceReallocation();
    
    /**
     * @brief Get total number of active threads
     * @return Number of active threads
     */
    int getActiveThreadCount() const;
    
    /**
     * @brief Get system utilization statistics
     * @return Pair of (allocated_registers, total_registers)
     */
    std::pair<int, int> getUtilizationStats() const;
    
    /**
     * @brief Check if the system maintains minimum caps for all threads
     * @return true if all threads have at least minimum allocation
     */
    bool validateMinimumCaps() const;
    
    // Debug and monitoring methods
    void printAllocationState() const;
    std::vector<ThreadMetrics> getThreadMetrics() const;
};

#endif // SMT_REGISTER_ALLOCATOR_H