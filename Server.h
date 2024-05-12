#pragma once
#include <memory>
#include "ThreadPool.h"
#include "Http.h"

class Server
{
public:
    Server(int port,int min_threads,int max_threads,const char* password);
    ~Server();

    void handNewConn(std::shared_ptr<void> args);
    void WaitNewConn();
    int socket_bind_listen(int port);
    void handle_for_sigpipe();
    void DiscardRemainData(int client_fd);
    void heartbeat(std::shared_ptr<void> args);
    int recv_one_line(int client_fd,char* buf,int size);
    void send_file(int client_fd,const char* path);
private:
    enum method_type_enum {GET,POST};
    int port_;
    int listenFd_;
    ThreadPool thread_pool;
    Http http;
    const char* password_;
    static const int MAXFDS = 100;
    static const int MAX_LINE_LEN = 1024;
    static const int HEARTBEAT_INTERVAL = 30;
};