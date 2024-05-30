#ifndef MLFQ_MUTEX_H
#define MLFQ_MUTEX_H

#include <pthread.h>
#include <chrono>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <iostream>
#include "queue.h"
#include "park.h"


class MLFQMutex {
private:
    int numLevels;
    double quantum;
    std::atomic<bool> locked;
    std::vector<Queue<pthread_t>> queues;
    std::unordered_map<pthread_t, int> threadLevels;
    std::unordered_map<pthread_t, std::chrono::time_point<std::chrono::system_clock>> threadStartTimes;
    Garage garage;
    pthread_mutex_t mtx;
public:
    MLFQMutex(int numLevels, double quantum) : numLevels(numLevels), quantum(quantum), locked(false) {
        queues.resize(numLevels);
        pthread_mutex_init(&mtx, NULL);
    }

    void lock() {
        pthread_t self = pthread_self();
        int currentLevel = threadLevels.find(self) != threadLevels.end() ? threadLevels[self] : 0;
        bool expected = false;

        // Fast path: try to acquire lock directly
        if (locked.compare_exchange_strong(expected, true)) {
            printf("Thread with ID: %lu acquired lock directly\n", self);
            fflush(stdout);
            threadStartTimes[self] = std::chrono::high_resolution_clock::now();
            return;
        }

        printf("Adding thread with ID: %lu to level %d\n", self, currentLevel);
        fflush(stdout);
        // Slow path: enqueue and park
        pthread_mutex_lock(&mtx);
        queues[currentLevel].enqueue(self);
        garage.setPark();
        pthread_mutex_unlock(&mtx);
        garage.park();
        threadStartTimes[self] = std::chrono::high_resolution_clock::now();
    }

    void unlock() {
        pthread_t self = pthread_self();
        // Stop timer and measure critical section execution time
        auto endTime = std::chrono::high_resolution_clock::now();
        auto startTime = threadStartTimes[self];
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
        // Update thread's priority level
        int levelChange = static_cast<int>(duration / quantum);
        int newLevel = std::min(threadLevels[self] + levelChange, numLevels - 1);
        threadLevels[self] = newLevel;
        // Find next thread to run
        pthread_mutex_lock(&mtx);
        pthread_t nextThread = 0;
        for (int i = 0; i <= numLevels; ++i) {
            if (!queues[i].isEmpty()) {
                nextThread = queues[i].dequeue();
                break;
            }
        }
        pthread_mutex_unlock(&mtx);
        // Release lock and unpark next thread
        if (nextThread != 0) {
            garage.unpark(nextThread);
        }
        else {
            locked.store(false);
        }
    }

    void print() {
        for (int i = 0; i < numLevels; ++i) {
            printf("Level %d: ", i);
            queues[i].print();
        }
    }
};

#endif // MLFQ_MUTEX_H 