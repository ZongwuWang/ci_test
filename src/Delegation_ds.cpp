#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>

using namespace std;
using namespace std::chrono;

class DelegationLock {
private:
    struct Task {
        function<void()> work;
        promise<void> completion;
    };

    queue<Task> taskQueue;
    mutex queueMutex;
    condition_variable queueCV;
    thread workerThread;
    atomic<bool> running;

    void worker() {
        while (running) {
            unique_lock<mutex> lock(queueMutex);
            queueCV.wait(lock, [this] { return !taskQueue.empty() || !running; });

            if (!running) break;

            Task task = std::move(taskQueue.front());
            taskQueue.pop();
            lock.unlock();

            // Execute the critical section work
            task.work();
            
            // Notify completion
            task.completion.set_value();
        }
    }

public:
    DelegationLock() : running(true) {
        workerThread = thread(&DelegationLock::worker, this);
    }

    ~DelegationLock() {
        running = false;
        queueCV.notify_one();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    future<void> async(function<void()> work) {
        Task task;
        task.work = std::move(work);
        auto fut = task.completion.get_future();

        lock_guard<mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
        queueCV.notify_one();

        return fut;
    }
};

// Test parameters
const int OPERATIONS_PER_THREAD = 10000;
const int MAX_THREADS = 8;
atomic<int> sharedCounter(0);

void workerTask(int id, DelegationLock& lock, int workload) {
    for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        auto fut = lock.async([id, workload] {
            // Critical section
            sharedCounter++;
            
            // Simulate some workload
            volatile int dummy = 0;
            for (int j = 0; j < workload; ++j) {
                dummy += j;
            }
        });
        fut.wait();
    }
}

void testCorrectness(int threadCount, int workload) {
    DelegationLock lock;
    sharedCounter = 0;
    
    vector<thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(workerTask, i, ref(lock), workload);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int expected = threadCount * OPERATIONS_PER_THREAD;
    if (sharedCounter != expected) {
        cout << "Error: Expected " << expected << ", got " << sharedCounter << endl;
    } else {
        cout << "Correctness test passed with " << threadCount 
             << " threads (workload: " << workload << ")" << endl;
    }
}

void testPerformance(int threadCount, int workload) {
    DelegationLock lock;
    sharedCounter = 0;
    
    auto start = high_resolution_clock::now();
    
    vector<thread> threads;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(workerTask, i, ref(lock), workload);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    cout << "Threads: " << threadCount 
         << ", Workload: " << workload
         << ", Time: " << duration << " ms" 
         << ", Throughput: " << (threadCount * OPERATIONS_PER_THREAD * 1000.0 / duration) << " ops/sec" << endl;
}

int main() {
    // Test correctness with different configurations
    cout << "Correctness testing:\n";
    for (int workload : {0, 10, 100}) {
        for (int threads = 1; threads <= MAX_THREADS; threads *= 2) {
            testCorrectness(threads, workload);
        }
    }
    
    // Test performance with different configurations
    cout << "\nPerformance testing:\n";
    for (int workload : {0, 10, 100, 1000}) {
        for (int threads = 1; threads <= MAX_THREADS; threads *= 2) {
            testPerformance(threads, workload);
        }
        cout << endl;
    }
    
    return 0;
}
