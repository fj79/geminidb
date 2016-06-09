#include "binlog.h"
#include "const.h"
#include "../include.h"
#include "../util/log.h"
#include "../util/strings.h"
#include <map>

BinlogQueue::BinlogQueue(leveldb::DB *db, bool enabled, int capacity){
	this->db = db;
	this->min_seq = 0;
	this->last_seq = 0;
	this->tran_seq = 0;
	this->capacity = capacity;
	this->enabled = enabled;
	
	if(this->last_seq > this->capacity){
		this->min_seq = this->last_seq - this->capacity;
	}else{
		this->min_seq = 0;
	}
}

BinlogQueue::~BinlogQueue(){
	db = NULL;
}


void BinlogQueue::begin(){
	tran_seq = last_seq;
	batch.Clear();
}

void BinlogQueue::rollback(){
	tran_seq = 0;
}

leveldb::Status BinlogQueue::commit(){
	leveldb::WriteOptions write_opts;
	leveldb::Status s = db->Write(write_opts, &batch);
	if(s.ok()){
		last_seq = tran_seq;
		tran_seq = 0;
	}
	return s;
}


void BinlogQueue::Put(const leveldb::Slice& key, const leveldb::Slice& value){
	batch.Put(key, value);
}

void BinlogQueue::Delete(const leveldb::Slice& key){
	batch.Delete(key);
}