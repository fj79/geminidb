#include "include.h"
#include "version.h"
#include "net/server.h"
#include "serv.h"
#include "util/log.h"
#include "util/file.h"
#include "util/config.h"
#include "strings.h"
#include <stdio.h>

#define APP_NAME "geminiDB-server"
#define APP_VERSION GEMINIDB_VERSION


class MyApplication
{
public:
    MyApplication(){};
    ~MyApplication(){};
    void welcome();
	void run();
    void parse_args(int argc, char **argv);
    void init();

    int read_pid();
    void write_pid();
    int check_pidfile();
    void remove_pidfile();
    void kill_process();

    struct AppArgs{
        bool is_daemon;
        std::string pidfile;
        std::string conf_file;
        std::string work_dir;
        std::string start_opt;

        AppArgs(){
            start_opt = "start";
        }
    };

    Config *conf;
    AppArgs app_args;

};

void MyApplication::run(){
	Options option;
	option.load(*conf);

	std::string data_db_dir = app_args.work_dir + "/data";

	log_info("geminiDB-server %s", APP_VERSION);
	log_info("conf_file        : %s", app_args.conf_file.c_str());
	log_info("log_level        : %s", Logger::shared()->level_name().c_str());
	log_info("log_output       : %s", Logger::shared()->output_name().c_str());
	log_info("log_rotate_size  : %" PRId64, Logger::shared()->rotate_size());

	log_info("main_db          : %s", data_db_dir.c_str());
	log_info("cache_size       : %d MB", option.cache_size);
	log_info("block_size       : %d KB", option.block_size);
	log_info("write_buffer     : %d MB", option.write_buffer_size);
	log_info("max_open_files   : %d", option.max_open_files);
	log_info("compaction_speed : %d MB/s", option.compaction_speed);
	log_info("compression      : %s", option.compression.c_str());
	log_info("binlog           : %s", option.binlog? "yes" : "no");
	log_info("binlog_capacity  : %d", option.binlog_capacity);

	GEMINIDB *data_db = NULL;
	data_db = GEMINIDB::open(option, data_db_dir);
	if(!data_db){
		log_fatal("could not open data db: %s", data_db_dir.c_str());
		fprintf(stderr, "could not open data db: %s\n", data_db_dir.c_str());
		exit(1);
	}

	NetworkServer *net = NULL;
	net = NetworkServer::init(*conf);

	ProcServer *procServer;
	procServer = new ProcServer(data_db, net);

	log_info("pidfile: %s, pid: %d", app_args.pidfile.c_str(), (int)getpid());
	log_info("data server started.");
	net->serve();

	delete net;
	delete procServer;
	delete data_db;

	log_info("%s exit.", APP_NAME);
}

void MyApplication::welcome(){
    fprintf(stderr, "%s %s\n", APP_NAME, APP_VERSION);
    fprintf(stderr, "Copyright (c) 2016-2016 geminidb.io\n");
    fprintf(stderr, "\n");
}

void MyApplication::parse_args(int argc, char **argv){
	for(int i=1; i<argc; i++){
		std::string arg = argv[i];
        app_args.conf_file = argv[i];
	}
	if(app_args.conf_file.empty())
		exit(1);
}

void MyApplication::init(){
	if(!is_file(app_args.conf_file.c_str())){
		fprintf(stderr, "'%s' is not a file or not exists!\n", app_args.conf_file.c_str());
		exit(1);
	}
	conf = Config::load(app_args.conf_file.c_str());
	if(!conf){
		fprintf(stderr, "error loading conf file: '%s'\n", app_args.conf_file.c_str());
		exit(1);
	}
	{
		std::string conf_dir = real_dirname(app_args.conf_file.c_str());
		if(chdir(conf_dir.c_str()) == -1){
			fprintf(stderr, "error chdir: %s\n", conf_dir.c_str());
			exit(1);
		}
	}
	app_args.pidfile = conf->get_str("pidfile");
	if (check_pidfile())
		remove_pidfile();
	{ // init logger
		std::string log_output;
		std::string log_level_;
		int64_t log_rotate_size;

		log_level_ = conf->get_str("logger.level");
		strtolower(&log_level_);
		if(log_level_.empty()){
			log_level_ = "debug";
		}
		int level = Logger::get_level(log_level_.c_str());
		log_rotate_size = conf->get_int64("logger.rotate.size");
		log_output = conf->get_str("logger.output");
		if(log_output == ""){
			log_output = "stdout";
		}
		if(log_open(log_output.c_str(), level, true, log_rotate_size) == -1){
			fprintf(stderr, "error opening log file: %s\n", log_output.c_str());
			exit(1);
		}
	}

	app_args.work_dir = conf->get_str("work_dir");
	if(app_args.work_dir.empty()){
		app_args.work_dir = ".";
	}
	if(!is_dir(app_args.work_dir.c_str())){
		fprintf(stderr, "'%s' is not a directory or not exists!\n", app_args.work_dir.c_str());
		exit(1);
	}
}

int MyApplication::read_pid(){
	if(app_args.pidfile.empty()){
		return -1;
	}
	std::string s;
	file_get_contents(app_args.pidfile, &s);
	if(s.empty()){
		return -1;
	}
	return str_to_int(s);
}

void MyApplication::write_pid(){
	if(app_args.pidfile.empty()){
		return;
	}
	int pid = (int)getpid();
	std::string s = str(pid);
	int ret = file_put_contents(app_args.pidfile, s);
	if(ret == -1){
		log_error("Failed to write pidfile '%s'(%s)", app_args.pidfile.c_str(), strerror(errno));
		exit(1);
	}
}

int MyApplication::check_pidfile(){
	if(app_args.pidfile.size()){
		if(access(app_args.pidfile.c_str(), F_OK) == 0){
		}
	}
	return 1;
}

void MyApplication::remove_pidfile(){
	if(app_args.pidfile.size())
		remove(app_args.pidfile.c_str());
}

void MyApplication::kill_process(){
	int pid = read_pid();
	if(pid == -1){
		fprintf(stderr, "could not read pidfile: %s(%s)\n", app_args.pidfile.c_str(), strerror(errno));
		exit(1);
	}
	if(kill(pid, 0) == -1 && errno == ESRCH){
		fprintf(stderr, "process: %d not running\n", pid);
		remove_pidfile();
		return;
	}
	int ret = kill(pid, SIGTERM);
	if(ret == -1){
		fprintf(stderr, "could not kill process: %d(%s)\n", pid, strerror(errno));
		exit(1);
	}

	while(file_exists(app_args.pidfile)){
		usleep(100 * 1000);
	}
}

int main(int argc, char **argv){
    MyApplication myApplication;

    myApplication.welcome();
    myApplication.parse_args(argc, argv);
    myApplication.init();

    myApplication.write_pid();
    myApplication.run();
    myApplication.remove_pidfile();

    return 0;
}