#pragma once
#include <functional>
#include <memory>
#include <queue>

// 定义一个任务结构体
// 其中包含一个函数指针和一个指向任意对象的指针
struct Task
{
    Task(){
        func = NULL;
        args = NULL;
    }
    Task(std::function<void(std::shared_ptr<void>)> f,std::shared_ptr<void> args){
        func = f;
        this->args = args;
    }
    std::function<void(std::shared_ptr<void>)> func;
    std::shared_ptr<void> args;
};


class TaskQueue
{
private:
    static pthread_mutex_t task_mutex;
    std::queue<Task> task_queue;
public:
    TaskQueue();
    ~TaskQueue();

    void TaskQueue_Add(std::function<void(std::shared_ptr<void>)> func, std::shared_ptr<void> args);

    Task Task_Get();

    inline int TaskNums(){
        return task_queue.size();
    }
};
