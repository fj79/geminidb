/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef UTIL_FDE_H
#define UTIL_FDE_H

#include "../include.h"

#ifdef __linux__
	#define HAVE_EPOLL 1
#endif

#define FDEVENT_NONE	(0)
#define FDEVENT_IN		(1<<0)
#define FDEVENT_PRI		(1<<1)
#define FDEVENT_OUT		(1<<2)
#define FDEVENT_HUP		(1<<3)
#define FDEVENT_ERR		(1<<4)

//封装epoll_event
/*
struct epoll_event {
	__uint32_t events;
	typedef union epoll_data {
		void *ptr;
		int fd;
		__uint32_t u32;
		__uint64_t u64;
	} epoll_data_t;
};
*/
struct Fdevent{
	int fd;
	int s_flags; // subscribed events
	int events;	 // ready events
	struct{
		int num;
		void *ptr;
	}data;
};

#include <vector>
#ifdef HAVE_EPOLL
	#include <sys/epoll.h>
#else
	#include <sys/select.h>
#endif


//list of Fevent
class Fdevents{
	public:
		typedef std::vector<struct Fdevent *> events_t;
	private:
#ifdef HAVE_EPOLL
		static const int MAX_FDS = 8 * 1024;
		int ep_fd;
		struct epoll_event ep_events[MAX_FDS];
#else
		int maxfd;
		fd_set readset;
		fd_set writeset;
#endif
		events_t events;
		events_t ready_events;

		struct Fdevent *get_fde(int fd);
	public:
		Fdevents();
		~Fdevents();

		bool isset(int fd, int flag);

		//注册监听fd
		int set(int fd, int flags, int data_num, void *data_ptr);
		//删除对fd的监听
		int del(int fd);
		int clr(int fd, int flags);
		//返回准备就绪的 fdevent列表
		const events_t* wait(int timeout_ms=-1);
};

#endif

//epoll events
/*
EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
EPOLLOUT：表示对应的文件描述符可以写；
EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
EPOLLERR：表示对应的文件描述符发生错误；
EPOLLHUP：表示对应的文件描述符被挂断；
EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
 */