#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

using namespace std;
using namespace std::chrono;

class BakeryLock {
private:
    vector<atomic<bool>> choosing;
    vector<atomic<int>> ticket;
    int threadCount;

public:
    BakeryLock(int n) : threadCount(n), choosing(n), ticket(n) {
        for (int i = 0; i < n; ++i) {
            choosing[i] = false;
            ticket[i] = 0;
        }
    }

    void lock(int id) {
        choosing[id] = true;
        
        // Find max ticket and add 1
        int max_ticket = 0;
        for (int i = 0; i < threadCount; ++i) {
            int current = ticket[i];
            max_ticket = max(max_ticket, current);
        }
        ticket[id] = max_ticket + 1;
        choosing[id] = false;

        // Wait until it's our turn
        for (int i = 0; i < threadCount; ++i) {
            if (i == id) continue;
            
            // Wait until thread i finishes choosing
            while (choosing[i]) {
                this_thread::yield();
            }
            
            // Wait until our ticket is the smallest
            while (ticket[i] != 0 && 
                  (ticket[i] < ticket[id] || 
                  (ticket[i] == ticket[id] && i < id))) {
                this_thread::yield();
            }
        }
    }

    void unlock(int id) {
        ticket[id] = 0;
    }
};

// Shared counter for testing
atomic<int> sharedCounter(0);
const int OPERATIONS_PER_THREAD = 100000;

void threadFunction(BakeryLock& lock, int id) {
    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        lock.lock(id);
        // Critical section
        sharedCounter++;
        lock.unlock(id);
    }
}

void testCorrectness(int threadCount) {
    BakeryLock lock(threadCount);
    sharedCounter = 0;
    
    vector<thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(threadFunction, ref(lock), i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int expected = threadCount * OPERATIONS_PER_THREAD;
    if (sharedCounter != expected) {
        cout << "Error: Expected " << expected << ", got " << sharedCounter << endl;
    } else {
        cout << "Correctness test passed with " << threadCount << " threads" << endl;
    }
}

void testPerformance(int threadCount) {
    BakeryLock lock(threadCount);
    sharedCounter = 0;
    
    auto start = high_resolution_clock::now();
    
    vector<thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(threadFunction, ref(lock), i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    cout << "Threads: " << threadCount 
         << ", Time: " << duration << " ms" 
         << ", Counter: " << sharedCounter << endl;
}

int main() {
    // Test correctness with different thread counts
    for (int i = 1; i <= 8; i *= 2) {
        testCorrectness(i);
    }
    
    cout << "\nPerformance testing:\n";
    // Test performance with different thread counts
    for (int i = 1; i <= 8; i *= 2) {
        testPerformance(i);
    }
    
    return 0;
}
