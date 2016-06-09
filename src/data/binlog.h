/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef GEMINIDB_BINLOG_H_
#define GEMINIDB_BINLOG_H_

#include <string>
#include "../util/thread.h"
#include "../util/bytes.h"
#include "../../deps/leveldb-1.18/include/leveldb/db.h"
#include "../../deps/leveldb-1.18/include/leveldb/options.h"
#include "../../deps/leveldb-1.18/include/leveldb/slice.h"
#include "../../deps/leveldb-1.18/include/leveldb/status.h"
#include "../../deps/leveldb-1.18/include/leveldb/write_batch.h"

// circular queue
class BinlogQueue{
private:
	leveldb::DB *db;
	uint64_t min_seq;
	uint64_t last_seq;
	uint64_t tran_seq;
	int capacity;

	leveldb::WriteBatch batch;
	bool enabled;
public:
	Mutex  mutex;

	BinlogQueue(leveldb::DB *db, bool enabled=true, int capacity=20000000);
	~BinlogQueue();
	void begin();
	void rollback();
	leveldb::Status commit();

	void Put(const leveldb::Slice& key, const leveldb::Slice& value);
	void Delete(const leveldb::Slice& key);
	int update(uint64_t seq, char type, char cmd, const std::string &key);
};

class Transaction{
private:
	BinlogQueue *logs;
public:
	Transaction(BinlogQueue *logs){
		this->logs = logs;
		logs->mutex.lock();
		logs->begin();
	}
	
	~Transaction(){
		// it is safe to call rollback after commit
		logs->rollback();
		logs->mutex.unlock();
	}
};


#endif
