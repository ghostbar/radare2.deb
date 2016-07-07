SDBPATH=../../shlr/sdb/src/
SDBLIB=${SDBPATH}/libsdb.a
EXTRA_TARGETS+=${SDBLIB}

SDB_OBJS=
SDB_OBJS+=buffer.o
SDB_OBJS+=cdb.o
SDB_OBJS+=cdb_make.o
SDB_OBJS+=ht.o
SDB_OBJS+=json.o
SDB_OBJS+=lock.o
SDB_OBJS+=ls.o
SDB_OBJS+=ns.o
SDB_OBJS+=query.o
SDB_OBJS+=sdb.o
SDB_OBJS+=base64.o
SDB_OBJS+=disk.o
SDB_OBJS+=array.o
SDB_OBJS+=fmt.o
SDB_OBJS+=match.o
SDB_OBJS+=num.o
SDB_OBJS+=util.o
SDB_OBJS+=journal.o

SDBOBJS=$(addprefix ${SDBPATH},${SDB_OBJS})

OBJS+=$(SDBOBJS)

CFLAGS+=-I$(SDBPATH)/src

sdb_version:
	$(MAKE) -C "$(SDBPATH)" sdb_version.h
	${MAKE} -C ${SDBPATH}
