#include "ThreadPool.h"
#include <stdio.h>
#include <string>
#include <unistd.h>


pthread_mutex_t ThreadPool::threadpool_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ThreadPool::thread_notify = PTHREAD_COND_INITIALIZER;
std::vector<pthread_t> ThreadPool::worker_threads;
TaskQueue* ThreadPool::task_queue = new TaskQueue();
int ThreadPool::min_thread_nums = 0;
int ThreadPool::max_thread_nums = 0;
int ThreadPool::worker_thread_nums = 0;
int ThreadPool::alive_thread_nums = 0;
int ThreadPool::exit_thread_nums = 0;
pthread_t ThreadPool::manager_thread;
bool ThreadPool::shutdown = 0;

#define dprintf printf

int ThreadPool::ThreadPool_Create(int min_nums, int max_nums)
{
    do{
        min_thread_nums = min_nums;
        max_thread_nums = max_nums;
        alive_thread_nums = min_nums;

        worker_threads.resize(max_thread_nums);

        pthread_create(&manager_thread,NULL,ThreadPool_Manager,(void*)(0));
        dprintf("Create Manager Thread Success\n");
        try {
                for(int i=0;i<min_nums;i++){
                    if(pthread_create(&worker_threads[i],NULL,ThreadPool_WorkerThread,nullptr) !=0){
                        return THREADPOOL_THREAD_FAILURE;
                    }
                }
        }
        catch  (const std::exception& e){
            dprintf("Exception caught: %s\n",e.what());

        }
        
        dprintf("Create worker Threads Success\n");


    }while(0);

    return 0;

}

int ThreadPool::ThreadPool_Add(std::function<void(std::shared_ptr<void>)> func, std::shared_ptr<void> args)
{
    if(pthread_mutex_lock(&threadpool_lock)!=0){
        return THREADPOOL_LOCK_FAILURE;
    }

    if(shutdown == 1){
        return THREADPOOL_SHUTDOWN;
    }
    // 这里没有对任务队列进行大小限制，会在manager中统一管理任务队列和工作线程数量的关系
    task_queue->TaskQueue_Add(func,args);
    dprintf("Add task success\n");

    pthread_cond_signal(&thread_notify);


    pthread_mutex_unlock(&threadpool_lock);
    return 0;
}


int ThreadPool::ThreadPool_Destroy(){
    int err=0;
    dprintf("Thread pool destroying\n");
    if(pthread_mutex_lock(&threadpool_lock) !=0)
    {
        return THREADPOOL_LOCK_FAILURE;
    }
    do
    {
        if(shutdown){
            err = THREADPOOL_SHUTDOWN;
            break; 
        }
        shutdown = 1;
        pthread_mutex_unlock(&threadpool_lock);

        pthread_join(manager_thread,nullptr);

        pthread_mutex_lock(&threadpool_lock);

        pthread_cond_broadcast(&thread_notify);

        pthread_mutex_unlock(&threadpool_lock);

        for(int i=0;i<alive_thread_nums;i++)
        {
            pthread_join(worker_threads[i],nullptr);
        }

    } while (0);
    
    if(!err){ 
        ThreadPool_Free();
    }
    return err;

}


int ThreadPool::ThreadPool_Free()
{
    if(!shutdown)
    {
        return -1;
    }
    pthread_mutex_lock(&threadpool_lock);
    pthread_mutex_destroy(&threadpool_lock);
    pthread_cond_destroy(&thread_notify);
    return 0;
}

void ThreadPool::ThreadPool_ThreadExit()
{
    pthread_t tid = pthread_self();
    for(int i=0;i<worker_threads.size();i++){
        if(worker_threads[i] == tid)
        {
            worker_threads[i] = 0;
            break;
        }
    }
    dprintf("thread %p closed...\n",(void*)pthread_self());
    pthread_exit(nullptr);
}

void *ThreadPool::ThreadPool_WorkerThread(void* args)
{
    while(true)
    {
        pthread_mutex_lock(&threadpool_lock);
        while(task_queue->TaskNums()==0 && !shutdown)
        {
            dprintf("thread %p waiting...\n",(void*)pthread_self());

            pthread_cond_wait(&thread_notify,&threadpool_lock);

            if(exit_thread_nums > 0)
            {
                exit_thread_nums --;
                if(alive_thread_nums > min_thread_nums)
                {
                    alive_thread_nums--;
                    pthread_mutex_unlock(&threadpool_lock);
                    ThreadPool_ThreadExit();
                }
            }
            
        }

        if(shutdown){
            pthread_mutex_unlock(&threadpool_lock);
            ThreadPool_ThreadExit();
        }

        Task task = task_queue->Task_Get();
        worker_thread_nums++;
        pthread_mutex_unlock(&threadpool_lock);
        dprintf("thread %p start working...\n",(void*)pthread_self());
        task.func(task.args);
        task.args = nullptr;

        dprintf("thread %p end working...\n",(void*)pthread_self());
        pthread_mutex_lock(&threadpool_lock);
        worker_thread_nums--;
        pthread_mutex_unlock(&threadpool_lock);
        
    }
    
    return nullptr;
}


void *ThreadPool::ThreadPool_Manager(void* args)
{
    while(!shutdown)
    {
        sleep(5);
        pthread_mutex_lock(&threadpool_lock);
        int task_num = task_queue->TaskNums();
        int alive_num = alive_thread_nums;
        int work_num = worker_thread_nums;
        pthread_mutex_unlock(&threadpool_lock);

        const int NUMBER = 2;
        if(task_num>alive_num && alive_num < max_thread_nums)
        {
            pthread_mutex_lock(&threadpool_lock);
            int num =0;
            for(int i=0;i<max_thread_nums && num<NUMBER && alive_thread_nums<max_thread_nums;i++)
            {
                if(worker_threads[i]==0)
                {
                    pthread_create(&worker_threads[i],nullptr,ThreadPool_WorkerThread,(void*)(0));
                    num++;
                    alive_thread_nums++;
                }
            }
            pthread_mutex_unlock(&threadpool_lock);
        }

        if(work_num *2 <alive_num && alive_num > min_thread_nums)
        {
            pthread_mutex_lock(&threadpool_lock);
            exit_thread_nums = NUMBER;
            pthread_mutex_unlock(&threadpool_lock);
            for(int i=0;i<NUMBER;i++)
            {
                pthread_cond_signal(&thread_notify);
            }
        }
    }
   // ThreadPool_Destroy();
    //ThreadPool_Free();
    dprintf("manager closed\n");
    return nullptr;
}

ThreadPool::~ThreadPool()
{
    ThreadPool_Destroy();
}