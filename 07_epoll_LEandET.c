#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/epoll.h>
#include <stdbool.h>

#define MAX_EVENT 1024

int setnonblock(int* fd){//将fd设置为非阻塞触发模式
	int old_option = fcntl(*fd, F_GETFL);
	int new_option = old_option |= O_NONBLOCK;
	fcntl(*fd, F_SETFL, new_option);
	return old_option;
}

void addfd(int fd, int* epfd, int ctl_et){//将文件描述符fd，加入到epfd树中，ctl_et为1时将fd设置为边缘触发模式
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;
	if(ctl_et){
		event.events |= EPOLLET;
	}
	epoll_ctl(*epfd, EPOLL_CTL_ADD, fd,  &event);
	setnonblock(&fd);
}

/* 水平触发模式
*  ev：对应时间的event
*  num：接收到有事件的文件描述符数 
*  lfd：监听的文件描述
*  epfd：epoll树的根结点*/	
void lt(struct epoll_event *ev, int num, int lfd, int epfd){
	char buf[1024];
	for(int i = 0; i < num; i++){
		int fd = ev[i].data.fd;
		//printf("i = %d, event = %d\n", i, ev[i].events);
		if(fd == lfd){
			struct sockaddr_in client;
			socklen_t cli_len = sizeof(client);
			int cfd = accept(lfd, (struct sockaddr*)&client, &cli_len);
			assert(cfd);
			printf("连接成功!\n");
			addfd(cfd, &epfd, 0);
		}
		else if (ev[i].events & EPOLLIN){
			//printf("i = %d\n", i);
			memset(buf, '\0', sizeof(buf));
			int len = recv(fd, buf, sizeof(buf), 0);
			if(len == -1){
				perror("recv error!\n");
				exit(1);
			}
			else if(len == 0){
				printf("客户端断开连接！\n");
				close(fd);
				epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
			}
			else {
				printf("recv:%s\n", buf);
				send(fd, buf, len, 0);
			}
		}
	}
}

/* 边缘触发模式
*  ev：对应时间的event
*  num：接收到有事件的文件描述符数 
*  lfd：监听的文件描述
*  epfd：epoll树的根结点*/	
void et(struct epoll_event *ev, int num, int lfd, int epfd){
	char buf[1024];
	for(int i = 0; i < num; i++){
		int fd = ev[i].data.fd;
		printf("i = %d, event = %d\n", i, ev[i].events);
		if(fd == lfd){
			struct sockaddr_in client;
			socklen_t cli_len = sizeof(client);
			int cfd = accept(lfd, (struct sockaddr*)&client, &cli_len);
			printf("连接成功!\n");
			assert(cfd != -1);
			addfd(cfd, &epfd, 1);
		}
		else if(ev[i].events & EPOLLIN){
			printf("you will see this just once! i = %d\n", i);
			memset(buf, '\0', sizeof(buf));
			int len;
			while((len = recv(fd, buf, sizeof(buf), 0)) > 0){
				printf("recv :%s\n", buf);
				send(fd, buf, len, 0);
			}
			if(len < 0){
				if(errno == EAGAIN){
					printf("数据以读完！\n");
				}
				else{
					perror("recv error!\n");
					printf("errno = %d\n", errno);
					exit(1);
				}
			}
			else if(len == 0){
				printf("客户端断开连接！\n");
				close(fd);
				epoll_ctl(epfd , EPOLL_CTL_DEL, fd, NULL);
			}	
		}
	}
}

int main(int argc, char * argv[])
{
	if(argc < 2){
		printf("no port!\n");
		exit(1);
	}
	int port = atoi(argv[1]);
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(lfd > 0);
	//初始化服务器
	struct sockaddr_in sever;
	sever.sin_family = AF_INET;
	sever.sin_port = htons(port);
	sever.sin_addr.s_addr = INADDR_ANY;
	//绑定
	int ret = bind(lfd, (struct sockaddr*)&sever, sizeof(sever));
	if(ret == -1){
		perror("bind error!\n");
		exit(1);
	}
	//设置监听最大个数
	ret = listen(lfd, MAX_EVENT);
	if(ret == -1){
		perror("listen error!\n");
		exit(1);
	}
	//初始化epoll树
	struct epoll_event ev[1024];
	int efd = epoll_create(MAX_EVENT);
	assert(efd != -1);
	//将其加入epoll树中
	addfd(lfd, &efd, 0);
	//epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &ev);
	while(1){
		ret = epoll_wait(efd, ev, sizeof(ev) / sizeof(ev[0]), -1);
		if(ret < 0){
			printf("epoll error!\n");
			exit(1);
		}
		lt(ev, ret, lfd, efd);
		//printf("(main)ret = %d\n", ret);
		//et(ev, ret, lfd, efd);
	}
	return 0;
}

