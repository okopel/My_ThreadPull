/**
 * Ori Kopel
 * okopel@gmail.com
 * 205533151
 * EX4_threadPool
 */

#include <pthread.h>
#include <unistd.h>
#include "threadPool.h"
#include "osqueue.h"
#include "stdlib.h"
#include <string.h>
#include "stdio.h"

#define RUNNING (1)
#define ERR_EXIT (-1)
#define NO_RUNNING (0)
#define STDERR 2
#define ERR_MSG "ERROR in System Call\n"
#define NO_WAIT_FLAG (0)

/**
 * Write Error about system call ti STDERR 2
 */
void writeError();

/**
 * Free memory of malloc
 * @param threadPool
 */
void freeMem(ThreadPool *threadPool);

/**
 * All the level after error in system call incluse exit -1
 * @param threadpool the tp
 */
void writeErrorAndFreeMemAndExit(ThreadPool *threadpool);

/**
 * the thread whice manage the function
 * @param param the threadpool
 * @return NULL
 */
static void *waitingThread(void *param);

/**
 * Create threadpool
 * @param numOfThreads how much thread
 * @return pointer to tp
 */
ThreadPool *tpCreate(int numOfThreads) {
    if (numOfThreads < 1) {
        exit(ERR_EXIT);
    }
    ThreadPool *threadPool;

    if ((threadPool = (ThreadPool *) malloc(sizeof(ThreadPool))) == NULL) {
        writeErrorAndFreeMemAndExit(threadPool);
    }
    if (pthread_mutex_init(&threadPool->mutex, NULL) != 0) {
        writeErrorAndFreeMemAndExit(threadPool);
    }
    if (pthread_cond_init(&threadPool->notify, NULL) != 0) {
        writeErrorAndFreeMemAndExit(threadPool);
    }
    threadPool->tasks = osCreateQueue();
    if (threadPool->tasks == NULL) {
        writeErrorAndFreeMemAndExit(threadPool);
    }
    threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * numOfThreads);
    if (threadPool->threads == NULL) {
        writeErrorAndFreeMemAndExit(threadPool);
    }
    threadPool->numOfThread = numOfThreads;
    threadPool->status = RUNNING;
    int i;
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&threadPool->threads[i], NULL, waitingThread, (void *) threadPool) != 0) {
            writeErrorAndFreeMemAndExit(threadPool);
        }
    }

    return threadPool;
}

/**
 * insert new task
 * @param threadPool the threadpoo
 * @param computeFunc the func
 * @param param her args
 * @return 0 if succes
 */
int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
    if (threadPool->status == NO_RUNNING || (computeFunc == NULL)) {
        return -1;
    }
    Task *t = (Task *) malloc(sizeof(Task));
    if (t == NULL) {
        writeErrorAndFreeMemAndExit(threadPool);
    }
    t->computeFunc = computeFunc;
    t->param = param;
    pthread_mutex_lock(&threadPool->mutex);
    osEnqueue(threadPool->tasks, t);
    pthread_mutex_unlock(&threadPool->mutex);

    pthread_cond_signal(&(threadPool->notify));

    return 0;
}

/**
 * destroy the tp
 * @param threadPool tp
 * @param shouldWaitForTasks 0 not wait, else wait
 */
void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
    if ((threadPool == NULL) || (shouldWaitForTasks < 0) || (shouldWaitForTasks > 1) ||
        threadPool->status == NO_RUNNING) {
        return;
    }
    if (shouldWaitForTasks == NO_WAIT_FLAG) {
        pthread_mutex_lock(&(threadPool->mutex));
        while (!osIsQueueEmpty(threadPool->tasks)) {
            Task *t = osDequeue(threadPool->tasks);
            free(t);
        }
        pthread_mutex_unlock(&(threadPool->mutex));
    }
    threadPool->status = NO_RUNNING;
    pthread_cond_broadcast(&(threadPool->notify));
    int i;
    for (i = 0; i < threadPool->numOfThread; i++) {
        if (pthread_join(threadPool->threads[i], NULL) != 0) {
            writeErrorAndFreeMemAndExit(threadPool);
        }
        pthread_cond_broadcast(&(threadPool->notify));
    }
    //all threads gone
    freeMem(threadPool);
}

void writeErrorAndFreeMemAndExit(ThreadPool *threadpool) {
    writeError();
    freeMem(threadpool);
    exit(-1);
}

void writeError() {
    write(STDERR, ERR_MSG, strlen(ERR_MSG));
}

void freeMem(ThreadPool *threadPool) {
    if (threadPool == NULL) {
        return;
    }
    pthread_cond_broadcast(&(threadPool->notify));
    pthread_mutex_lock(&threadPool->mutex);

    while (!osIsQueueEmpty(threadPool->tasks)) {
        Task *t = osDequeue(threadPool->tasks);
        free(t);
    }
    if (threadPool->tasks != NULL) {
        osDestroyQueue(threadPool->tasks);
        threadPool->tasks = NULL;
    }
    pthread_mutex_unlock(&threadPool->mutex);
    pthread_mutex_destroy(&threadPool->mutex);
    pthread_cond_destroy(&threadPool->notify);
    if (threadPool->threads != NULL) {
        free(threadPool->threads);
        threadPool->threads = NULL;
    }
    free(threadPool);
}

void *waitingThread(void *param) {
    ThreadPool *threadPool = (ThreadPool *) param;
    if (threadPool == NULL) {
        return NULL;
    }
    Task *myTask;
    while (1) {
        pthread_mutex_lock(&threadPool->mutex);
        while (osIsQueueEmpty(threadPool->tasks)) {
            if (threadPool->status == NO_RUNNING && osIsQueueEmpty(threadPool->tasks)) {
                pthread_mutex_unlock(&threadPool->mutex);
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&threadPool->mutex);
            if (osIsQueueEmpty(threadPool->tasks) && threadPool->status == RUNNING) {
                pthread_cond_wait(&(threadPool->notify), &(threadPool->mutex));
            }
            if (threadPool->status == NO_RUNNING && osIsQueueEmpty(threadPool->tasks)) {
                pthread_mutex_unlock(&threadPool->mutex);
                pthread_exit(NULL);
            }

        }//end of while of yes empty
        myTask = osDequeue(threadPool->tasks);
        pthread_mutex_unlock(&(threadPool->mutex));
        (myTask->computeFunc)(myTask->param);
        free(myTask);
    }
}
