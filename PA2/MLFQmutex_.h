#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <pthread.h>
#include <vector>
#include <unordered_map>
#include <iostream>


#include "park.h"
#include "queue_.h"

class MLFQMutex {
private:
    int num_levels;
    double quantum;
    std::vector<Queue<pthread_t>> queues;
    std::unordered_map<pthread_t, int> thread_levels;
    Garage garage;
    std::mutex mtx;
    std::atomic<bool> flag;
    std::chrono::steady_clock::time_point start;
public:
    MLFQMutex(int num_levels, double quantum) : num_levels(num_levels), quantum(quantum), flag(false) {
        for (int i = 0; i < num_levels; i++) {
            Queue<pthread_t> q;
            queues.push_back(q);
        }
    }

    void lock() {
        if (thread_levels.find(pthread_self()) == thread_levels.end()) {
            thread_levels[pthread_self()] = 0;
        }
        start = std::chrono::steady_clock::now();
        while (flag.exchange(true)) {
            int current_level = thread_levels[pthread_self()];
            if (current_level < num_levels && !queues[current_level].isEmpty()) {
                // Round-robin scheduling
                pthread_t next_thread = queues[current_level].dequeue();
                queues[current_level].enqueue(pthread_self());
                garage.unpark(next_thread);
            }
            else {
                queues[current_level].enqueue(pthread_self());
                garage.park();
            }
        }
        flag.store(true);
    }

    void unlock(){
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        int execTime = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        int current_level = thread_levels[pthread_self()];
        if (execTime <= quantum) {
            garage.unpark(queues[current_level].dequeue());
            flag.store(false);
            thread_levels[pthread_self()] = current_level;
            /*
            cout << "Thread with program ID: " << pthread_self() << " and thread ID: " << pthread_self() << " finished in level " << current_level << endl;
            cout.flush();
            */
            return;
        }
        else {
            int new_level = current_level + (execTime / quantum);
            if (new_level >= num_levels) {
                new_level = num_levels - 1;
            }
            thread_levels[pthread_self()] = new_level;
            queues[new_level].enqueue(pthread_self());
        }
        garage.unpark(queues[current_level].dequeue());
        flag.store(false);
        /*
        cout << "Thread with program ID: " << pthread_self() << " and thread ID: " << pthread_self() << " moved to level " << thread_levels[pthread_self()] << endl;
        cout.flush();
        */
        return;
    }

    void print(){
        for (int i = 0; i < num_levels; i++) {
            std::cout << "Level " << i << ": ";
            std::cout.flush();
            queues[i].print();
        }
    }

};
