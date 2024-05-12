#include "TaskQueue.h"

pthread_mutex_t TaskQueue::task_mutex;

TaskQueue::TaskQueue()
{
    pthread_mutex_init(&task_mutex,NULL);
}

TaskQueue::~TaskQueue()
{
    pthread_mutex_destroy(&task_mutex);
}

void TaskQueue::TaskQueue_Add(std::function<void(std::shared_ptr<void>)> func, std::shared_ptr<void> args)
{
     pthread_mutex_lock(&task_mutex);
    Task NewTask(func,args);
    this->task_queue.emplace(NewTask);
    pthread_mutex_unlock(&task_mutex);
}

Task TaskQueue::Task_Get(){
    Task OutTask;
    pthread_mutex_lock(&task_mutex);
    if(this->task_queue.size()>0){
        OutTask = task_queue.front();
        this->task_queue.pop();
    }
    pthread_mutex_unlock(&task_mutex);
    return OutTask;
}


