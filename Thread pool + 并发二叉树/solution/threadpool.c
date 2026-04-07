#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

static void *worker_thread(void *arg) {
    threadpool_t *pool = (threadpool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        /* Sleep while nothing to do and pool is alive */
        while (pool->queue_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }

        /* Shutdown path: drain the queue first, then exit */
        if (pool->queue_size == 0 && pool->shutdown) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }

        /* Dequeue one task (circular buffer) */
        task_t task = pool->queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1) % pool->queue_capacity;
        pool->queue_size--;

        /* A slot opened up - wake a blocked submitter if any */
        pthread_cond_signal(&pool->not_full);

        pthread_mutex_unlock(&pool->lock);

        /* Execte the task without holding the lock */
        task.func(task.arg);
    }
}

threadpool_t *threadpool_create(int num_threads, int queue_capacity) {
    // TODO
    if (num_threads <= 0 || queue_capacity <= 0) {
        return NULL;
    }

    threadpool_t *pool = malloc(sizeof(threadpool_t));
    if (!pool) return NULL;

    /* Allocate the task queue and thread handle array */
    pool->queue = malloc(sizeof(task_t) * queue_capacity);
    if (!pool->queue) {
        free(pool);
        return NULL;
    }

    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        free(pool->queue);
        free(pool);
        return NULL;
    }

    /* Initialize integer fields */
    pool->num_threads = num_threads;
    pool->queue_capacity = queue_capacity;
    pool->queue_size = 0;
    pool->queue_head = 0;
    pool->queue_tail = 0;
    pool->shutdown = 0;

    /* Initialize mutex and condition variables */
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool->threads);
        free(pool->queue);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&pool->lock);
        free(pool->threads);
        free(pool->queue);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->not_full, NULL) != 0) {
        pthread_cond_destroy(&pool->not_empty);
        pthread_mutex_destroy(&pool->lock);
        free(pool->threads);
        free(pool->queue);
        free(pool);
        return NULL;
    }

    /* Span worker threads */
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            pthread_mutex_lock(&pool->lock);
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->not_empty);
            pthread_mutex_unlock(&pool->lock);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL); 
            }
            pthread_cond_destroy(&pool->not_full);
            pthread_cond_destroy(&pool->not_empty);
            pthread_mutex_destroy(&pool->lock);
            free(pool->threads);
            free(pool->queue);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

int threadpool_submit(threadpool_t *pool, void (*func)(void *), void *arg) {
    // TODO
    pthread_mutex_lock(&pool->lock);

    /* Wait while the queue is full (and the pool is still alive)*/
    while (pool->queue_size == pool->queue_capacity && !pool->shutdown) {
        pthread_cond_wait(&pool->not_full, &pool->lock);
    }

    /* If we were woken becuase of shutdown, bail out */
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    /* Enqueue the task (circular buffer) */
    pool->queue[pool->queue_tail].func = func;
    pool->queue[pool->queue_tail].arg  = arg;
    pool->queue_tail = (pool->queue_tail + 1) % pool->queue_capacity; // point to the next rewirte place
    pool->queue_size++;

    /* Wake one worker */
    pthread_cond_signal(&pool->not_empty);

    pthread_mutex_unlock(&pool->lock);
    return 0;

}

void threadpool_destroy(threadpool_t *pool) {
    // TODO
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    /* Wake workers blocked on empty queue */
    pthread_cond_broadcast(&pool->not_empty);
    /* Wake workers blocked on full queue */
    pthread_cond_broadcast(&pool->not_full);
    pthread_mutex_unlock(&pool->lock);

    /* Wait for every woker to finish */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    /* Clean up */
    pthread_cond_destroy(&pool->not_full); 
    pthread_cond_destroy(&pool->not_empty);
    pthread_mutex_destroy(&pool->lock);
    free(pool->threads);
    free(pool->queue);
    free(pool);
}
