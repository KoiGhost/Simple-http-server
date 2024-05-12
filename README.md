# Simple-http-server
## A simple http server

用来提高coding能力的小项目

使用了任务队列、线程池。后续可以考虑添加IO复用技术和sock非阻塞实现高性能服务器。

http是维持的长连接，对POST、GET包，以及404、403、400、501、200响应码都做了简单处理，通过心跳包判断客户端是否关闭。

POST包会固定读取body的字符串当作密码，判断通过则发送文件。

GET包固定传输一个文件，对query string进行了读取，但是未实现功能。

没有实现读取命令行参数的功能。

## Reference：

[fumiama/simple-http-server](https://github.com/fumiama/simple-http-server)
