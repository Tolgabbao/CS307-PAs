// Tolga Toker 32639
#ifndef QUEUE_H
#define QUEUE_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <algorithm>
#include <utility>
#include <limits>
#include <climits> 
#include <array>
#include <pthread.h>
#include <mutex>

typedef struct __node_t {
    pthread_t value;
    struct __node_t *next;
} node_t;

typedef struct __queue_t {
    node_t *head;
    node_t *tail;
    pthread_mutex_t headLock;
    pthread_mutex_t tailLock;
} queue_t;

void Queue_Init(queue_t *q) {
    node_t *tmp = (node_t*)malloc(sizeof(node_t));
    tmp->next = NULL;
    q->head = q->tail = tmp;
    pthread_mutex_init(&q->headLock, NULL);
    pthread_mutex_init(&q->tailLock, NULL);
}

void Queue_Enqueue(queue_t *q, pthread_t value) {
    node_t *tmp = (node_t*)malloc(sizeof(node_t));
    assert(tmp != NULL);
    tmp->value = value;
    tmp->next = NULL;

    pthread_mutex_lock(&q->tailLock);
    q->tail->next = tmp;
    q->tail = tmp;
    pthread_mutex_unlock(&q->tailLock);
}

pthread_t Queue_Dequeue(queue_t *q) {
    pthread_mutex_lock(&q->headLock);
    node_t *tmp = q->head;
    node_t *newHead = tmp->next;
    if (newHead == NULL) {
        pthread_mutex_unlock(&q->headLock);
        return 0; // queue was empty
    }
    pthread_t value = newHead->value;
    q->head = newHead;
    pthread_mutex_unlock(&q->headLock);
    free(tmp);
    return value;
}

template <typename T>
class Queue {
private:
    queue_t queue;
public:
    Queue() {
        Queue_Init(&queue);
    }

    void enqueue(const T& item) {
        Queue_Enqueue(&queue, item);
    }

    T dequeue() {
        return Queue_Dequeue(&queue);
    }

    bool isEmpty() {
        // Acquire headLock before accessing head and next
        pthread_mutex_lock(&queue.headLock);
        node_t *tmp = queue.head;
        node_t *newHead = tmp->next;
        pthread_mutex_unlock(&queue.headLock);
        return newHead == NULL;
    }

    void print() {
        // Acquire headLock for the entire traversal
        pthread_mutex_lock(&queue.headLock);
        node_t *tmp = queue.head;
        node_t *newHead = tmp->next;
        while (newHead != NULL) {
            std::cout << newHead->value << " ";
            std::cout.flush();
            tmp = newHead;
            newHead = tmp->next;
        }
        std::cout << std::endl;
        std::cout.flush();
        pthread_mutex_unlock(&queue.headLock);
    }
};

#endif // QUEUE_H