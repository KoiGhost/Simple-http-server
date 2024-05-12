#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#include "Server.h"

#define dprintf printf

#define SplitTextBySpace(buf,idx,end_idx) while(!isspace((int)buf[idx]) && (idx<end_idx)) idx++;
#define SkipSpace(buf,idx,end_idx) while(isspace((int)buf[idx]) && (idx<end_idx)) idx++;
#define SkipStrTerminate(buf,idx,end_idx) while((buf[idx] =='\0') && (idx<end_idx)) idx++;
#define GetMethod(method_type) method_type == GET? "GET":"POST"

Server::Server(int port,int min_threads,int max_threads,const char* password)
:   port_(port),
    password_(password),
    listenFd_(socket_bind_listen(port))
{
    handle_for_sigpipe();
    thread_pool.ThreadPool_Create(min_threads,max_threads);

    // 该函数需要void(*)(void*)类型函数，即接受一个void*参数，返回void*类型的值
    // 将int类型转为long long是因为64位系统指针为8字节，int直接转会导致警告或错误
    pthread_cleanup_push((void (*)(void*))&close, (void*)((long long)listenFd_));
    WaitNewConn();
    pthread_cleanup_pop(1);
}

Server::~Server()
{
    //shutdown(listenFd_,SHUT_WR);
    close(listenFd_);
}


void Server::handNewConn(std::shared_ptr<void> args)
{
    std::shared_ptr<int> client_fd_ptr = std::static_pointer_cast<int>(args);
    if(client_fd_ptr.get()==nullptr){
        dprintf("> Connection fd is Null\n");
        return;
    }
    int client_fd = *client_fd_ptr.get();
    char* recv_line_buf = (char*) calloc(MAX_LINE_LEN,sizeof(char));
    char* path = (char*) calloc(MAX_LINE_LEN,sizeof(char));
    char* query_string = (char*) calloc(MAX_LINE_LEN,sizeof(char));
    int line_len = 0;
    int split_idx = 0;
    method_type_enum method_type;
    struct stat file_stat,sock_stat;
    Http http;
    while (1)
    {
        line_len = recv_one_line(client_fd, recv_line_buf, MAX_LINE_LEN);
        if(line_len<0 || (fstat(client_fd,&sock_stat)==-1)){
            close(client_fd);
            return;
        }
        split_idx = 0;
        bool is_query = false;
        // 跳过http Method
        SplitTextBySpace(recv_line_buf,split_idx,line_len-1);
        recv_line_buf[split_idx] = '\0';
        
        if(!strcmp(recv_line_buf,"GET")){
            method_type = GET;
        }
        else if(!strcmp(recv_line_buf,"POST")){
            method_type = POST;
        }
        else{
            if(line_len==0){
                continue;
            }
            http.UnImplementedMethod(client_fd);
            DiscardRemainData(client_fd);
            continue;
        }
        
        SkipStrTerminate(recv_line_buf,split_idx,line_len-1);
        path = recv_line_buf + split_idx ;
        // 跳过 path
        SplitTextBySpace(recv_line_buf,split_idx,line_len-1);
        recv_line_buf[split_idx] = '\0';

        if(method_type == GET){
            query_string = path;
            while((*query_string!='?' && *query_string!='\0')) query_string++;
            if(*query_string=='?'){
                *query_string = '\0';
                query_string++;
                is_query = true;
            }
            else{
                is_query = false;
            }
        }

        // 跳过../，避免路径穿越
        // 例如../a.txt -> ./a.txt
        // /security/a.txt -> ./security/a.txt
        // 将访问的文件路径锁定在当前文件夹中
        while((*path=='.' || *path=='/' || *path=='#')) path++;
        path -=2;
        path[0] = '.';
        path[1] = '/';

        dprintf("[%s] path='%s' \n",GetMethod(method_type),path);

        // 如果是GET /search?q=example
        // 此时path为/search，query_string为q=example
        int path_len = strlen(recv_line_buf) + 1;
        // +11 是因为path后面要添加/index.html
        char path_tmp[path_len + 11]; 
        memcpy(path_tmp,path,path_len);
        path = path_tmp;

        if(is_query)
        {
            int query_len = strlen(query_string) + 1;
            char query_string_tmp[query_len];
            memcpy(query_string_tmp,query_string,query_len);
            query_string = query_string_tmp;
        }

        int content_length = 0;
        // 读取完报文头
        while(line_len>0){
            line_len = recv_one_line(client_fd,recv_line_buf,MAX_LINE_LEN);
            // 只有header 和 body之间的行才会有第一个字符是'\r'
            // POST /path HTTP/1.1\r\n
            // Host: 192.1.1.1\r\n
            // Content-Length: 1\r\n
            // \r\n
            // body
            if(recv_line_buf[0]=='\r'||recv_line_buf[0]=='\n'){
                break;
            }
            if(content_length==0 && !strncasecmp(recv_line_buf,"Content-Length: ",16)){
                content_length = atoi(recv_line_buf+16);
            }
        }

        if(stat(path,&file_stat)==-1){
            http.NotFound(client_fd);
            DiscardRemainData(client_fd);
            continue;
        }
        
        // 如果是POST则读取body，并判断body的密码是否正确
        if(method_type == POST){
            if(content_length == strlen(password_))
            {
                bool check_password = false;
                char recv_password[content_length];
                line_len = recv_one_line(client_fd,recv_line_buf,content_length);
                if(line_len>0){
                    memcpy(recv_password,recv_line_buf,content_length);
                    recv_line_buf[content_length] = '\0';
                    if(!strncasecmp(password_,recv_password,(sizeof(password_)<sizeof(recv_password))?sizeof(password_):sizeof(recv_password))){
                        check_password = true;
                    }
                }
                if(!check_password){
                    http.Forbidden(client_fd);
                    continue;
                }
            }
            else{
                http.Forbidden(client_fd);
                DiscardRemainData(client_fd);
                continue;
            }
        }
        
        if((file_stat.st_mode & S_IFMT)==S_IFDIR){
            if(method_type == GET)
            {
                strcat(path,"./index.html");
            }
            else if(method_type == POST){
                strcat(path,"./data.html");
            }
            if(stat(path,&file_stat)==-1){
                http.NotFound(client_fd);
                continue;
            }
        }
        
        if(method_type == POST && content_length == -1) http.BadRequest(client_fd);
        else if(is_query) http.UnImplementedQuery(client_fd);
        else{
            send_file(client_fd,path);
        }
        // memset(recv_line_buf,0,MAX_LINE_LEN);
        // memset(path,0,MAX_LINE_LEN);
    }
    free(path);
    free(recv_line_buf);
    
}

void Server::WaitNewConn()
{
    while(1)
    {
        dprintf("Ready for accept new connection\n");
        struct sockaddr_in client_sock;
        memset((void*)&client_sock,0,sizeof(sockaddr_in));
        int client_fd;
        socklen_t socklen = sizeof(struct sockaddr_in);
        if((client_fd = accept(listenFd_, (struct sockaddr*)&client_sock, &socklen))<=0)
        {
            perror("accept");
            continue;
        }
        uint16_t port = ntohs(client_sock.sin_port);
        struct in_addr c_addr = client_sock.sin_addr;
        char str_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&c_addr,str_addr,sizeof(str_addr));
        dprintf("\n> Accept client %s:%u\n ", str_addr, port);

        std::shared_ptr<void> client_fd_ptr = std::make_shared<int>(client_fd);
        thread_pool.ThreadPool_Add([this,client_fd_ptr](std::shared_ptr<void> args){
            this->handNewConn(args);
        },client_fd_ptr);
        thread_pool.ThreadPool_Add([this,client_fd_ptr](std::shared_ptr<void> args){
            this->heartbeat(args);
        },client_fd_ptr);

        dprintf("> Add client %s:%u connection to Task queue\n",str_addr, port);
    }
}

void Server::DiscardRemainData(int client_fd) {
    char* tmp_buf = (char*) calloc(MAX_LINE_LEN,sizeof(char));
    int line_len = 1;
    while (line_len>0 && tmp_buf[0]!='\n' && tmp_buf[0]!='\r') 
        line_len = recv_one_line(client_fd, tmp_buf, sizeof(tmp_buf));
}
    

void Server::heartbeat(std::shared_ptr<void> args)
{
    std::shared_ptr<int> client_fd_ptr = std::static_pointer_cast<int>(args);
    if(client_fd_ptr.get()==nullptr){
        dprintf("> Connection fd is Null\n");
        return;
    }
    int client_fd = *client_fd_ptr.get();
    int ret_len = 0;
    while (true)
    {
        sleep(HEARTBEAT_INTERVAL);
        ret_len = http.HeartBeat(client_fd);
        if(ret_len <=0){
            close(client_fd);
            dprintf("Heart beat send failed, close socket\n");
            return;
        }
        ret_len = 0;
        
    }
}

int Server::socket_bind_listen(int port)
{
    if(port<0||port>65535)
        return -1;
    int listen_fd = 0;
    // AF_INET 为Ipv4
    // SOCK_STREAM 为TCP
    if((listen_fd = socket(AF_INET,SOCK_STREAM,0))==-1)
        return -1;
    
    int optval = 1;
    //SO_REUSERADDR 设置Ip地址和端口复用
    // SOL_SOCKET 设置套接字层的复用
    if(setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval))==-1)
        return -1;

    struct sockaddr_in server_addr;
    memset((void*)&server_addr,0,sizeof(sockaddr_in));
    server_addr.sin_family = AF_INET;
    // 指定ip地址为0.0.0.0 表示接受所有地址
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);
    if(bind(listen_fd,(struct sockaddr*)&server_addr,sizeof(sockaddr_in))==-1)
        return -1;
    if(listen(listen_fd,2048)==-1)
        return -1;
    if(listen_fd == -1)
    {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

// 当客户端关闭一个链接，进程又尝试向已关闭的TCP连接发送数据时，服务器端会收到系统的一个SIGPIPE信号
// 而接受到SIGPIPE信号后的进程会异常终止，导致服务器终止
// 避免进程被终止，则有程序进行更优雅的异常处理。
void Server::handle_for_sigpipe()
{
    struct sigaction sa;
    // '\0' 是0的ascii字符
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    // sa设置了SIG_IGN会忽视SIGPIPE信号
    if(sigaction(SIGPIPE,&sa,NULL))
        return;
}

void Server::send_file(int client_fd,const char* file_name)
{
    int fd = open(file_name,O_RDONLY);
    if(fd<0){
        perror("open");
        return http.NotFound(client_fd);
    }
    struct stat file_stat;
    if(fstat(fd,&file_stat)){
        perror("fstat");
        return http.NotFound(client_fd);
    }
    if(http.SendHeader(client_fd,file_name,(uint64_t)file_stat.st_size)<=0){
        perror("send");
        return http.NotFound(client_fd);
    }
    off_t offt=0;
    ssize_t ret = sendfile(client_fd,fd,&offt,(size_t)file_stat.st_size);
    close(fd);
}


int Server::recv_one_line(int listen_fd,char* buf, int size)
{
    int is_recv = 0;
    char recv_char = '\0';
    int line_end = 0;
    for(line_end = 0;line_end < size && recv_char!='\n';)
    {
        is_recv = recv(listen_fd,&recv_char,1,0);
        if(is_recv>0)
        {
            // 处理行以'\r\n'结束的情况
            // 不管是'\r'还是'\r\n'都认为是一行结束，都用'\n'替换
            if(recv_char == '\r')
            {
                is_recv = recv(listen_fd,&recv_char,1,MSG_PEEK);
                if(is_recv>0 && recv_char=='\n')
                {
                    recv(listen_fd,&recv_char,1,0);
                }
            }
            buf[line_end++] = recv_char;
        }
        // 如果接收数据失败则认为结束
        else{
            recv_char = '\n';
        }
    }
    // buf结构应该为 data\n\0
    buf[line_end] = '\0';
    return line_end;
}