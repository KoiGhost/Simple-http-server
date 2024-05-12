#include "Http.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

#define SERVER_STRING "Server: simple http server\r\n"



#define HTTP501M "HTTP/1.1 501 Method Not Implemented\r\n" SERVER_STRING "Content-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>HTTP request method not supported.\r\n</BODY></HTML>\r\n"
void Http::UnImplementedMethod(int client_fd)
{
    send(client_fd,HTTP501M,sizeof(HTTP501M)-1,0);
    printf("501 Method Not Implement\n");
}

#define HTTP501Q "HTTP/1.1 501 Query Not Implemented\r\n" SERVER_STRING "Content-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Query Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>Query not supported.\r\n</BODY></HTML>\r\n"
void Http::UnImplementedQuery(int client_fd)
{
    send(client_fd,HTTP501Q,sizeof(HTTP501Q)-1,0);
    printf("501 Query Not Implement\n");
}

#define HTTP404 "HTTP/1.1 404 NOT FOUND\r\n" SERVER_STRING "Content-Type: text/html\r\n\r\n<HTML><TITLE>Not Found</TITLE>\r\n<BODY><P>File Not Found\r\nthe resource specified\r\nis unavailable or nonexistent.\r\n</BODY></HTML>\r\n"
void Http::NotFound(int client_fd)
{
    send(client_fd,HTTP404,sizeof(HTTP404)-1,0); 
    printf("404 NOT FOUND\n");
}

#define HTTP403 "HTTP/1.1 403 Forbidden\r\nContent-Type: text/html\r\n\r\n<P>Password authenticate failed\r\nYour access is not allowed.\r\n"
void Http::Forbidden(int client_fd)
{
    send(client_fd,HTTP403,sizeof(HTTP403)-1,0);
    printf("403 FORBIDDEN\n");
}

#define HTTP400 "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: text/html\r\n\r\n<P>Your browser sent a bad request, such as a POST without a Content-Length.\r\n"
void Http::BadRequest(int client_fd) 
{
	send(client_fd, HTTP400, sizeof(HTTP400)-1, 0);
	printf("400 BAD REQUEST.\n");
}

#define HEARTBEAT "GET HeartBeat Send\r\nContent-Type: text/plain\r\nContent-Length: 16\r\n\r\nAre you alive???\r\n"
int Http::HeartBeat(int client_fd) 
{
    int ret_len = send(client_fd, HEARTBEAT, sizeof(HEARTBEAT)-1, 0);
	printf("Heart Beat.\n");
    return ret_len;
}


#define HTTP200 "HTTP/1.1 200 OK\r\n"
#define CONTENT_TYPE "Content-Type: %s\r\n"
#define CONTENT_LEN "Content-Length: %d\r\n"
#define KEEP_ALIVE "Connection: keep-alive\r\n"
#define IsAllowedExtion(type) (strcmp(path+extpos,type))
#define AddHeader(header)\
    strcpy(send_header+offset,header);\
    offset += sizeof(header) - 1;
#define AddHeaderPara(header,value)\
    sprintf(send_header+offset, header,value);\
    offset += strlen(send_header+offset);

int Http::SendHeader(int client_fd,const char*path,uint64_t file_size)
{
    char send_header[MAX_HEADER_LEN];
    uint32_t offset = 0;
    uint32_t extpos = strlen(path)-4;
    AddHeader(HTTP200 SERVER_STRING);
    AddHeader(KEEP_ALIVE);
    //AddHeaderPara(CONTENT_TYPE,IsAllowedExtion("html")?(IsAllowedExtion(".css")?(IsAllowedExtion(".ico")?"text/pain":"image/x-icon"):"text/css"):"text/html");
    
    // AddHeaderPara(CONTENT_LEN "\r\n", file_size)
    printf("200 OK\n");
    return send(client_fd,send_header,offset,0);
}
