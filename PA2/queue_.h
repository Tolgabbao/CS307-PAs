#ifndef QUEUE_H
#define QUEUE_H

#include <atomic>
#include <mutex>
#include "concurrentqueue.h"

template <typename T>
class Queue {
private:
    moodycamel::ConcurrentQueue<T> queue;

public:
    void enqueue(const T& item) {
        queue.enqueue(item);
    }

    T dequeue() {
        T item;
        queue.try_dequeue(item);
        return item;
    }

    bool isEmpty() {
        return queue.size_approx() == 0;
    }

    void print() {
        moodycamel::ConcurrentQueue<T> tempQueue;
        T item;
        while (queue.try_dequeue(item)) {
            tempQueue.enqueue(item);
            std::cout << item << " ";
        }
        std::cout << std::endl;
        queue = std::move(tempQueue); // Use std::move() to assign the temporary queue to the main queue
    }
};

#endif