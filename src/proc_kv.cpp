/* proc kv */
#include "serv.h"
#include "net/proc.h"
#include "net/server.h"

int proc_get(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	std::string val;
	int ret = serv->geminidb->get(req[1], &val);
	resp->reply_get(ret, &val);
	return 0;
}

int proc_set(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(3);

	int ret = serv->geminidb->set(req[1], req[2]);
	if(ret == -1){
		resp->push_back("error");
	}else{
		resp->push_back("ok");
		resp->push_back("1");
	}
	return 0;
}

int proc_exists(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	const Bytes key = req[1];
	std::string val;
	int ret = serv->geminidb->get(key, &val);
	resp->reply_bool(ret);
	return 0;
}

int proc_multi_exists(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	resp->push_back("ok");
	for(Request::const_iterator it=req.begin()+1; it!=req.end(); it++){
		const Bytes key = *it;
		std::string val;
		int ret = serv->geminidb->get(key, &val);
		resp->push_back(key.String());
		if(ret == 1){
			resp->push_back("1");
		}else if(ret == 0){
			resp->push_back("0");
		}else{
			resp->push_back("0");
		}
	}
	return 0;
}

int proc_multi_set(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	if(req.size() < 3 || req.size() % 2 != 1){
		resp->push_back("client_error");
	}else{
		int ret = serv->geminidb->multi_set(req, 1);
		resp->reply_int(0, ret);
	}
	return 0;
}

int proc_multi_del(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	int ret = serv->geminidb->multi_del(req, 1);
	if(ret == -1){
		resp->push_back("error");
	}else{
		for(Request::const_iterator it=req.begin()+1; it!=req.end(); it++){
			const Bytes key = *it;
		}
		resp->reply_int(0, ret);
	}
	return 0;
}

int proc_multi_get(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);

	resp->push_back("ok");
	for(int i=1; i<req.size(); i++){
		std::string val;
		int ret = serv->geminidb->get(req[i], &val);
		if(ret == 1){
			resp->push_back(req[i].String());
			resp->push_back(val);
		}
	}
	return 0;
}

int proc_del(NetworkServer *net, Link *link, const Request &req, Response *resp){
	ProcServer *serv = (ProcServer *)net->data;
	CHECK_NUM_PARAMS(2);
	int ret = serv->geminidb->del(req[1]);
	if(ret == -1){
		resp->push_back("error");
	}else{
		resp->push_back("ok");
		resp->push_back("1");
	}
	return 0;
}