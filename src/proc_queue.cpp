/* proc queue */
#include "serv.h"
#include "net/proc.h"
#include "net/server.h"

int proc_qsize(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	int64_t ret = serv->geminidb->qsize(req[1]);
	resp->reply_int(ret, ret);
	return 0;
}

int proc_qfront(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	std::string item;
	int ret = serv->geminidb->qfront(req[1], &item);
	resp->reply_get(ret, &item);
	return 0;
}

int proc_qback(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	std::string item;
	int ret = serv->geminidb->qback(req[1], &item);
	resp->reply_get(ret, &item);
	return 0;
}

static int QFRONT = 2;
static int QBACK  = 3;

static inline
int proc_qpush_func(NetworkServer *net, Link *link, const Request &req, Response *resp, int front_or_back){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(3);

	int64_t size = 0;
	std::vector<Bytes>::const_iterator it;
	it = req.begin() + 2;
	for(; it != req.end(); it += 1){
		const Bytes &item = *it;
		if(front_or_back == QFRONT){
			size = serv->geminidb->qpush_front(req[1], item);
		}else{
			size = serv->geminidb->qpush_back(req[1], item);
		}
		if(size == -1){
			resp->push_back("error");
			return 0;
		}
	}
	resp->reply_int(0, size);
	return 0;
}

int proc_qpush_front(NetworkServer *net, Link *link, const Request &req, Response *resp){
	return proc_qpush_func(net, link, req, resp, QFRONT);
}

int proc_qpush_back(NetworkServer *net, Link *link, const Request &req, Response *resp){
	return proc_qpush_func(net, link, req, resp, QBACK);
}

int proc_qpush(NetworkServer *net, Link *link, const Request &req, Response *resp){
	return proc_qpush_func(net, link, req, resp, QBACK);
}


static inline
int proc_qpop_func(NetworkServer *net, Link *link, const Request &req, Response *resp, int front_or_back){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);
	
	uint64_t size = 1;
	if(req.size() > 2){
		size = req[2].Uint64();
	}
		
	int ret;
	std::string item;

	if(size == 1){
		if(front_or_back == QFRONT){
			ret = serv->geminidb->qpop_front(req[1], &item);
		}else{
			ret = serv->geminidb->qpop_back(req[1], &item);
		}
		resp->reply_get(ret, &item);
	}else{
		resp->push_back("ok");
		while(size-- > 0){
			if(front_or_back == QFRONT){
				ret = serv->geminidb->qpop_front(req[1], &item);
			}else{
				ret = serv->geminidb->qpop_back(req[1], &item);
			}
			if(ret <= 0){
				break;
			}else{
				resp->push_back(item);
			}
		}
	}

	return 0;
}

int proc_qpop_front(NetworkServer *net, Link *link, const Request &req, Response *resp){
	return proc_qpop_func(net, link, req, resp, QFRONT);
}

int proc_qpop_back(NetworkServer *net, Link *link, const Request &req, Response *resp){
	return proc_qpop_func(net, link, req, resp, QBACK);
}

int proc_qpop(NetworkServer *net, Link *link, const Request &req, Response *resp){
	return proc_qpop_func(net, link, req, resp, QFRONT);
}