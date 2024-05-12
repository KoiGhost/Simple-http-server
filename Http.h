#pragma once
#include <stdint.h>

class Http
{
private:
    static const int MAX_HEADER_LEN = 1024;

public:
    void UnImplementedMethod(int client_fd);
    void UnImplementedQuery(int client_fd);
    void NotFound(int client_fd);
    void Forbidden(int client_fd);
    void BadRequest(int client_fd);
    int HeartBeat(int client_fd);
    int SendHeader(int client_fd,const char*path,uint64_t file_size);
};

