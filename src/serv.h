#ifndef Proc_SERVER_H_
#define Proc_SERVER_H_

#include "include.h"
#include <map>
#include <vector>
#include <string>
#include "data/geminidb_impl.h"
#include "net/server.h"
#include "version.h"

class ProcServer {
private:
	void reg_procs(NetworkServer *net);

public:
	GEMINIDBImpl *geminidb;

	ProcServer(GEMINIDB *geminidb, NetworkServer *net);

	~ProcServer();
};


#define CHECK_NUM_PARAMS(n) do{ \
		if(req.size() < n){ \
			resp->push_back("client_error"); \
			resp->push_back("wrong number of arguments"); \
			return 0; \
		} \
	}while(0)

#endif
