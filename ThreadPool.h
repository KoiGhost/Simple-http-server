#pragma once
#include <functional>
#include <memory>
#include <vector>
#include "TaskQueue.h"
#include "Err.h"



class ThreadPool
{
public:
    ~ThreadPool();

    static int ThreadPool_Add(std::function<void(std::shared_ptr<void>)> fun, std::shared_ptr<void> args);
    static int ThreadPool_Create(int min_nums, int max_nums);
    static int ThreadPool_Destroy();
    static int ThreadPool_Free();
    static void ThreadPool_ThreadExit();
    static void *ThreadPool_WorkerThread(void *args);
    static void *ThreadPool_Manager(void *args);

private:
    static pthread_mutex_t threadpool_lock;
    static pthread_cond_t thread_notify;

    static std::vector<pthread_t> worker_threads;
    static TaskQueue* task_queue;
    static pthread_t manager_thread;
    static int min_thread_nums;
    static int max_thread_nums;
    // excuting threads
    static int worker_thread_nums;
    // excuting and waiting to excute threads
    static int alive_thread_nums;
    static int exit_thread_nums;
    static bool shutdown;

};