include ../../build_config.mk

OBJS = log.o config.o bytes.o
EXES = 

all: ${OBJS}
	ar -cru ./libutil.a ${OBJS}
	rm *.o

log.o: log.h log.cpp
	${CXX} ${CFLAGS} -c log.cpp

config.o: config.h config.cpp
	${CXX} ${CFLAGS} -c config.cpp

bytes.o: bytes.h bytes.cpp
	${CXX} ${CFLAGS} -c bytes.cpp

test:

clean:
	rm -f ${EXES} ${OBJS} *.o *.exe *.a

