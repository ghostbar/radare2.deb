OBJ_TAR=fs_tar.o
EXTRA=../p/grub/libgrubfs.a
CFLAGS+=-Igrub/include

STATIC_OBJ+=${OBJ_TAR}
#STATIC_OBJ+=${EXTRA}
TARGET_TAR=fs_tar.${EXT_SO}

ALL_TARGETS+=${TARGET_TAR}

${TARGET_TAR}: ${OBJ_TAR}
	${CC} ${LDFLAGS} ${CFLAGS} -o ${TARGET_TAR} ${OBJ_TAR} ${EXTRA}
