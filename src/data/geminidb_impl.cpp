/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#include "geminidb_impl.h"

#include "t_kv.h"
#include "t_queue.h"
#include "../../deps/leveldb-1.18/include/leveldb/env.h"
#include "../../deps/leveldb-1.18/include/leveldb/status.h"
#include "../../deps/leveldb-1.18/include/leveldb/cache.h"
#include "../../deps/leveldb-1.18/include/leveldb/filter_policy.h"

GEMINIDBImpl::GEMINIDBImpl(){
	ldb = NULL;
	binlogs = NULL;
}

GEMINIDBImpl::~GEMINIDBImpl(){
	if(binlogs){
		delete binlogs;
	}
	if(ldb){
		delete ldb;
	}
	if(options.block_cache){
		delete options.block_cache;
	}
	if(options.filter_policy){
		delete options.filter_policy;
	}
}

GEMINIDB* GEMINIDB::open(const Options &opt, const std::string &dir){
	GEMINIDBImpl *geminidb = new GEMINIDBImpl();
	//初始化levelDB
	geminidb->options.create_if_missing = true;
	geminidb->options.max_open_files = opt.max_open_files;
	geminidb->options.filter_policy = leveldb::NewBloomFilterPolicy(10);
	geminidb->options.block_cache = leveldb::NewLRUCache(opt.cache_size * 1048576);
	geminidb->options.block_size = opt.block_size * 1024;
	geminidb->options.write_buffer_size = opt.write_buffer_size * 1024 * 1024;
	geminidb->options.compaction_speed = opt.compaction_speed;
	if(opt.compression == "yes"){
		geminidb->options.compression = leveldb::kSnappyCompression;
	}else{
		geminidb->options.compression = leveldb::kNoCompression;
	}
	leveldb::Status status;
	status = leveldb::DB::Open(geminidb->options, dir, &geminidb->ldb);
	if(!status.ok()){
		log_error("open db failed: %s", status.ToString().c_str());
		goto err;
	}
	//使用levelDB创建binlog
	geminidb->binlogs = new BinlogQueue(geminidb->ldb, opt.binlog, opt.binlog_capacity);
	return geminidb;
err:
	if(geminidb){
		delete geminidb;
	}
	return NULL;
}

uint64_t GEMINIDBImpl::size(){
	std::string s = "A";
	std::string e(1, 'z' + 1);
	leveldb::Range ranges[1];
	ranges[0] = leveldb::Range(s, e);
	uint64_t sizes[1];
	ldb->GetApproximateSizes(ranges, 1, sizes);
	return sizes[0];
}

std::vector<std::string> GEMINIDBImpl::info(){
	std::vector<std::string> info;
	std::vector<std::string> keys;
	keys.push_back("leveldb.stats");
	for(size_t i=0; i<keys.size(); i++){
		std::string key = keys[i];
		std::string val;
		if(ldb->GetProperty(key, &val)){
			info.push_back(key);
			info.push_back(val);
		}
	}
	return info;
}