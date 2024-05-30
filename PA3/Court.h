#ifndef COURT_H
#define COURT_H

#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <stdexcept>
#include <vector>

class Court {
private:
    int player_count;
    bool has_referee;
    bool match_in_progress;
    int current_players;
    int waiting_players;
    sem_t court_lock;
    sem_t match_start;
    sem_t match_end;
    pthread_mutex_t mutex;
    pthread_mutex_t print_mutex;
    pthread_t referee_thread;

    void init() {
        sem_init(&court_lock, 0, 1);
        sem_init(&match_start, 0, 0);
        sem_init(&match_end, 0, 0);
        pthread_mutex_init(&mutex, NULL);
        pthread_mutex_init(&print_mutex, NULL);
        match_in_progress = false;
        current_players = 0;
        waiting_players = 0;
    }

public:
    Court(int player_count, bool has_referee) {
        if (player_count <= 0 || (has_referee != 0 && has_referee != 1)) {
            throw std::invalid_argument("Invalid arguments");
        }
        this->player_count = player_count;
        this->has_referee = has_referee;
        init();
    }

    ~Court() {
        sem_destroy(&court_lock);
        sem_destroy(&match_start);
        sem_destroy(&match_end);
        pthread_mutex_destroy(&mutex);
        pthread_mutex_destroy(&print_mutex);
    }

    void enter() {
        pthread_t tid = pthread_self();

        pthread_mutex_lock(&print_mutex);
        printf("Thread ID: %lu, I have arrived at the court.\n", tid);
        pthread_mutex_unlock(&print_mutex);

        sem_wait(&court_lock);

        pthread_mutex_lock(&mutex);
        if (match_in_progress) {
            waiting_players++;
            pthread_mutex_unlock(&mutex);
            sem_post(&court_lock);
            sem_wait(&match_start);
            pthread_mutex_lock(&mutex);
            waiting_players--;
        } else {
            current_players++;
            if (current_players == player_count + has_referee) {
                match_in_progress = true;
                pthread_mutex_lock(&print_mutex);
                printf("Thread ID: %lu, There are enough players, starting a match.\n", tid);
                pthread_mutex_unlock(&print_mutex);

                if (has_referee) {
                    referee_thread = tid;
                }
                for (int i = 0; i < current_players - 1; ++i) {
                    sem_post(&match_start);
                }
            } else {
                pthread_mutex_lock(&print_mutex);
                printf("Thread ID: %lu, There are only %d players, passing some time.\n", tid, current_players);
                pthread_mutex_unlock(&print_mutex);

                pthread_mutex_unlock(&mutex);
                sem_post(&court_lock);
                return;
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    void leave() {
        pthread_t tid = pthread_self();

        pthread_mutex_lock(&mutex);
        if (!match_in_progress) {
            pthread_mutex_unlock(&mutex);
            pthread_mutex_lock(&print_mutex);
            printf("Thread ID: %lu, I was not able to find a match and I have to leave.\n", tid);
            pthread_mutex_unlock(&print_mutex);
            return;
        }

        current_players--;
        if (has_referee && pthread_equal(tid, referee_thread)) {
            pthread_mutex_lock(&print_mutex);
            printf("Thread ID: %lu, I am the referee and now, I am leaving.\n", tid);
            pthread_mutex_unlock(&print_mutex);
        } else {
            pthread_mutex_lock(&print_mutex);
            printf("Thread ID: %lu, I am a player and now, I am leaving.\n", tid);
            pthread_mutex_unlock(&print_mutex);
        }

        if (current_players == 0) {
            pthread_mutex_lock(&print_mutex);
            printf("Thread ID: %lu, everybody left, letting any waiting people know.\n", tid);
            pthread_mutex_unlock(&print_mutex);

            match_in_progress = false;
            for (int i = 0; i < waiting_players; ++i) {
                sem_post(&match_start);
            }
            sem_post(&court_lock);
        }
        pthread_mutex_unlock(&mutex);
    }

    void play();
};

#endif // COURT_H
