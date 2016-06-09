PREFIX=/usr/local/geminidb

$(shell sh build.sh 1>&2)
include build_config.mk

all:
	mkdir -p var
	chmod u+x "${LEVELDB_PATH}/build_detect_platform"
	cd "${LEVELDB_PATH}"; ${MAKE}
	cd src/util; ${MAKE}
	cd src/net; ${MAKE}
	cd src/data; ${MAKE}
	cd src; ${MAKE}

clean:
	cd src/util; ${MAKE} clean
	cd src/net; ${MAKE} clean
	cd src/data; ${MAKE} clean
	cd src; ${MAKE} clean
