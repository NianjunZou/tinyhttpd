/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
/*
 * tinyhttpd的学习,
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
//#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

//函数说明：检查参数c是否为空格字符，
//也就是判断是否为空格(' ')、定位字符(' \t ')、回车(' \r ')、换行(' \n ')、垂直定位字符(' \v ')或翻页(' \f ')的情况。
//返回值：若参数c 为空白字符，则返回非 0，否则返回 0。
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"	//定义服务器名称
#define STDIN   0
#define STDOUT  1
#define STDERR  2

//函数声明
void accept_request(void *);//处理从套接字上监听到的一个 HTTP 请求
void bad_request(int);//返回给客户端这是个错误请求，400响应码
void cat(int, FILE *);//读取服务器上某个文件写到 socket 套接字
void cannot_execute(int);//处理发生在执行 cgi 程序时出现的错误
void error_die(const char *);//把错误信息写到 perror
void execute_cgi(int, const char *, const char *, const char *);//运行cgi脚本，这个非常重要，涉及动态解析
int get_line(int, char *, int);//读取一行HTTP报文
void headers(int, const char *);//返回HTTP响应头
void not_found(int);//返回找不到请求文件
void serve_file(int, const char *);//调用 cat 把服务器文件内容返回给浏览器。
int startup(u_short *);//开启http服务，包括绑定端口，监听，开启子进程处理连接
void unimplemented(int);//返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。


/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/*
 * 向客户端发送请求的文件内容
 */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}


/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/*
 * 每次从socket字节流获得一行放入buf，行末的标识可以是新行、回车或者回车换行，每次遇到空字符的时候
 * 结束一次读取。只有在最后一行的时候，最后遇到标识符，以空字符结束读取，否则字符串被置为null
 */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int i = 0;		//缓冲区游标
    char c = '\0';	//设置单字节缓冲区
    int n;

    //把终止条件统一为 \n 换行符，以标准化 buf 数组
    while ((i < size - 1) && (c != '\n'))
    {
    	//recv函数，一次只接收一个字节赋值给c，标志位通常为0，正确返回为接收的字节数
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        //如果接收成功
        if (n > 0)
        {
        	//收到 \r 则继续接收下个字节，因为换行符可能是 \r\n
            if (c == '\r')
            {
            	//使用 MSG_PEEK查看当前数据，数据被送到缓冲区，但是并不从输入队列删除，
            	//因此下一次读取依然可以得到这次读取的内容，可认为接收窗口不滑动
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                //如果是换行符则把\r跳过(\r\n -> \n)，否则将\r覆盖(\r -> \n)，目标都是变为\n
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    //使用\0作为结束符，i=size-1
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/* 承接startup函数，下一个建议阅读的函数是execute_cgi；
 * 本函数处理已连接套接字的http请求
/**********************************************************************/
void accept_request(void *arg)
{
	//将指针转换为intptr_t类型，这样可以跨平台正确存放地址
    //将客户端传入的指针强制转换为套接字所能认识的文件描述符
    int client = (intptr_t)arg;
    //套接字缓冲区，http方法，url表示，路径
	//读取起始行，格式例子：GET /form.html HTTP/1.1 (CRLF)
    char buf[1024];     //通用字符缓冲区
    size_t numchars;    //字符计数
    char method[255];   //存储http方法
    char url[255];      //存储url
    char path[512];
    size_t i, j;
    struct stat st;     //套接字状态
    int cgi = 0;      /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;  //匹配查询内容的字符指针
    //调用get_line将此行变成只有\n换行符的字符数组，返回为字符数
    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    //把buffer中的method解析出来，隔离条件是出现空格
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';
    //strcasecmp用于忽略大小写比较两个字符串，当然\0是会被忽略的。
    //如果method不是get或者post，则调用unimplemented函数向客户端报告无法提供功能
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return;
    }
    //如果是post方法则需要执行cgi程序
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    //把buffer中的url解析出来，并处理
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';
    //如果是get方法，首先需要找到url中的查询表达式
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        //当找到url中的查询语句，设置会调用cgi程序，
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
    //建立path的内容，其实是将两个字符串htdocs和url进行连接
    sprintf(path, "htdocs%s", url);
    //如果路径最后是/，则需要添加完整路径
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    //如果没找到这个文件
    if (stat(path, &st) == -1) {
    	//丢弃每一行
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        //返回404 not found
        not_found(client);
    }
    //如果找到了文件
    else
    {
    	//使用st_mode来判断文件类型，S_IFMT是文件类型的屏蔽字
    	//S_IFDIR是目录类型，S_IXUSR是文件所有者可执行文件，S_IXGRP用户组可执行文件
    	//S_IXOTH是其他用户可执行文件
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH)    )
            cgi = 1;
        //如果不需要执行cgi程序，则向客户端发送请求文件的内容（get方法，未使用查询语句）
        if (!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }
    //服务器主动关闭已连接套接字
    close(client);
}


/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/* 这是最后一个推荐阅读的关键函数，输入套接字描述符、请求文件路径、请求方法、查询指针
/**********************************************************************/
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];	//建立输出流向的管道，保存这个管道的进出口文件描述符
    int cgi_input[2];	//建立输入流向的管道，保存这个管道的进出口文件描述符
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    //如果是get方法，把所有的网页内容换行符全变成\n格式
    if (strcasecmp(method, "GET") == 0)
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    //如果是post方法，也要处理网页内容，获得content_length的值
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }
    else/*HEAD or other*/
    {
    }

    //初步处理网页内容之后开始建立管道
    //父进程创建管道，得到两个文件描述符指向管道的两端
    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }
    //父进程fork出子进程，由于继承父进程已经打开的文件描述符，⼦进程也有两个⽂件描述符指向同⼀管道。
    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    //子进程返回0
    if (pid == 0)  /* child: CGI script */
    {
    	//设置环境变量
        char meth_env[255];
        char query_env[255];
        char length_env[255];
        //建立起管道之间的关系
        dup2(cgi_output[1], STDOUT);	//子进程关闭STDOUT，并将cgi_output[1]的文件描述符拷贝给STDOUT
        dup2(cgi_input[0], STDIN);		//子进程关闭STDIN，并将cgi_input[0]的文件描述符拷贝给STDIN
        close(cgi_output[0]);			//子进程关闭管道的另一端
        close(cgi_input[1]);
        //添加环境变量
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, NULL);
        exit(0);
    }
    //父进程返回子进程的进程号
    else {    /* parent */
    	//父进程关闭管道的另一端
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        //收集子进程的状态
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/* main函数阅读完之后阅读此函数，下一个建议阅读的函数是accept_request;
 * 此函数的功能是启动一个进程在某个端口完成网络监听；
 * 端口分配默认使用传引用分配，也可以动态随机分配；
 * 返回的是监听套接字；
 * bind用于将套接字与守护进程绑定*/
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;
    int on = 1;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);	//建立一个套接字
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));	//初始化服务器端套接字地址空间，使用0占位
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);	//将端口号整型变量从主机字节顺序转变成网络字节顺序，端口为0是
    name.sin_addr.s_addr = htonl(INADDR_ANY);	//将IPv4地址转换成无符号长整型的网络字节序，INADDR_ANY表示任意地址
    //设置套接字选项，SOL_SOCKET表示等级为可设置选项，SO_REUSEADDR结合on表示打开地址复用功能
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
    {
        error_die("setsockopt failed");
    }
    //将IP地址与套接字绑定
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    //是否动态分配端口
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    //进入监听模式，5表示后备连接数为5个
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* 主函数，首先阅读的函数，下一个建议阅读的函数为startup。
 * 默认端口为4000，初始化服务器和客户端描述符为-1；
 * sockaddr_in数据结构代表了啥？？？
 * 调用startup函数来绑定端口，建立监听套接字，服务器启动；
 * 服务过程中，服务器被动连接，使用accept调用建立TCP连接；
 * 监听套接字派生子进程去执行已连接套接字；
 * 服务完成之后，被动关闭TCP连接。*/
/*********************************************************************/
int main(void)
{
    int server_sock = -1;	//服务器套接字
    u_short port = 4000;	//系统默认初始端口号
    int client_sock = -1;	//客户端套接字
    struct sockaddr_in client_name;	//存放网络通信的地址
    socklen_t  client_name_len = sizeof(client_name);	//地址类型的长度
    //pthread_t newthread;

    server_sock = startup(&port);	//绑定监听套接字，端口号可以由系统随机分配
    printf("httpd running on port %d\n", port);

    while (1)
    {
    	//被动监听
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        accept_request(&client_sock);
        // if (pthread_create(&newthread , NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0)
        //     perror("pthread_create");
    }
    //主动关闭套接字
    close(server_sock);

    return(0);
}
