#ifndef GEMINIDB_H_
#define GEMINIDB_H_

#include <vector>
#include <string>
#include "const.h"
#include "options.h"

class Bytes;

class Config;

class GEMINIDB {
public:
    GEMINIDB() {}

    virtual ~GEMINIDB() {};

    static GEMINIDB *open(const Options &opt, const std::string &base_dir);

    virtual uint64_t size() = 0;

    virtual std::vector<std::string> info() = 0;

    virtual int set(const Bytes &key, const Bytes &val, char log_type = BinlogType::SYNC) = 0;

    virtual int get(const Bytes &key, std::string *val) = 0;

    virtual int del(const Bytes &key, char log_type = BinlogType::SYNC) = 0;

    virtual int multi_set(const std::vector<Bytes> &kvs, int offset = 0, char log_type = BinlogType::SYNC) = 0;

    virtual int multi_del(const std::vector<Bytes> &keys, int offset = 0, char log_type = BinlogType::SYNC) = 0;

    virtual int64_t qsize(const Bytes &name) = 0;

    virtual int qfront(const Bytes &name, std::string *item) = 0;

    virtual int qback(const Bytes &name, std::string *item) = 0;

    virtual int64_t qpush_front(const Bytes &name, const Bytes &item, char log_type = BinlogType::SYNC) = 0;

    virtual int64_t qpush_back(const Bytes &name, const Bytes &item, char log_type = BinlogType::SYNC) = 0;

    virtual int qpop_front(const Bytes &name, std::string *item, char log_type = BinlogType::SYNC) = 0;

    virtual int qpop_back(const Bytes &name, std::string *item, char log_type = BinlogType::SYNC) = 0;
};

#endif
