/*************************************************************************
    > File Name: ukey_httpd.c
    > Author: Ukey
    > Mail: gsl110809@gmail.com
    > Created Time: 2017年06月07日 星期三 19时04分58秒
 ************************************************************************/

#include "ukey_httpd.h"
#include "thread_pool.h"

struct transfer_fd
{
	int sock_fd;
	int epoll_fd;
};

static void epoll_add_fd(int epoll_fd, int fd, int et_flag)
{
	struct epoll_event event;
	event.data.fd = fd;
	//如果et_flag为true,则设置该事件的触发方式为边缘触发并设置该文件描述符为非阻塞
	if(et_flag)
	{
		event.events = EPOLLIN | EPOLLET;
		//设置了边缘触发,一定要记得将该文件描述符设置为非阻塞
		int flag = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flag | O_NONBLOCK);
	}
	else
	{
		event.events = EPOLLIN;
	}
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
}

static void *accept_connect(void *arg)
{
	struct transfer_fd *fd = (struct transfer_fd *)arg;
	struct sockaddr_in client_addr;
	socklen_t client_len;
	client_len = sizeof(client_addr);
	int new_fd = accept(fd->sock_fd, (struct sockaddr* )&client_addr, &client_len);
				
	epoll_add_fd(fd->epoll_fd, new_fd, 1);

	return NULL;

}

//按行获取浏览器的request
static int get_line(int sock_fd, char *buf, int size)
{
	int i = 0, n;
	char c = '\0';
	while((i < size) && (c != '\n'))
	{
		n = recv(sock_fd, &c, 1, 0);
		if(n > 0)
		{
			//这里主要的目的是读取请求数据时将http请求的回车换行符在我们的buf中换成换行
			if(c == '\r')
			{
				//MSG_PEEK的作用是读取数据后,但是并不将读取的数据清出缓冲区,也就是说下次读的时候还在
				n = recv(sock_fd, &c, 1, MSG_PEEK);
				if((n > 0) && (c == '\n'))
				{
					recv(sock_fd, &c, 1, 0);
				}
				else
				{
					c = '\n';
				}
			}
			buf[i] = c;
			i++;
		}
		else
			break;
	}
	buf[i] = '\0';
	return i;
}
static void send_headers(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 200 OK\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
}

static void not_found(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>404 not found</TITLE>\r\n<BODY><P>server can't find the source which you request\r\n</P></BODY></HTML>\r\n");
	send(client_fd, buf, strlen(buf), 0);
}

static void permission_denied(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 403 PERMISSION DENIED\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>403 permission denied</TITLE>\r\n<BODY><P>you don't have enough permissions to view this file\r\n</P></BODY></HTML>\r\n");
	send(client_fd, buf, strlen(buf), 0);
}
	

static void serve_file(int client_fd, const char *file_name)
{
	int n = 1, fd;
	char buf[BUFF_SIZE] = {0};
	struct stat s;
	while((n > 0) && strcmp(buf, "\n"))
	{
		n = get_line(client_fd, buf, sizeof(buf) - 1);
	}
	//判断文件是否存在
	if((access(file_name, F_OK)) == -1)
	{
		not_found(client_fd);
	}
	else
	{
		fd = open(file_name, O_RDONLY);
		//权限不足
		if(errno == EACCES)
		{
			permission_denied(client_fd);
		}
		//否则回送静态文本数据
		else
		{
			//使用mmap将文件映射到内存中
			fstat(fd, &s);
			int file_size = s.st_size;
			char *p = (char *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
			if(p == MAP_FAILED)
			{
				perror("mmap error");
				exit(1);
			}
			send_headers(client_fd);
			send(client_fd, p, file_size, 0);
			munmap(p, file_size);
		}
	}
	close(fd);
}

static void method_not_implemented(int client)
{
	char buf[BUFF_SIZE];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\nServer: ukey_httpd/1.0\r\nContent-Type:text/html\r\n\r\n");
	send(client, buf, strlen(buf), 0);

	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE></HEAD>\r\n<BODY><P>request method not support\r\n</P></BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}


static void bad_request(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\nServer: ukey_httpd/1.0\r\nContent-type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>400 BAD REQUEST</TITLE></HEAD><BODY><P>this is a bad request for server</P></BODY></HTML>");
	send(client_fd, buf, strlen(buf), 0);
}

static void cannot_execute(int client_fd)
{
	char buf[BUFF_SIZE];
	sprintf(buf, "HTTP/1.0 500 Server error\r\nServer: ukey_httpd/1.0\r\nContent-type:text/html\r\n\r\n");
	send(client_fd, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>500 SERVER ERROR</TITLE></HEAD><BODY><P>SERVER ERROR</P></BODY></HTML>");
	send(client_fd, buf, strlen(buf), 0);
}

static void execute_cgi(int client_fd, const char *path, const char *method, const char *query_string)
{

	char buf[BUFF_SIZE], c;
	int cgi_output[2];
	int cgi_input[2];
	int i, status;
	int n = 1, content_length = -1;
	pid_t pid;

	//GET请求
	if(!strcasecmp(method, "GET"))
	{
		while((n > 0) && strcmp(buf, "\n"))
		{
			n = get_line(client_fd, buf, sizeof(buf) - 1);
		}
	}
	//POST请求
	else
	{
		n = get_line(client_fd, buf, sizeof(buf) - 1);
		while((n > 0) && (strcmp(buf, "\n")))
		{
			buf[15] = '\0';
			if(!strcasecmp(buf, "Content-Length:"))
			{
				content_length = atoi(&(buf[16]));
			}
			n = get_line(client_fd, buf, sizeof(buf) - 1);
		}
		if(content_length == -1)
		{
			bad_request(client_fd);
			return ;
		}
	}

	if(pipe(cgi_output) < 0)
	{
		cannot_execute(client_fd);
		return ;
	}
	if(pipe(cgi_input) < 0)
	{
		cannot_execute(client_fd);
		return ;
	}
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client_fd, buf, strlen(buf), 0);

	if((pid = fork()) < 0)
	{
		cannot_execute(client_fd);
		return ;
	}
	if(pid == 0)
	{
		char meth_env[ARG_SIZE];
		char query_env[ARG_SIZE];
		char length_env[ARG_SIZE];

		close(cgi_output[0]);
		close(cgi_input[1]);
		if(dup2(cgi_output[1], STDOUT_FILENO) == -1)
		{
			perror("dup2 out error");
			exit(1);
		}
		if(dup2(cgi_input[0], STDIN_FILENO) == -1)
		{
			perror("dup2 in error");
			exit(1);
		}
		sprintf(meth_env, "REQUEST_METHOD=%s", method);
		//设置环境变量
		putenv(meth_env);
		if(!strcasecmp(method, "GET"))
		{
			sprintf(query_env, "QUERY_STRING=%s", query_string);
			putenv(query_env);
		}
		else
		{
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
			putenv(length_env);
		}
		execl(path, path, (char *)0);
		exit(0);
	}
	else
	{
		close(cgi_output[1]);
		close(cgi_input[0]);
		if(!strcasecmp(method, "POST"))
		{
			for(i = 0; i < content_length; i++)
			{
				recv(client_fd, &c, 1, 0);
				write(cgi_input[1], &c, 1);
			}
		}
		while(read(cgi_output[0], &c, 1) > 0)
		{
			send(client_fd, &c, 1, 0);
		}
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);
	}
}


static void *work_request(void *arg)
{
	int *work_fd = (int *)arg;
	char buf[BUFF_SIZE];
	char method[ARG_SIZE];
	char url[ARG_SIZE];
	char path[512];
	char *query_string = NULL;
	size_t n, i = 0, j = 0;
	int cgi_flag = 0;
	struct stat s;

	n = get_line(*work_fd, buf, sizeof(buf) - 1);
	
	while((i < sizeof(method) - 1) && (!isspace(buf[i])))
	{
		method[i] = buf[i];
		i++;
	}
	method[i] = '\0';
	j = i;

	//strcastcmp用于不区分大小写的字符串比较
	if(strcasecmp(method, "GET") && strcasecmp(method, "POST"))
	{
		method_not_implemented(*work_fd);
		return  NULL;
	}

	//跳过空格字符
	while(isspace(buf[j]) && (j < n))
	{
		j++;
	}
	i = 0;
	//获取url
	while(!isspace(buf[j]) && (i < sizeof(url) - 1) && (j < n))
	{
		url[i] = buf[j];
		i++;
		j++;
	}
	url[i] = '\0';


	if(!strcasecmp(method, "POST"))
	{
		cgi_flag = 1;
	}
	else
	{
		query_string = url;
		while((*query_string != '?') && (*query_string != '\0'))
		{
			query_string++;
		}
		if(*query_string == '?')
		{
			cgi_flag = 1;
			*query_string = '\0';
			query_string++;
		}
	}
	sprintf(path, "htdocs%s", url);
	if(path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}
	if(stat(path, &s) == -1)
	{
		while((n > 0) && strcmp(buf, "\n"))
		{
			n = get_line(*work_fd, buf, sizeof(buf) - 1);
			not_found(*work_fd);
		}
	}
	else
	{
		if(S_ISDIR(s.st_mode))
		{
			strcat(path, "/index.html");
		}
		if((s.st_mode & S_IXUSR) || (s.st_mode & S_IXGRP) || (s.st_mode & S_IXOTH))
		{
			cgi_flag = 1;
		}
		if(!cgi_flag)
		{
			serve_file(*work_fd, path);
		}
		else
		{
			execute_cgi(*work_fd, path, method, query_string);
		}
	}
	close(*work_fd);
	return NULL;
}


int main()
{
	struct sockaddr_in server_addr;
	int listen_fd, epoll_fd;
	if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket error");
		exit(1);
	}
	bzero(&server_addr, sizeof(server_addr));

	//设置端口复用
	int flag = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(SERV_PORT);

	bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	
	listen(listen_fd, 20);

	epoll_fd = epoll_create(EPOLL_SIZE);
	if(epoll_fd < 0)
	{
		perror("create epoll error");
		exit(1);
	}
	struct epoll_event events[EPOLL_SIZE];
	epoll_add_fd(epoll_fd, listen_fd, 1);

	threadpool_t *thread_pool = threadpool_create(50, 200, 200);

	while(1)
	{
		int event_count = epoll_wait(epoll_fd, events, EPOLL_SIZE, -1);
		if(event_count < 0)
		{
			perror("epoll wait error");
			exit(1);
		}
		int i = 0;
		for(; i < event_count; i++)
		{
			int sock_fd = events[i].data.fd;
			//如果是监听连接的套结字
			if(sock_fd == listen_fd)
			{
				struct transfer_fd fd;
				fd.sock_fd = sock_fd;
				fd.epoll_fd = epoll_fd;
				threadpool_add(thread_pool, accept_connect, (void *)&fd);
			}
			//否则就是传输数据的套结字
			else
			{
				threadpool_add(thread_pool, work_request, (void *)&sock_fd);

			}
		}
	}	
	close(listen_fd);
	close(epoll_fd);
	threadpool_destroy(thread_pool);
	return 0;
}
