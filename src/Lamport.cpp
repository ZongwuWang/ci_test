#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <numeric> // For std::iota
#include <algorithm> // For std::max_element
#include <chrono>    // For timing
#include <vector>
#include <iomanip> // For std::setw

// --- Configuration ---
const int NUM_THREADS = 8;           // Number of threads to run in parallel
const int ITERATIONS_PER_THREAD = 10000; // Number of times each thread enters the critical section
const bool ENABLE_WORK_SIMULATION = true; // Add a small delay in the critical section?
const std::chrono::microseconds WORK_DELAY(1); // Small delay to simulate work

// --- Lamport's Bakery Algorithm Shared Data ---
// Use std::vector<std::atomic<bool/int>> for dynamic sizing if NUM_THREADS isn't constant,
// but for fixed size, arrays are slightly simpler. Ensure MAX_THREADS is large enough.
const int MAX_THREADS = 64; // Max supported threads by these arrays
std::atomic<bool> choosing[MAX_THREADS];
std::atomic<int> number[MAX_THREADS];

// --- Verification Data ---
std::atomic<long long> shared_counter(0);           // Counter incremented in the critical section
std::atomic<int> inside_critical_section_count(0); // Counter to check mutual exclusion directly
std::atomic<bool> mutual_exclusion_violated(false); // Flag set if violation detected

// --- Lamport's Bakery Lock Implementation ---
// --- Lamport's Bakery Lock Implementation ---
void lock(int thread_id) { // The ID of the current thread
    // 1. Indicate intention to choose a number
    choosing[thread_id].store(true, std::memory_order_seq_cst);

    // 2. Choose a number: max(number[0..N-1]) + 1
    int max_num = 0;
    for (int i = 0; i < NUM_THREADS; ++i) { // 'i' is defined here for this loop only
        int current_num = number[i].load(std::memory_order_relaxed);
        if (current_num > max_num) {
            max_num = current_num;
        }
    }
    number[thread_id].store(max_num + 1, std::memory_order_seq_cst);

    // 3. Indicate number selection is complete
    choosing[thread_id].store(false, std::memory_order_seq_cst);

    // 4. Wait for other threads
    for (int j = 0; j < NUM_THREADS; ++j) { // Loop iterates through other threads with index 'j'
        // Don't compare with self
        // CORRECTED LINE: Use 'thread_id' instead of 'i'
        if (thread_id == j) continue;

        // Wait while thread j is choosing its number
        while (choosing[j].load(std::memory_order_seq_cst)) {
            std::this_thread::yield();
        }

        // Wait while thread j has a number and has higher priority:
        // Priority: (number[j], j) < (number[thread_id], thread_id)
        int num_j;
        while ( (num_j = number[j].load(std::memory_order_seq_cst)) != 0 &&
                ( (num_j < number[thread_id].load(std::memory_order_seq_cst)) ||
                  (num_j == number[thread_id].load(std::memory_order_seq_cst) && j < thread_id)
                )
              )
        {
            std::this_thread::yield();
        }
    }
    // --- At this point, thread_id has acquired the lock ---
}

void unlock(int thread_id) {
    // Reset the number to indicate exiting the critical section
    number[thread_id].store(0, std::memory_order_seq_cst); // Release semantics ensure prior writes are visible
}

// --- Worker Thread Function ---
void worker_thread(int id) {
    for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
        lock(id);

        // --- Critical Section Start ---
        int current_inside = inside_critical_section_count.fetch_add(1, std::memory_order_relaxed);
        // **Verification:** Check if more than one thread is inside
        if (current_inside > 0) { // If > 0, it means *another* thread was already inside when we incremented
             mutual_exclusion_violated.store(true, std::memory_order_relaxed);
             // You could print an error immediately, but a flag is often cleaner for aggregation
        }

        // Actual work in the critical section
        shared_counter.fetch_add(1, std::memory_order_relaxed); // Increment shared counter

        // Simulate some work to increase contention probability
        if (ENABLE_WORK_SIMULATION) {
             std::this_thread::sleep_for(WORK_DELAY);
        }

         // **Verification:** Double check immediately before leaving (optional but good sanity check)
        if (inside_critical_section_count.load(std::memory_order_relaxed) > 1) {
             mutual_exclusion_violated.store(true, std::memory_order_relaxed);
        }
        inside_critical_section_count.fetch_sub(1, std::memory_order_relaxed); // Decrement before unlocking
        // --- Critical Section End ---

        unlock(id);

        // Optional: Do some non-critical work outside the lock
        // std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
}

// --- Main Function ---
int main() {
    if (NUM_THREADS > MAX_THREADS) {
        std::cerr << "Error: NUM_THREADS exceeds MAX_THREADS." << std::endl;
        return 1;
    }

    std::cout << "--- Lamport's Bakery Algorithm Verification ---" << std::endl;
    std::cout << "Number of Threads: " << NUM_THREADS << std::endl;
    std::cout << "Iterations per Thread: " << ITERATIONS_PER_THREAD << std::endl;
    std::cout << "Total Expected Increments: " << (long long)NUM_THREADS * ITERATIONS_PER_THREAD << std::endl;
    std::cout << "Simulating Work in CS: " << (ENABLE_WORK_SIMULATION ? "Yes" : "No") << std::endl;
     if (ENABLE_WORK_SIMULATION) {
        std::cout << "Work Delay per CS: " << WORK_DELAY.count() << " us" << std::endl;
     }
    std::cout << "---------------------------------------------" << std::endl;


    // Initialize shared arrays
    for (int i = 0; i < NUM_THREADS; ++i) {
        choosing[i].store(false);
        number[i].store(0);
    }
    shared_counter.store(0);
    inside_critical_section_count.store(0);
    mutual_exclusion_violated.store(false);


    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    // --- Start Timer ---
    auto start_time = std::chrono::high_resolution_clock::now();

    // Create and launch threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker_thread, i); // Pass thread ID 'i'
    }

    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (threads[i].joinable()) {
            threads[i].join();
        }
    }

    // --- Stop Timer ---
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // --- Verification and Results ---
    long long final_count = shared_counter.load();
    long long expected_count = (long long)NUM_THREADS * ITERATIONS_PER_THREAD;
    bool count_correct = (final_count == expected_count);
    bool mutex_ok = !mutual_exclusion_violated.load();

    std::cout << "\n--- Results ---" << std::endl;
    std::cout << "Execution Time: " << duration.count() << " ms" << std::endl;

    std::cout << "\n--- Correctness Verification ---" << std::endl;
    std::cout << "Final Shared Counter: " << final_count << std::endl;
    std::cout << "Expected Counter:     " << expected_count << std::endl;
    std::cout << "Counter Value Correct? " << (count_correct ? "Yes" : "No") << std::endl;
    std::cout << "Mutual Exclusion Preserved? " << (mutex_ok ? "Yes" : "No") << std::endl;

    if (count_correct && mutex_ok) {
        std::cout << "\nSUCCESS: Lamport's Bakery Algorithm appears correct." << std::endl;
    } else {
        std::cerr << "\nERROR: Lamport's Bakery Algorithm failed verification!" << std::endl;
        if (!count_correct) std::cerr << " - Final counter value mismatch." << std::endl;
        if (!mutex_ok) std::cerr << " - Mutual exclusion violation detected." << std::endl;
         return 1; // Indicate failure
    }

    return 0;
}
