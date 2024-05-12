#include <getopt.h>
#include <string>
#include "ThreadPool.h"
#include "Server.h"
#include <unistd.h>
#include <memory>

void test(std::shared_ptr<void>)
{
    while (1)
    {
        sleep(2);
        printf("thread %p wake up\n",(void*)pthread_self());
        break;
    }
    
}


int main(){
    const char* password = "1234";
    Server server(9000,5,12,password);
    
    
    // ThreadPool thread_pool;
    // thread_pool.ThreadPool_Create(5,12);
    // for(int i =0 ;i<5;i++)
    // {
    //     thread_pool.ThreadPool_Add(test,nullptr);
    // }
    // sleep(3);
    

}