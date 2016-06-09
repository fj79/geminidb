#include "t_queue.h"
#include "../../deps/leveldb-1.18/include/leveldb/status.h"

static int qget_by_seq(leveldb::DB* db, const Bytes &name, uint64_t seq, std::string *val){
	std::string key = encode_qitem_key(name, seq);
	leveldb::Status s;

	s = db->Get(leveldb::ReadOptions(), key, val);
	if(s.IsNotFound()){
		return 0;
	}else if(!s.ok()){
		log_error("Get() error!");
		return -1;
	}else{
		return 1;
	}
}

static int qget_uint64(leveldb::DB* db, const Bytes &name, uint64_t seq, uint64_t *ret){
	std::string val;
	*ret = 0;
	int s = qget_by_seq(db, name, seq, &val);
	if(s == 1){
		if(val.size() != sizeof(uint64_t)){
			return -1;
		}
		*ret = *(uint64_t *)val.data();
	}
	return s;
}

static int qdel_one(GEMINIDBImpl *ssdb, const Bytes &name, uint64_t seq){
	std::string key = encode_qitem_key(name, seq);
	leveldb::Status s;

	ssdb->binlogs->Delete(key);
	return 0;
}

static int qset_one(GEMINIDBImpl *ssdb, const Bytes &name, uint64_t seq, const Bytes &item){
	std::string key = encode_qitem_key(name, seq);
	leveldb::Status s;

	ssdb->binlogs->Put(key, slice(item));
	return 0;
}

static int64_t incr_qsize(GEMINIDBImpl *ssdb, const Bytes &name, int64_t incr){
	int64_t size = ssdb->qsize(name);
	if(size == -1){
		return -1;
	}
	size += incr;
	if(size <= 0){
		ssdb->binlogs->Delete(encode_qsize_key(name));
		qdel_one(ssdb, name, QFRONT_SEQ);
		qdel_one(ssdb, name, QBACK_SEQ);
	}else{
		ssdb->binlogs->Put(encode_qsize_key(name), leveldb::Slice((char *)&size, sizeof(size)));
	}
	return size;
}

/****************/

int64_t GEMINIDBImpl::qsize(const Bytes &name){
	std::string key = encode_qsize_key(name);
	std::string val;

	leveldb::Status s;
	s = ldb->Get(leveldb::ReadOptions(), key, &val);
	if(s.IsNotFound()){
		return 0;
	}else if(!s.ok()){
		log_error("Get() error!");
		return -1;
	}else{
		if(val.size() != sizeof(uint64_t)){
			return -1;
		}
		return *(int64_t *)val.data();
	}
}

// @return 0: empty queue, 1: item peeked, -1: error
int GEMINIDBImpl::qfront(const Bytes &name, std::string *item){
	int ret = 0;
	uint64_t seq;
	ret = qget_uint64(this->ldb, name, QFRONT_SEQ, &seq);
	if(ret == -1){
		return -1;
	}
	if(ret == 0){
		return 0;
	}
	ret = qget_by_seq(this->ldb, name, seq, item);
	return ret;
}

// @return 0: empty queue, 1: item peeked, -1: error
int GEMINIDBImpl::qback(const Bytes &name, std::string *item){
	int ret = 0;
	uint64_t seq;
	ret = qget_uint64(this->ldb, name, QBACK_SEQ, &seq);
	if(ret == -1){
		return -1;
	}
	if(ret == 0){
		return 0;
	}
	ret = qget_by_seq(this->ldb, name, seq, item);
	return ret;
}


int64_t GEMINIDBImpl::_qpush(const Bytes &name, const Bytes &item, uint64_t front_or_back_seq, char log_type){
	Transaction trans(binlogs);

	int ret;
	// generate seq
	uint64_t seq;
	ret = qget_uint64(this->ldb, name, front_or_back_seq, &seq);
	if(ret == -1){
		return -1;
	}
	// update front and/or back
	if(ret == 0){
		seq = QITEM_SEQ_INIT;
		ret = qset_one(this, name, QFRONT_SEQ, Bytes(&seq, sizeof(seq)));
		if(ret == -1){
			return -1;
		}
		ret = qset_one(this, name, QBACK_SEQ, Bytes(&seq, sizeof(seq)));
	}else{
		seq += (front_or_back_seq == QFRONT_SEQ)? -1 : +1;
		ret = qset_one(this, name, front_or_back_seq, Bytes(&seq, sizeof(seq)));
	}
	if(ret == -1){
		return -1;
	}
	if(seq <= QITEM_MIN_SEQ || seq >= QITEM_MAX_SEQ){
		log_info("queue is full, seq: %" PRIu64 " out of range", seq);
		return -1;
	}
	
	// prepend/append item
	ret = qset_one(this, name, seq, item);
	if(ret == -1){
		return -1;
	}

	std::string buf = encode_qitem_key(name, seq);
//	if(front_or_back_seq == QFRONT_SEQ){
//		binlogs->add_log(log_type, BinlogCommand::QPUSH_FRONT, buf);
//	}else{
//		binlogs->add_log(log_type, BinlogCommand::QPUSH_BACK, buf);
//	}

	// update size
	int64_t size = incr_qsize(this, name, +1);
	if(size == -1){
		return -1;
	}

	leveldb::Status s = binlogs->commit();
	if(!s.ok()){
		log_error("Write error! %s", s.ToString().c_str());
		return -1;
	}
	return size;
}

int64_t GEMINIDBImpl::qpush_front(const Bytes &name, const Bytes &item, char log_type){
	return _qpush(name, item, QFRONT_SEQ, log_type);
}

int64_t GEMINIDBImpl::qpush_back(const Bytes &name, const Bytes &item, char log_type){
	return _qpush(name, item, QBACK_SEQ, log_type);
}

int GEMINIDBImpl::_qpop(const Bytes &name, std::string *item, uint64_t front_or_back_seq, char log_type){
	Transaction trans(binlogs);
	
	int ret;
	uint64_t seq;
	ret = qget_uint64(this->ldb, name, front_or_back_seq, &seq);
	if(ret == -1){
		return -1;
	}
	if(ret == 0){
		return 0;
	}
	
	ret = qget_by_seq(this->ldb, name, seq, item);
	if(ret == -1){
		return -1;
	}
	if(ret == 0){
		return 0;
	}

	// delete item
	ret = qdel_one(this, name, seq);
	if(ret == -1){
		return -1;
	}

//	if(front_or_back_seq == QFRONT_SEQ){
//		binlogs->add_log(log_type, BinlogCommand::QPOP_FRONT, name.String());
//	}else{
//		binlogs->add_log(log_type, BinlogCommand::QPOP_BACK, name.String());
//	}

	// update size
	int64_t size = incr_qsize(this, name, -1);
	if(size == -1){
		return -1;
	}
		
	// update front
	if(size > 0){
		seq += (front_or_back_seq == QFRONT_SEQ)? +1 : -1;
		//log_debug("seq: %" PRIu64 ", ret: %d", seq, ret);
		ret = qset_one(this, name, front_or_back_seq, Bytes(&seq, sizeof(seq)));
		if(ret == -1){
			return -1;
		}
	}
		
	leveldb::Status s = binlogs->commit();
	if(!s.ok()){
		log_error("Write error! %s", s.ToString().c_str());
		return -1;
	}
	return 1;
}

// @return 0: empty queue, 1: item popped, -1: error
int GEMINIDBImpl::qpop_front(const Bytes &name, std::string *item, char log_type){
	return _qpop(name, item, QFRONT_SEQ, log_type);
}

int GEMINIDBImpl::qpop_back(const Bytes &name, std::string *item, char log_type){
	return _qpop(name, item, QBACK_SEQ, log_type);
}