#include "t_kv.h"
#include "../../deps/leveldb-1.18/include/leveldb/status.h"

int GEMINIDBImpl::multi_set(const std::vector<Bytes> &kvs, int offset, char log_type){
	Transaction trans(binlogs);

	std::vector<Bytes>::const_iterator it;
	it = kvs.begin() + offset;
	for(; it != kvs.end(); it += 2){
		const Bytes &key = *it;
		if(key.empty()){
			log_error("empty key!");
			return 0;
		}
		const Bytes &val = *(it + 1);
		std::string buf = encode_kv_key(key);
		binlogs->Put(buf, slice(val));
	}
	leveldb::Status s = binlogs->commit();
	if(!s.ok()){
		log_error("multi_set error: %s", s.ToString().c_str());
		return -1;
	}
	return (kvs.size() - offset)/2;
}

int GEMINIDBImpl::multi_del(const std::vector<Bytes> &keys, int offset, char log_type){
	Transaction trans(binlogs);

	std::vector<Bytes>::const_iterator it;
	it = keys.begin() + offset;
	for(; it != keys.end(); it++){
		const Bytes &key = *it;
		std::string buf = encode_kv_key(key);
		binlogs->Delete(buf);
	}
	leveldb::Status s = binlogs->commit();
	if(!s.ok()){
		log_error("multi_del error: %s", s.ToString().c_str());
		return -1;
	}
	return keys.size() - offset;
}

int GEMINIDBImpl::set(const Bytes &key, const Bytes &val, char log_type){
	if(key.empty()){
		log_error("empty key!");
		return 0;
	}
	Transaction trans(binlogs);

	std::string buf = encode_kv_key(key);
	binlogs->Put(buf, slice(val));
	leveldb::Status s = binlogs->commit();
	if(!s.ok()){
		log_error("set error: %s", s.ToString().c_str());
		return -1;
	}
	return 1;
}


int GEMINIDBImpl::del(const Bytes &key, char log_type){
	Transaction trans(binlogs);

	std::string buf = encode_kv_key(key);
	binlogs->Delete(buf);
	leveldb::Status s = binlogs->commit();
	if(!s.ok()){
		log_error("del error: %s", s.ToString().c_str());
		return -1;
	}
	return 1;
}


int GEMINIDBImpl::get(const Bytes &key, std::string *val){
	std::string buf = encode_kv_key(key);

	leveldb::Status s = ldb->Get(leveldb::ReadOptions(), buf, val);
	if(s.IsNotFound()){
		return 0;
	}
	if(!s.ok()){
		log_error("get error: %s", s.ToString().c_str());
		return -1;
	}
	return 1;
}