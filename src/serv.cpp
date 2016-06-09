#include "util/log.h"
#include "util/strings.h"
#include "serv.h"

DEF_PROC(get);
DEF_PROC(set);
DEF_PROC(del);
DEF_PROC(exists);
DEF_PROC(multi_exists);
DEF_PROC(multi_get);
DEF_PROC(multi_set);
DEF_PROC(multi_del);

DEF_PROC(qsize);
DEF_PROC(qfront);
DEF_PROC(qback);
DEF_PROC(qpush);
DEF_PROC(qpush_front);
DEF_PROC(qpush_back);
DEF_PROC(qpop);
DEF_PROC(qpop_front);
DEF_PROC(qpop_back);

DEF_PROC(info);
DEF_PROC(version);
DEF_PROC(dbsize);

#define REG_PROC(c, f)     net->proc_map.set_proc(#c, f, proc_##c)

void ProcServer::reg_procs(NetworkServer *net){
	REG_PROC(get, "rt");
	REG_PROC(set, "wt");
	REG_PROC(del, "wt");
	REG_PROC(exists, "rt");
	REG_PROC(multi_exists, "rt");
	REG_PROC(multi_get, "rt");
	REG_PROC(multi_set, "wt");
	REG_PROC(multi_del, "wt");

	REG_PROC(qsize, "rt");
	REG_PROC(qfront, "rt");
	REG_PROC(qback, "rt");
	REG_PROC(qpush, "wt");
	REG_PROC(qpush_front, "wt");
	REG_PROC(qpush_back, "wt");
	REG_PROC(qpop, "wt");
	REG_PROC(qpop_front, "wt");
	REG_PROC(qpop_back, "wt");

	REG_PROC(info, "r");
	REG_PROC(version, "r");
	REG_PROC(dbsize, "rt");
}


ProcServer::ProcServer(GEMINIDB *geminidb,NetworkServer *net){
	this->geminidb = (GEMINIDBImpl *)geminidb;
	net->data = this;
	//注册处理函数
	this->reg_procs(net);
}

ProcServer::~ProcServer(){
}

int proc_dbsize(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	uint64_t size = serv->geminidb->size();
	resp->push_back("ok");
	resp->push_back(str(size));
	return 0;
}

int proc_version(NetworkServer *net, Link *link, const Request &req, Response *resp){
	resp->push_back("ok");
	resp->push_back("1.0.0");
	return 0;
}

int proc_info(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	resp->push_back("ok");
	resp->push_back("geminiDB-server");
	resp->push_back("version");
	resp->push_back(GEMINIDB_VERSION);
	{
		resp->push_back("links");
		resp->add(net->link_count);
	}
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
	
	{
		uint64_t size = serv->geminidb->size();
		resp->push_back("dbsize");
		resp->push_back(str(size));
	}

	if(req.size() == 1 || req[1] == "leveldb"){
		std::vector<std::string> tmp = serv->geminidb->info();
		for(int i=0; i<(int)tmp.size(); i++){
			std::string block = tmp[i];
			resp->push_back(block);
		}
	}

	if(req.size() > 1 && req[1] == "cmd"){
		proc_map_t::iterator it;
		for(it=net->proc_map.begin(); it!=net->proc_map.end(); it++){
			Command *cmd = it->second;
			resp->push_back("cmd." + cmd->name);
			char buf[128];
			snprintf(buf, sizeof(buf), "calls: %" PRIu64 "\ttime_wait: %.0f\ttime_proc: %.0f",
				cmd->calls, cmd->time_wait, cmd->time_proc);
			resp->push_back(buf);
		}
	}
	
	return 0;
}
