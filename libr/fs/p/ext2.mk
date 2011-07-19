OBJ_EXT2=fs_ext2.o
EXTRA=../p/grub/libgrubfs.a
CFLAGS+=-Igrub/include

STATIC_OBJ+=${OBJ_EXT2}
#STATIC_OBJ+=${EXTRA}
TARGET_EXT2=fs_ext2.${EXT_SO}

ALL_TARGETS+=${TARGET_EXT2}

${TARGET_EXT2}: ${OBJ_EXT2}
	${CC} $(call libname,fs_ext2) ${LDFLAGS} ${CFLAGS} -o ${TARGET_EXT2} ${OBJ_EXT2} ${EXTRA}
