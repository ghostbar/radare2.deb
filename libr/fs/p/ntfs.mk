OBJ_NTFS=fs_ntfs.o
EXTRA=../p/grub/libgrubfs.a
CFLAGS+=-Igrub/include

STATIC_OBJ+=${OBJ_NTFS}
#STATIC_OBJ+=${EXTRA}
TARGET_NTFS=fs_ntfs.${EXT_SO}

ALL_TARGETS+=${TARGET_NTFS}

${TARGET_NTFS}: ${OBJ_NTFS}
	${CC} $(call libname,fs_ntfs) ${LDFLAGS} ${CFLAGS} -o ${TARGET_NTFS} ${OBJ_NTFS} ${EXTRA}
