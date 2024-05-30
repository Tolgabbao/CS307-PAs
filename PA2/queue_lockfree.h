#ifndef QUEUE_LOCKFREE_H
#define QUEUE_LOCKFREE_H

#include <atomic>
#include <cstddef>

template <typename T>
class Queue {
private:
    struct Node {
        T data;
        Node* next;
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    Queue() : head(nullptr), tail(nullptr) {}

    ~Queue() {
        while (head.load()) {
            Node* temp = head.load();
            head.store(temp->next);
            delete temp;
        }
    }

    void enqueue(const T& item) {
        Node* newNode = new Node{item, nullptr};

        while (true) {
            Node* currentTail = tail.load();
            Node* next = currentTail->next;

            if (currentTail == tail.load()) {
                if (next == nullptr) {
                    if (__sync_bool_compare_and_swap(&tail->next, nullptr, newNode)) {
                        break; // Enqueue successful
                    }
                } else {
                    // Tail was not pointing to the last node, try to swing tail to the next node
                    __sync_bool_compare_and_swap(&tail, currentTail, next);
                }
            }
        }
        // Try to swing tail to the new node
        __sync_bool_compare_and_swap(&tail, currentTail, newNode);
    }

    bool dequeue(T& item) {
        while (true) {
            Node* currentHead = head.load();
            Node* next = currentHead->next;

            if (currentHead == head.load()) {
                if (next == nullptr) {
                    return false; // Queue is empty
                }

                if (__sync_bool_compare_and_swap(&head, currentHead, next)) {
                    item = next->data;
                    delete currentHead;
                    return true; // Dequeue successful
                }
            }
        }
    }

    bool isEmpty() {
        return head.load() == tail.load();
    }
};

#endif // QUEUE_LOCKFREE_H