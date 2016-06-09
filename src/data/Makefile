include ../../build_config.mk

OBJS = geminidb_impl.o options.o t_kv.o t_queue.o binlog.o
LIBS = ../util/libutil.a


all: geminidb.h ${OBJS}
	ar -cru ./libdata.a ${OBJS}
	rm *.o

geminidb_impl.o: geminidb.h geminidb_impl.h geminidb_impl.cpp
	${CXX} ${CFLAGS} -c geminidb_impl.cpp
options.o: geminidb.h options.h options.cpp
	${CXX} ${CFLAGS} -c options.cpp
t_kv.o: geminidb.h t_kv.h t_kv.cpp
	${CXX} ${CFLAGS} -c t_kv.cpp
t_queue.o: geminidb.h t_queue.h t_queue.cpp
	${CXX} ${CFLAGS} -c t_queue.cpp
binlog.o: geminidb.h binlog.h binlog.cpp
	${CXX} ${CFLAGS} -c binlog.cpp

test:
	${CXX} -o test.out test.cpp ${OBJS} ${CFLAGS} ${LIBS} ${CLIBS}

clean:
	rm -f ${EXES} *.o *.exe *.a

