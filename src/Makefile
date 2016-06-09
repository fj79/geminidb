include ../build_config.mk

OBJS = proc_kv.o proc_queue.o serv.o
LIBS = ./data/libdata.a ./util/libutil.a ./net/libnet.a
EXES = ../geminidb-server


all: ${OBJS} geminidb-server.o
	${CXX} -o ../geminidb-server geminidb-server.o ${OBJS} ${LIBS} ${CLIBS}
	rm *.o

geminidb-server.o: geminidb-server.cpp
	${CXX} ${CFLAGS} -c geminidb-server.cpp

proc.o: serv.h proc.cpp
	${CXX} ${CFLAGS} -c proc.cpp
proc_kv.o: serv.h proc_kv.cpp
	${CXX} ${CFLAGS} -c proc_kv.cpp
proc_queue.o: serv.h proc_queue.cpp
	${CXX} ${CFLAGS} -c proc_queue.cpp

serv.o: serv.h serv.cpp
	${CXX} ${CFLAGS} -c serv.cpp

clean:
	rm -f ${EXES} *.o *.exe *.a

