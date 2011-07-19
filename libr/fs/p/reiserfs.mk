OBJ_REISERFS=fs_reiserfs.o
EXTRA=../p/grub/libgrubfs.a
CFLAGS+=-Igrub/include

STATIC_OBJ+=${OBJ_REISERFS}
#STATIC_OBJ+=${EXTRA}
TARGET_REISERFS=fs_reiserfs.${EXT_SO}

ALL_TARGETS+=${TARGET_REISERFS}

${TARGET_REISERFS}: ${OBJ_REISERFS}
	${CC} $(call libname,fs_reiserfs) ${LDFLAGS} ${CFLAGS} -o ${TARGET_REISERFS} ${OBJ_REISERFS} ${EXTRA}
