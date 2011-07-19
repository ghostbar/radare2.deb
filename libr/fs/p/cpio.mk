OBJ_CPIO=fs_cpio.o
EXTRA=../p/grub/libgrubfs.a
CFLAGS+=-Igrub/include

STATIC_OBJ+=${OBJ_CPIO}
#STATIC_OBJ+=${EXTRA}
TARGET_CPIO=fs_cpio.${EXT_SO}

ALL_TARGETS+=${TARGET_CPIO}

${TARGET_CPIO}: ${OBJ_CPIO}
	${CC} $(call libname,fs_cpio) ${LDFLAGS} ${CFLAGS} -o ${TARGET_CPIO} ${OBJ_CPIO} ${EXTRA}
