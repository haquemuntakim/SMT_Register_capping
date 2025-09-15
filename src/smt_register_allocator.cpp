#include "smt_register_allocator.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <cassert>

SMTRegisterAllocator::SMTRegisterAllocator(int total_registers, int min_registers_per_thread, 
                                         int max_threads, uint64_t reallocation_interval)
    : total_registers_(total_registers)
    , min_registers_per_thread_(min_registers_per_thread)
    , max_threads_(max_threads)
    , allocation_cycles_(0)
    , reallocation_interval_(reallocation_interval) {
    
    // Validate parameters
    assert(total_registers > 0);
    assert(min_registers_per_thread > 0);
    assert(max_threads > 0);
    assert(min_registers_per_thread * max_threads <= total_registers);
    
    threads_.reserve(max_threads);
}

bool SMTRegisterAllocator::addThread(int thread_id) {
    // Check if thread already exists
    if (thread_id_to_index_.find(thread_id) != thread_id_to_index_.end()) {
        return false;
    }
    
    // Check if we have space for more threads
    if (threads_.size() >= static_cast<size_t>(max_threads_)) {
        return false;
    }
    
    // Add new thread with minimum allocation
    auto new_thread = std::make_unique<ThreadMetrics>(thread_id);
    new_thread->allocated_registers = min_registers_per_thread_;
    
    thread_id_to_index_[thread_id] = threads_.size();
    threads_.push_back(std::move(new_thread));
    
    // Reallocate to distribute registers properly
    reallocateRegisters();
    
    return true;
}

bool SMTRegisterAllocator::removeThread(int thread_id) {
    auto it = thread_id_to_index_.find(thread_id);
    if (it == thread_id_to_index_.end()) {
        return false;
    }
    
    int index = it->second;
    
    // Remove thread from vector (swap with last element)
    if (index < static_cast<int>(threads_.size()) - 1) {
        std::swap(threads_[index], threads_.back());
        // Update the mapping for the swapped thread
        thread_id_to_index_[threads_[index]->thread_id] = index;
    }
    
    threads_.pop_back();
    thread_id_to_index_.erase(it);
    
    // Reallocate remaining registers
    if (!threads_.empty()) {
        reallocateRegisters();
    }
    
    return true;
}

void SMTRegisterAllocator::updateThreadMetrics(int thread_id, uint64_t l2_misses, uint64_t instructions) {
    auto it = thread_id_to_index_.find(thread_id);
    if (it == thread_id_to_index_.end()) {
        return;
    }
    
    ThreadMetrics& thread = *threads_[it->second];
    thread.l2_misses = l2_misses;
    thread.instructions = instructions;
    
    // Calculate miss rate
    if (instructions > 0) {
        thread.miss_rate = static_cast<double>(l2_misses) / instructions;
    } else {
        thread.miss_rate = 0.0;
    }
}

int SMTRegisterAllocator::getThreadAllocation(int thread_id) const {
    auto it = thread_id_to_index_.find(thread_id);
    if (it == thread_id_to_index_.end()) {
        return -1;
    }
    
    return threads_[it->second]->allocated_registers;
}

void SMTRegisterAllocator::advanceCycle() {
    allocation_cycles_++;
    
    if (isReallocationNeeded()) {
        calculateMissRates();
        reallocateRegisters();
        allocation_cycles_ = 0;
    }
}

void SMTRegisterAllocator::forceReallocation() {
    calculateMissRates();
    reallocateRegisters();
    allocation_cycles_ = 0;
}

int SMTRegisterAllocator::getActiveThreadCount() const {
    return static_cast<int>(threads_.size());
}

std::pair<int, int> SMTRegisterAllocator::getUtilizationStats() const {
    int allocated = 0;
    for (const auto& thread : threads_) {
        allocated += thread->allocated_registers;
    }
    return {allocated, total_registers_};
}

bool SMTRegisterAllocator::validateMinimumCaps() const {
    for (const auto& thread : threads_) {
        if (thread->allocated_registers < min_registers_per_thread_) {
            return false;
        }
    }
    return true;
}

void SMTRegisterAllocator::calculateMissRates() {
    // Miss rates are updated in updateThreadMetrics, but we can normalize here if needed
    // This method can be extended for more sophisticated calculations
}

void SMTRegisterAllocator::reallocateRegisters() {
    if (threads_.empty()) {
        return;
    }
    
    // Start with minimum allocation for all threads
    int reserved_registers = threads_.size() * min_registers_per_thread_;
    int available_registers = total_registers_ - reserved_registers;
    
    // Initialize all threads with minimum allocation
    for (auto& thread : threads_) {
        thread->allocated_registers = min_registers_per_thread_;
    }
    
    if (available_registers <= 0) {
        return; // No extra registers to distribute
    }
    
    // Sort threads by performance (lower miss rate = better performance)
    std::vector<std::pair<double, int>> thread_performance;
    for (size_t i = 0; i < threads_.size(); ++i) {
        // Use inverse of miss rate as performance metric (higher is better)
        // Add small epsilon to avoid division by zero
        double performance = 1.0 / (threads_[i]->miss_rate + 1e-10);
        thread_performance.push_back({performance, static_cast<int>(i)});
    }
    
    // Sort by performance (highest first)
    std::sort(thread_performance.begin(), thread_performance.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Distribute extra registers based on performance
    double total_performance = 0.0;
    for (const auto& perf : thread_performance) {
        total_performance += perf.first;
    }
    
    if (total_performance > 0) {
        for (const auto& perf : thread_performance) {
            int thread_idx = perf.second;
            double performance_ratio = perf.first / total_performance;
            int extra_registers = static_cast<int>(available_registers * performance_ratio);
            
            threads_[thread_idx]->allocated_registers += extra_registers;
            available_registers -= extra_registers;
        }
        
        // Distribute remaining registers (due to rounding) to best performing thread
        if (available_registers > 0 && !thread_performance.empty()) {
            int best_thread_idx = thread_performance[0].second;
            threads_[best_thread_idx]->allocated_registers += available_registers;
        }
    } else {
        // If no performance data, distribute equally
        int extra_per_thread = available_registers / threads_.size();
        int remainder = available_registers % threads_.size();
        
        for (size_t i = 0; i < threads_.size(); ++i) {
            threads_[i]->allocated_registers += extra_per_thread;
            if (static_cast<int>(i) < remainder) {
                threads_[i]->allocated_registers += 1;
            }
        }
    }
}

bool SMTRegisterAllocator::isReallocationNeeded() const {
    return allocation_cycles_ >= reallocation_interval_;
}

int SMTRegisterAllocator::calculateOptimalAllocation(const ThreadMetrics& /* thread */) const {
    // This is a placeholder for more sophisticated allocation algorithms
    // Could consider factors like:
    // - Historical performance trends
    // - Instruction types being executed
    // - Cache hierarchy behavior
    return min_registers_per_thread_;
}

void SMTRegisterAllocator::printAllocationState() const {
    std::cout << "\n=== SMT Register Allocation State ===" << std::endl;
    std::cout << "Total Registers: " << total_registers_ << std::endl;
    std::cout << "Min Registers per Thread: " << min_registers_per_thread_ << std::endl;
    std::cout << "Active Threads: " << threads_.size() << "/" << max_threads_ << std::endl;
    std::cout << "Allocation Cycles: " << allocation_cycles_ << "/" << reallocation_interval_ << std::endl;
    
    auto [allocated, total] = getUtilizationStats();
    std::cout << "Register Utilization: " << allocated << "/" << total 
              << " (" << std::fixed << std::setprecision(1) 
              << (100.0 * allocated / total) << "%)" << std::endl;
    
    std::cout << "\nThread Details:" << std::endl;
    std::cout << std::left << std::setw(8) << "Thread"
              << std::setw(12) << "Registers"
              << std::setw(12) << "L2 Misses"
              << std::setw(15) << "Instructions"
              << std::setw(12) << "Miss Rate" << std::endl;
    std::cout << std::string(59, '-') << std::endl;
    
    for (const auto& thread : threads_) {
        std::cout << std::left << std::setw(8) << thread->thread_id
                  << std::setw(12) << thread->allocated_registers
                  << std::setw(12) << thread->l2_misses
                  << std::setw(15) << thread->instructions
                  << std::setw(12) << std::fixed << std::setprecision(4) 
                  << thread->miss_rate << std::endl;
    }
    std::cout << "======================================" << std::endl;
}

std::vector<ThreadMetrics> SMTRegisterAllocator::getThreadMetrics() const {
    std::vector<ThreadMetrics> metrics;
    for (const auto& thread : threads_) {
        metrics.push_back(*thread);
    }
    return metrics;
}