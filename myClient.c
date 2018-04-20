/*
 * 客户端程序，绑定套接字，然后调用connect函数向服务器发起连接
 */
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
int main(int argc, char *argv[]) {
	int sockfd;	//套接字文件描述符
	int len;
	struct sockaddr_in address;	//结构体，存放网络通信的地址
	int result;	//返回connect函数连接状态
	char ch = 'A';

	sockfd = socket(AF_INET, SOCK_STREAM, 0);	//建立socket套接字文件描述符
	//为网络通信的地址赋值
	address.sin_family = AF_INET;		//地址家族的推荐选项，表示是IPv4协议
    address.sin_addr.s_addr = inet_addr("127.0.0.1");	//IPv4地址
    address.sin_port = htons(9734);		//端口号

    len = sizeof(address);	//地址缓冲区长度
    result = connect(sockfd, (struct sockaddr *)&address, len);	//客户端调用connect函数与服务器建立起连接，成功返回0，失败返回-1，错误原因存于errno中

    if (result == -1)
    {
        perror("oops: client1");	//perror用来将上一个函数发生错误的原因输出到标准设备(stderr)
        exit(1);	//异常退出程序时值选择1
    }
    //调用socket通信函数write、read
    write(sockfd, &ch, 1);
    read(sockfd, &ch, 1);
    printf("char from server = %c\n", ch);
    close(sockfd);	//主动关闭套接字
    exit(0);	//正常退出程序

}
