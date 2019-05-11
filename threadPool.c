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
#include <asm/errno.h>
#include "stdio.h"

#define RUNNING (1)
#define ERR_EXIT (-1)
#define GOOD_EXIT (0)
#define NO_RUNNING (0)
#define STDERR (2)
#define ERR_MSG "ERROR in System Call\n"
#define NO_WAIT_FLAG (0)


/**
 * Write Error about system call ti STDERR 2
 * @param who num of caller
 */
void writeError(int who);

/**
 * Free memory of malloc
 * @param threadPool
 * @param isFromError if error or destroy
 */
void freeMem(ThreadPool *threadPool);

/**
 * All the level after error in system call incluse exit -1
 * @param threadpool the tp
 */
void writeErrorAndFreeMemAndExit(ThreadPool *threadpool, int who);

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
        writeErrorAndFreeMemAndExit(threadPool, 1);
    }
    if (pthread_mutex_init(&threadPool->mutex, NULL) != 0) {
        writeErrorAndFreeMemAndExit(threadPool, 2);
    }
    if (pthread_cond_init(&threadPool->notify, NULL) != 0) {
        writeErrorAndFreeMemAndExit(threadPool, 3);
    }
    threadPool->tasks = osCreateQueue();
    if (threadPool->tasks == NULL) {
        writeErrorAndFreeMemAndExit(threadPool, 4);
    }
    threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * numOfThreads);
    if (threadPool->threads == NULL) {
        writeErrorAndFreeMemAndExit(threadPool, 5);
    }
    threadPool->numOfThread = numOfThreads;
    threadPool->status = RUNNING;
    int i;
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&threadPool->threads[i], NULL, waitingThread, (void *) threadPool) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 6);
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
    if ((threadPool->status == NO_RUNNING) || (computeFunc == NULL)) {
        return ERR_EXIT;
    }
    Task *t = (Task *) malloc(sizeof(Task));
    if (t == NULL) {
        writeErrorAndFreeMemAndExit(threadPool, 7);
    }
    t->computeFunc = computeFunc;
    t->param = param;
    bool isTasksListEmp = osIsQueueEmpty(threadPool->tasks);
    //enter task pointer and his parameters to tasksQueue
    if ((pthread_mutex_lock(&threadPool->mutex)) != 0) {
        writeErrorAndFreeMemAndExit(threadPool, 8);
    }
    osEnqueue(threadPool->tasks, t);
    if ((pthread_mutex_unlock(&threadPool->mutex)) != 0) {
        writeErrorAndFreeMemAndExit(threadPool, 9);
    }
    //notify the threads that task added because there are in wait mode
    //the signal sent just if the queue is empty because this is the case that thread isnt busy
    if (isTasksListEmp) {
        if ((pthread_cond_broadcast(&(threadPool->notify))) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 10);
        }
    }
    return GOOD_EXIT;
}

/**
 * destroy the tp
 * @param threadPool tp
 * @param shouldWaitForTasks 0 not wait, else wait
 */
void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
    //NULL threadpool or the threadpool has already destroyed
    if ((threadPool == NULL) || threadPool->status == NO_RUNNING) {
        return;
    }
    //not wait case is to end the cur tasks without the tasks in the queue
    if (shouldWaitForTasks == NO_WAIT_FLAG) {
        //delete all the tasks from the queue
        if ((pthread_mutex_lock(&threadPool->mutex)) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 11);
        }
        while (!osIsQueueEmpty(threadPool->tasks)) {
            Task *t = osDequeue(threadPool->tasks);
            free(t);
        }
        if ((pthread_mutex_unlock(&(threadPool->mutex))) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 12);
        }
    }
    threadPool->status = NO_RUNNING;
    //wakeup the threads (just if the queue is empty->there are waiting threads)
    if (osIsQueueEmpty(threadPool->tasks)) {
        if ((pthread_cond_broadcast(&(threadPool->notify))) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 13);
        }
    }

    //waiting to the threads to be done
    int i;
    for (i = 0; i < threadPool->numOfThread; i++) {
        if (pthread_join(threadPool->threads[i], NULL) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 14);
        }
    }
    //all threads gone so we can free the allocated memories
    freeMem(threadPool);
}

void writeErrorAndFreeMemAndExit(ThreadPool *threadpool, int who) {
    writeError(ERR_EXIT);
    freeMem(threadpool);
    exit(ERR_EXIT);
}

void writeError(int who) {
    write(STDERR, ERR_MSG, strlen(ERR_MSG));
}

void freeMem(ThreadPool *threadPool) {
    if (threadPool == NULL) {
        return;
    }
    //let the threads wakeup
    if ((threadPool->status == RUNNING) && (osIsQueueEmpty(threadPool->tasks))) {
        if ((pthread_cond_broadcast(&(threadPool->notify))) != 0) {
            writeError(16);
        }
    }
    //clean the tasks queue
    if ((pthread_mutex_lock(&(threadPool->mutex))) != 0) {
        writeError(17);
    }
    if (threadPool->tasks != NULL) {
        while (!osIsQueueEmpty(threadPool->tasks)) {
            Task *t = osDequeue(threadPool->tasks);
            free(t);
        }
    }
    if (threadPool->tasks != NULL) {
        osDestroyQueue(threadPool->tasks);
        threadPool->tasks = NULL;
    }
    if ((pthread_mutex_unlock(&(threadPool->mutex))) != 0) {
        writeError(18);
    }

    if ((pthread_mutex_destroy(&(threadPool->mutex))) != 0) {
        writeError(19);
    }
    if ((pthread_cond_destroy(&(threadPool->notify))) != 0) {
        writeError(20);
    }
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
    //the loop will go until tpDestroy will exit the thread
    //there isn't busy waiting because pthread_wait in the loop
    while (1) {
        if ((pthread_mutex_lock(&threadPool->mutex)) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 21);
        }        //if queue is empty -> wait else->do the task
        while (osIsQueueEmpty(threadPool->tasks)) {
            //tpDestroy has called, and there are not tasks to do so exit
            if ((threadPool->status == NO_RUNNING) && (osIsQueueEmpty(threadPool->tasks))) {
                if ((pthread_mutex_unlock(&(threadPool->mutex))) != 0) {
                    writeErrorAndFreeMemAndExit(threadPool, 22);
                }
                pthread_exit(NULL);
            }
            //we have to run but this time we dont have task so wait until signal
            if (osIsQueueEmpty(threadPool->tasks) && (threadPool->status == RUNNING)) {
                if ((pthread_cond_wait(&(threadPool->notify), &(threadPool->mutex))) != 0) {
                    writeErrorAndFreeMemAndExit(threadPool, 24);
                }
            }
        }//end of while of empty
        //take the task and goto work
        myTask = osDequeue(threadPool->tasks);
        if ((pthread_mutex_unlock(&(threadPool->mutex))) != 0) {
            writeErrorAndFreeMemAndExit(threadPool, 25);
        }
        (myTask->computeFunc)(myTask->param);
        free(myTask);
    }
}
