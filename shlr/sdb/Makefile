include config.mk
VALADIR=bindings/vala

PWD=$(shell pwd)
PFX=${DESTDIR}${PREFIX}
HGFILES=`find sdb-${SDBVER} -type f | grep -v hg | grep -v swp`
MANDIR=${PFX}/share/man/man1

all: src/sdb-version.h
	${MAKE} -C src
	${MAKE} -C memcache
ifneq (${HAVE_VALA},)
	cd ${VALADIR} && ${MAKE}
	cd ${VALADIR}/types && ${MAKE}
endif

.PHONY: test
test:
	${MAKE} -C test

src/sdb-version.h:
	echo '#define SDB_VERSION "${SDBVER}"' > src/sdb-version.h

EMCCFLAGS=-O2 -s ASM_JS=1
#EMCCFLAGS+=--embed-file sdb.data
sdb.js: src/sdb-version.h
	cd src ; emcc ${EMCCFLAGS} -I. -o ../sdb.js *.c 
#json/api.c json/js0n.c json/json.c json/rangstr.c  

clean:
	rm -f src/sdb-version.h
	cd src && ${MAKE} clean
	cd memcache && ${MAKE} clean
	cd test && ${MAKE} clean
ifneq (${HAVE_VALA},)
	cd ${VALADIR} && ${MAKE} clean
endif

dist:
	rm -f sdb-${SDBVER}.tar.gz
	rm -rf sdb-${SDBVER}
	git clone . sdb-${SDBVER}
	rm -rf sdb-${SDBVER}/.git*
	tar czvf sdb-${SDBVER}.tar.gz sdb-${SDBVER}
	pub sdb-${SDBVER}.tar.gz
	rm -rf sdb-${SDBVER}

install-dirs:
	$(INSTALL_DIR) ${MANDIR} ${PFX}/lib/pkgconfig ${PFX}/bin 
	$(INSTALL_DIR) ${PFX}/share/vala/vapi ${PFX}/include/sdb

INCFILES=src/sdb.h src/sdb-version.h src/cdb.h src/ht.h src/types.h
INCFILES+=src/ls.h src/cdb_make.h src/buffer.h src/config.h

install: install-dirs
	$(INSTALL_MAN) src/sdb.1 ${MANDIR}
	$(INSTALL_LIB) src/libsdb.${SOEXT} ${PFX}/lib
	-if [ "$(SOEXT)" != "$(SOVER)" ]; then \
	cd $(PFX)/lib ; \
	mv libsdb.$(SOEXT) libsdb.$(SOVER) ; \
	ln -s libsdb.$(SOVER) libsdb.$(SOEXT) ; \
fi
	$(INSTALL_DATA) $(INCFILES) ${PFX}/include/sdb
	$(INSTALL_PROGRAM) src/sdb ${PFX}/bin
	$(INSTALL_DATA) memcache/libmcsdb.a ${PFX}/lib
	$(INSTALL_DATA) memcache/mcsdb.h ${PFX}/include/sdb
	$(INSTALL_PROGRAM) memcache/mcsdbd ${PFX}/bin
	$(INSTALL_PROGRAM) memcache/mcsdbc ${PFX}/bin
	$(INSTALL_DATA) ${VALADIR}/sdb.pc ${PFX}/lib/pkgconfig
	$(INSTALL_DATA) ${VALADIR}/mcsdb.pc ${PFX}/lib/pkgconfig
ifneq (${HAVE_VALA},)
	$(INSTALL_DATA) ${VALADIR}/sdb.vapi ${PFX}/share/vala/vapi
	$(INSTALL_DATA) ${VALADIR}/mcsdb.vapi ${PFX}/share/vala/vapi
	cd ${VALADIR}/types && ${MAKE} install PFX=${PFX}
endif

deinstall uninstall:
	rm -rf ${PFX}/include/sdb
	rm -f ${PFX}/bin/sdb
	rm -f ${PFX}/bin/mcsdbc
	rm -f ${PFX}/bin/mcsdbd
	rm -f ${PFX}/lib/libsdb.*
	rm -f ${PFX}/lib/libmcsdb.a
	rm -f ${PFX}/lib/pkgconfig/sdb.pc
	rm -f ${PFX}/lib/pkgconfig/mcsdb.pc
	rm -f ${MANDIR}/sdb.1
ifneq (${HAVE_VALA},)
	rm -f ${PFX}/share/vala/vapi/sdb.vapi 
	rm -f ${PFX}/share/vala/vapi/mcsdb.vapi 
	cd ${VALADIR}/types && ${MAKE} uninstall PFX=${PFX}
endif

symstall: install-dirs
	cd src ; for a in libsdb.* ; do \
		ln -fs ${PWD}/src/$$a ${PFX}/lib/$$a ; done
	ln -fs ${PWD}/src/sdb.1 ${MANDIR}/sdb.1
	ln -fs ${PWD}/src/sdb ${PFX}/bin
	ln -fs ${PWD}/src/sdb.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/sdb-version.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/cdb.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/ht.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/types.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/ls.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/cdb_make.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/buffer.h ${PFX}/include/sdb
	ln -fs ${PWD}/src/config.h ${PFX}/include/sdb
	ln -fs ${PWD}/bindings/vala/sdb.pc ${PFX}/lib/pkgconfig
	ln -fs ${PWD}/bindings/vala/mcsdb.pc ${PFX}/lib/pkgconfig
ifneq (${HAVE_VALA},)
	ln -fs ${PWD}/vala/sdb.vapi ${PFX}/share/vala/vapi
	ln -fs ${PWD}/vala/mcsdb.vapi ${PFX}/share/vala/vapi
	ln -fs ${PWD}/vala/sdb.vapi ${PFX}/share/vala/vapi
	ln -fs ${PWD}/vala/mcsdb.vapi ${PFX}/share/vala/vapi
	cd ${VALADIR}/types && ${MAKE} symstall PFX=${PFX}
endif

# windows compiler prefix
WCP=i386-mingw32

w32: src/sdb-version.h
	cd src ; \
	${MAKE} OS=w32 WCP=${WCP} CC=${WCP}-gcc AR=${WCP}-ar RANLIB=${WCP}-ranlib sdb.exe

.PHONY: all ${VALADIR} clean dist w32
.PHONY: install-dirs install uninstall deinstall symstall
