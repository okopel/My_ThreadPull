/**
 * Ori Kopel
 * okopel@gmail.com
 * 205533151
 * EX4_threadPool
 */
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <sys/types.h>
#include <stdbool.h>

typedef struct thread_pool {
    pthread_mutex_t mutex;
    pthread_cond_t notify;
    struct os_queue *tasks;
    pthread_t *threads;
    bool status;
    int numOfThread;
} ThreadPool;

typedef struct task {
    void (*computeFunc)(void *);

    void *param;

} Task;


ThreadPool *tpCreate(int numOfThreads);

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param);

#endif
