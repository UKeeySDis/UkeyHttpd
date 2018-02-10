/*************************************************************************
    > File Name: ukey_httpd.h
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2017年06月07日 星期三 19时01分16秒
 ************************************************************************/
#ifndef __UKEY_HTTPD_H_
#define __UKEY_HTTPD_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <errno.h>
#include "connection.h"


#define SERV_PORT	4000
#define EPOLL_SIZE  5000
#define BUFF_SIZE	1024
#define ARG_SIZE	255


int event_ralloc();
int epev_ralloc();
//接受新的连接
void *accept_connect(void *arg);

//按行读取http请求
static int get_line(int sock_fd, char *buf, int size);

//向浏览器回送http响应
static void send_headers(int client_fd);

//向浏览器回送403响应码(服务器无权访问所请求的文件)
static void permission_denied(int client_fd);

//向浏览器回送404响应码(服务器不能找到所请求的文件)
static void not_found(int client_fd);

//请求静态内容
static void serve_file(int client_fd, const char *file_name);

//向浏览器回送501响应码(服务器不支持请求的方法)
static void method_not_implemented(int client);

//向浏览器回送400响应(服务器不能理解该请求)
static void bad_request(int client_fd);

//向浏览器回送500响应
static void cannot_execute(int client_fd);

//执行cgi程序
static void execute_cgi(int client_fd, const char *path, const char *method, const char *query_string);

//处理http请求
void *work_request(void *arg);

void add_conn(int, int, void *);

void process_conn(int, int, void *);

int manager_init();
void manager_destroy();


#endif


