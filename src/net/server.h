#ifndef NET_SERVER_H_
#define NET_SERVER_H_

#include "../include.h"
#include <string>
#include <vector>

#include "fde.h"
#include "proc.h"
#include "worker.h"

class Link;
class Config;
class Fdevents;

typedef std::vector<Link *> ready_list_t;

class NetworkServer
{
private:
	int tick_interval;
	int status_report_ticks;

	Link *serv_link;
	Fdevents *fdes;

	Link* accept_link();
	int proc_result(ProcJob *job, ready_list_t *ready_list);
	int proc_request(const Fdevent *fde, ready_list_t *ready_list);

	int proc(ProcJob *job);

	int num_readers;			//读线程数
	int num_writers;			//写线程数
	ProcWorkerPool *writer;
	ProcWorkerPool *reader;

	NetworkServer();

public:
	void *data;					//数据服务器
	ProcMap proc_map;			//命令字典表
	int link_count;

	~NetworkServer();
	
	static NetworkServer* init(const char *conf_file, int num_readers=-1, int num_writers=-1);
	static NetworkServer* init(const Config &conf, int num_readers=-1, int num_writers=-1);
	void serve();
};


#endif
