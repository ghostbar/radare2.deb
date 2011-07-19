OBJ_UFS2=fs_ufs2.o
EXTRA=../p/grub/libgrubfs.a
CFLAGS+=-Igrub/include

STATIC_OBJ+=${OBJ_UFS2}
#STATIC_OBJ+=${EXTRA}
TARGET_UFS2=fs_ufs2.${EXT_SO}

ALL_TARGETS+=${TARGET_UFS2}

${TARGET_UFS2}: ${OBJ_UFS2}
	${CC} $(call libname,fs_ufs2) ${LDFLAGS} ${CFLAGS} -o ${TARGET_UFS2} ${OBJ_UFS2} ${EXTRA}
