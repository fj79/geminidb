#ifndef GEMINIDB_IMPL_H_
#define GEMINIDB_IMPL_H_

#include "../util/log.h"
#include "../util/config.h"

#include "geminidb.h"
#include "binlog.h"
#include "t_kv.h"
#include "t_queue.h"
#include "../../deps/leveldb-1.18/include/leveldb/slice.h"
#include "../../deps/leveldb-1.18/include/leveldb/db.h"
#include "../../deps/leveldb-1.18/include/leveldb/options.h"

inline
static leveldb::Slice slice(const Bytes &b){
	return leveldb::Slice(b.data(), b.size());
}

class GEMINIDBImpl : public GEMINIDB
{
private:
	friend class GEMINIDB;
	leveldb::DB* ldb;
	leveldb::Options options;

	GEMINIDBImpl();
public:
	BinlogQueue *binlogs;

	virtual ~GEMINIDBImpl();

	virtual uint64_t size();
	virtual std::vector<std::string> info();

	virtual int set(const Bytes &key, const Bytes &val, char log_type=BinlogType::SYNC);
	virtual int get(const Bytes &key, std::string *val);
	virtual int del(const Bytes &key, char log_type=BinlogType::SYNC);

	virtual int multi_set(const std::vector<Bytes> &kvs, int offset=0, char log_type=BinlogType::SYNC);
	virtual int multi_del(const std::vector<Bytes> &keys, int offset=0, char log_type=BinlogType::SYNC);

	virtual int64_t qsize(const Bytes &name);
	virtual int qfront(const Bytes &name, std::string *item);
	virtual int qback(const Bytes &name, std::string *item);
	virtual int64_t qpush_front(const Bytes &name, const Bytes &item, char log_type=BinlogType::SYNC);
	virtual int64_t qpush_back(const Bytes &name, const Bytes &item, char log_type=BinlogType::SYNC);
	virtual int qpop_front(const Bytes &name, std::string *item, char log_type=BinlogType::SYNC);
	virtual int qpop_back(const Bytes &name, std::string *item, char log_type=BinlogType::SYNC);

private:
	int64_t _qpush(const Bytes &name, const Bytes &item, uint64_t front_or_back_seq, char log_type=BinlogType::SYNC);
	int _qpop(const Bytes &name, std::string *item, uint64_t front_or_back_seq, char log_type=BinlogType::SYNC);
};

#endif
