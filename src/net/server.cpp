/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#include "server.h"
#include "../util/strings.h"
#include "../util/file.h"
#include "../util/config.h"
#include "../util/log.h"
#include "../util/ip_filter.h"
#include "link.h"
#include <vector>

static DEF_PROC(ping);
static DEF_PROC(info);

#define TICK_INTERVAL          100 // ms
#define STATUS_REPORT_TICKS    (300 * 1000/TICK_INTERVAL) // second
static const int READER_THREADS = 10;
static const int WRITER_THREADS = 1;  // 必须为1, 因为某些写操作依赖单线程

volatile bool quit = false;
volatile uint32_t g_ticks = 0;

void signal_handler(int sig){
	switch(sig){
		case SIGTERM:
		case SIGINT:{
			quit = true;
			break;
		}
		case SIGALRM:{
			g_ticks ++;
			break;
		}
	}
}

NetworkServer::NetworkServer(){
	num_readers = READER_THREADS;
	num_writers = WRITER_THREADS;
	
	tick_interval = TICK_INTERVAL;
	status_report_ticks = STATUS_REPORT_TICKS;

	//conf = NULL;
	serv_link = NULL;
	link_count = 0;

	fdes = new Fdevents();

	// add built-in procs, can be overridden
	proc_map.set_proc("ping", "r", proc_ping);
	proc_map.set_proc("info", "r", proc_info);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
#ifndef __CYGWIN__
	signal(SIGALRM, signal_handler);
	{
		struct itimerval tv;
		tv.it_interval.tv_sec = (TICK_INTERVAL / 1000);
		tv.it_interval.tv_usec = (TICK_INTERVAL % 1000) * 1000;
		tv.it_value.tv_sec = 1;
		tv.it_value.tv_usec = 0;
		setitimer(ITIMER_REAL, &tv, NULL);
	}
#endif
}
	
NetworkServer::~NetworkServer(){
	//delete conf;
	delete serv_link;
	delete fdes;

	writer->stop();
	delete writer;
	reader->stop();
	delete reader;
}

NetworkServer* NetworkServer::init(const char *conf_file, int num_readers, int num_writers){
	if(!is_file(conf_file)){
		fprintf(stderr, "'%s' is not a file or not exists!\n", conf_file);
		exit(1);
	}
	Config *conf = Config::load(conf_file);
	if(!conf){
		fprintf(stderr, "error loading conf file: '%s'\n", conf_file);
		exit(1);
	}
	{
		std::string conf_dir = real_dirname(conf_file);
		if(chdir(conf_dir.c_str()) == -1){
			fprintf(stderr, "error chdir: %s\n", conf_dir.c_str());
			exit(1);
		}
	}
	NetworkServer* serv = init(*conf, num_readers, num_writers);
	delete conf;
	return serv;
}

NetworkServer* NetworkServer::init(const Config &conf, int num_readers, int num_writers){
	NetworkServer *serv = new NetworkServer();
	if(num_readers >= 0)
		serv->num_readers = num_readers;
	if(num_writers >= 0)
		serv->num_writers = num_writers;
	// init serv_link
	{
		const char *ip = conf.get_str("server.ip");
		int port = conf.get_num("server.port");
		if(ip == NULL || ip[0] == '\0')
			ip = "127.0.0.1";
		serv->serv_link = Link::listen(ip, port);
		if(serv->serv_link == NULL){
			log_fatal("error opening server socket! %s", strerror(errno));
			fprintf(stderr, "error opening server socket! %s\n", strerror(errno));
			exit(1);
		}
		log_info("server listen on %s:%d", ip, port);
	}
	return serv;
}

void NetworkServer::serve(){
	writer = new ProcWorkerPool("writer");
	writer->start(num_writers);
	reader = new ProcWorkerPool("reader");
	reader->start(num_readers);

	ready_list_t ready_list;
	ready_list_t ready_list_2;
	ready_list_t::iterator it;
	const Fdevents::events_t *events;

	//epoll_ctl-注册epoll事件
	fdes->set(serv_link->fd(), FDEVENT_IN, 0, serv_link);
	fdes->set(this->reader->fd(), FDEVENT_IN, 0, this->reader);
	fdes->set(this->writer->fd(), FDEVENT_IN, 0, this->writer);
	
	uint32_t last_ticks = g_ticks;
	
	while(!quit){
		// status report
		if((uint32_t)(g_ticks - last_ticks) >= STATUS_REPORT_TICKS){
			last_ticks = g_ticks;
			log_info("server running, links: %d", this->link_count);
		}
		
		ready_list.swap(ready_list_2);
		ready_list_2.clear();
		
		if(!ready_list.empty()){
			// ready_list not empty, so we should return immediately
			events = fdes->wait(0);
		}else{
			events = fdes->wait(50);
		}
		if(events == NULL){
			log_fatal("events.wait error: %s", strerror(errno));
			break;
		}
		
		for(int i=0; i<(int)events->size(); i++){
			const Fdevent *fde = events->at(i);
			if(fde->data.ptr == serv_link){
				Link *link = accept_link();
				if(link){
					this->link_count ++;				
					log_debug("new link from %s:%d, fd: %d, links: %d",
						link->remote_ip, link->remote_port, link->fd(), this->link_count);
					fdes->set(link->fd(), FDEVENT_IN, 1, link);
				}
			}else if(fde->data.ptr == this->reader || fde->data.ptr == this->writer){
				ProcWorkerPool *worker = (ProcWorkerPool *)fde->data.ptr;
				ProcJob *job;
				if(worker->pop(&job) == 0){
					log_fatal("reading result from workers error!");
					exit(0);
				}
				if(proc_result(job, &ready_list) == PROC_ERROR){
					//
				}
			}else{
				proc_request(fde, &ready_list);
			}
		}

		for(it = ready_list.begin(); it != ready_list.end(); it ++){
			Link *link = *it;
			if(link->error()){
				this->link_count --;
				fdes->del(link->fd());
				delete link;
				continue;
			}

			const Request *req = link->recv();
			if(req == NULL){
				log_warn("fd: %d, link parse error, delete link", link->fd());
				this->link_count --;
				fdes->del(link->fd());
				delete link;
				continue;
			}
			if(req->empty()){
				fdes->set(link->fd(), FDEVENT_IN, 1, link);
				continue;
			}
			
			link->active_time = millitime();

			ProcJob *job = new ProcJob();
			job->link = link;
			job->req = link->last_recv();
			//处理job
			int result = this->proc(job);
			if(result == PROC_THREAD){
				fdes->del(link->fd());
				continue;
			}
			if(result == PROC_BACKEND){
				fdes->del(link->fd());
				this->link_count --;
				continue;
			}
			
			if(proc_result(job, &ready_list_2) == PROC_ERROR){
				//
			}
		} // end foreach ready link
	}
}

Link* NetworkServer::accept_link(){
	Link *link = serv_link->accept();
	if(link == NULL){
		log_error("accept failed! %s", strerror(errno));
		return NULL;
	}
	//所产生的结果都是使I/O变成非搁置模式(non-blocking)，在读取不到数据或是写入缓冲区已满会马上return，而不会搁置程序动作，直到有数据或写入完成。
	link->nodelay();
	link->noblock();
	link->create_time = millitime();
	link->active_time = link->create_time;
	return link;
}

//发送结果到客户端
int NetworkServer::proc_result(ProcJob *job, ready_list_t *ready_list){
	Link *link = job->link;
	int result = job->result;
			
	if(log_level() >= Logger::LEVEL_DEBUG){
		log_debug("w:%.3f,p:%.3f, req: %s, resp: %s",
			job->time_wait, job->time_proc,
			serialize_req(*job->req).c_str(),
			serialize_req(job->resp.resp).c_str());
	}
	//命令技术
	if(job->cmd){
		job->cmd->calls += 1;
		job->cmd->time_wait += job->time_wait;
		job->cmd->time_proc += job->time_proc;
	}
	delete job;
	
	if(result == PROC_ERROR){
		log_info("fd: %d, proc error, delete link", link->fd());
		goto proc_err;
	}
	
	if(!link->output->empty()){
		// 这里将任务产生的结果发送给客户端
		int len = link->write();
		//log_debug("write: %d", len);
		if(len < 0){
			log_debug("fd: %d, write: %d, delete link", link->fd(), len);
			goto proc_err;
		}
	}

	//输出缓冲区不为空, 则说明还有数据未完全发送给客户端，需要关注写事件
	if(!link->output->empty()){
		fdes->set(link->fd(), FDEVENT_OUT, 1, link);
	}
	//输入缓冲区为空, 说明客户端发送过来的数据已经全部解析完成，继续关注读事件
	if(link->input->empty()){
		fdes->set(link->fd(), FDEVENT_IN, 1, link);
	}else{
		// 读缓冲区不为空, 说明还有输入的数据需要处理，将link放到ready_list里面等待处理，同时
		// 暂时不再关注该连接上的读事件
		fdes->clr(link->fd(), FDEVENT_IN);
		ready_list->push_back(link);
	}
	return PROC_OK;

proc_err:
	this->link_count --;
	fdes->del(link->fd());
	delete link;
	return PROC_ERROR;
}

//处理交互
int NetworkServer::proc_request(const Fdevent *fde, ready_list_t *ready_list){
	Link *link = (Link *)fde->data.ptr;
	if(fde->events & FDEVENT_IN){
		ready_list->push_back(link);
		if(link->error()){
			return 0;
		}
		int len = link->read();
		//log_debug("fd: %d read: %d", link->fd(), len);
		if(len <= 0){
			log_debug("fd: %d, read: %d, delete link", link->fd(), len);
			link->mark_error();
			return 0;
		}
	}
	return 0;
}


int NetworkServer::proc(ProcJob *job){
	job->serv = this;
	job->result = PROC_OK;
	job->stime = millitime();

	const Request *req = job->req;

	do{
		//根据名字 在处理函数表中 找到命令
		Command *cmd = proc_map.get_proc(req->at(0));
		if(!cmd){
			job->resp.push_back("client_error");
			job->resp.push_back("Unknown Command: " + req->at(0).String());
			break;
		}
		job->cmd = cmd;

		//需要在子线程中执行的  丢到线程池中
		if(cmd->flags & Command::FLAG_THREAD){
			if(cmd->flags & Command::FLAG_WRITE){
				writer->push(job);
			}else{
				reader->push(job);
			}
			return PROC_THREAD;
		}

		//在本线程处理
		proc_t p = cmd->proc;
		job->time_wait = 1000 * (millitime() - job->stime);
		job->result = (*p)(this, job->link, *req, &job->resp);
		job->time_proc = 1000 * (millitime() - job->stime) - job->time_wait;
	}while(0);

	//把response发送到输出缓冲区
	if(job->link->send(job->resp.resp) == -1){
		job->result = PROC_ERROR;
	}
	return job->result;
}


/* built-in procs */

static int proc_ping(NetworkServer *net, Link *link, const Request &req, Response *resp){
	resp->push_back("ok");
	return 0;
}

static int proc_info(NetworkServer *net, Link *link, const Request &req, Response *resp){
	resp->push_back("ok");
	resp->push_back("version");
	resp->push_back("1.0");
	resp->push_back("links");
	resp->add(net->link_count);
	{
		int64_t calls = 0;
		proc_map_t::iterator it;
		for(it=net->proc_map.begin(); it!=net->proc_map.end(); it++){
			Command *cmd = it->second;
			calls += cmd->calls;
		}
		resp->push_back("total_calls");
		resp->add(calls);
	}
	return 0;
}
