-include config-user.mk
include global.mk

STRIP?=strip
ifneq ($(shell bsdtar -h 2>/dev/null|grep bsdtar),)
TAR=bsdtar czvf
else
TAR=tar -czvf
endif
PWD=$(shell pwd)
REMOTE=radare.org:/srv/http/radareorg/get/beta

all: plugins.cfg
	${MAKE} libr
	${MAKE} binr

plugins.cfg:
	@if [ ! -e config-user.mk ]; then echo ; \
	echo "  Please, run ./configure first" ; echo ; exit 1 ; fi
	./configure-plugins

farm:
	./sys/farm/run.sh

libr:
	cd libr && ${MAKE} all

binr:
	cd binr && ${MAKE} all

w32:
	make clean
	# TODO: add support for debian
	./configure --without-ssl --without-gmp --with-compiler=i486-mingw32-gcc --with-ostype=windows --host=i486-unknown-windows
	make

.PHONY: depgraph.png
depgraph.png:
	cd libr ; perl depgraph.pl | dot -Tpng -odepgraph.png

android:
	@if [ -z "$(NDK_ARCH)" ]; then echo "Set NDK_ARCH=[arm|mips|x86]" ; false; fi
	sys/android-${NDK_ARCH}.sh

w32dist:
	rm -rf radare2-w32-${VERSION} w32dist
	mkdir w32dist
	for a in `find libr | grep -e dll$$`; do cp $$a w32dist ; done
	for a in `find binr | grep -e exe$$`; do cp $$a w32dist ; done
	rm w32dist/plugin.dll
	mkdir -p w32dist/radare2/${VERSION}/magic
	cp -f libr/magic/d/default/* w32dist/radare2/${VERSION}/magic
	mkdir -p w32dist/radare2/${VERSION}/syscall
	cp -f libr/syscall/d/*.sdb w32dist/radare2/${VERSION}/syscall
	mkdir -p w32dist/radare2/${VERSION}/opcodes
	cp -f libr/asm/d/*.sdb w32dist/radare2/${VERSION}/opcodes
	mkdir -p w32dist/share/doc/radare2
	mkdir -p w32dist/include/libr
	cp libr/include/*.h w32dist/include/libr
	#mkdir -p w32dist/include/libr/sflib
	cp -f doc/fortunes w32dist/share/doc/radare2
	mv w32dist radare2-w32-${VERSION}
	rm -f radare2-w32-${VERSION}.zip 
	zip -r radare2-w32-${VERSION}.zip radare2-w32-${VERSION}

w32beta: w32dist
	scp radare2-w32-${VERSION}.zip ${REMOTE}
	cd r2-bindings ; $(MAKE) w32dist
	scp radare2-bindings-w32-${VERSION}.zip ${REMOTE}

clean:
	for a in libr binr shlr ; do (cd $$a ; ${MAKE} clean) ; done

mrproper:
	for a in libr binr shlr ; do ( cd $$a ; ${MAKE} mrproper) ; done
	rm -f config-user.mk plugins.cfg libr/config.h
	rm -f libr/include/r_userconf.h libr/config.mk
	rm -f pkgcfg/*.pc

pkgcfg:
	cd libr && ${MAKE} pkgcfg

install-man:
	mkdir -p ${MDR}/man1
	for a in man/*.1 ; do ${INSTALL_MAN} $$a ${MDR}/man1 ; done
	cd ${MDR}/man1 && ln -fs radare2.1 r2.1

install-man-symlink:
	mkdir -p ${MDR}/man1
	cd man && for a in *.1 ; do ln -fs ${PWD}/man/$$a ${MDR}/man1/$$a ; done
	cd ${MDR}/man1 && ln -fs radare2.1 r2.1

install-doc:
	${INSTALL_DIR} ${PFX}/share/doc/radare2
	for a in doc/* ; do ${INSTALL_DATA} $$a ${PFX}/share/doc/radare2 ; done

install-doc-symlink:
	${INSTALL_DIR} ${PFX}/share/doc/radare2
	cd doc ; for a in * ; do ln -fs ${PWD}/doc/$$a ${PFX}/share/doc/radare2 ; done

DATADIRS=libr/asm/d libr/syscall/d libr/magic/d
#binr/ragg2/d
install: install-doc install-man install-www
	cd libr && ${MAKE} install PARENT=1 PREFIX=${PREFIX} DESTDIR=${DESTDIR}
	cd binr && ${MAKE} install PREFIX=${PREFIX} DESTDIR=${DESTDIR}
	for a in ${DATADIRS} ; do \
	(cd $$a ; ${MAKE} install LIBDIR=${LIBDIR} PREFIX=${PREFIX} DESTDIR=${DESTDIR} ); \
	done
	mkdir -p ${DESTDIR}/${LIBDIR}/radare2/${VERSION}/hud
	cp -f libr/core/hud/main ${DESTDIR}/${LIBDIR}/radare2/${VERSION}/hud/

install-www:
	rm -rf ${DESTDIR}/${WWWROOT}
	mkdir -p ${DESTDIR}/${WWWROOT}
	cp -rf shlr/www/* ${DESTDIR}/${WWWROOT}

symstall-www:
	rm -rf ${DESTDIR}/${WWWROOT}
	mkdir -p ${DESTDIR}/${WWWROOT}
	cd ${DESTDIR}/${WWWROOT} ; for a in ${PWD}/shlr/www/* ; do \
		ln -fs $$a ${DLIBDIR}/radare2/${VERSION}/www ; done

DLIBDIR=$(DESTDIR)/$(LIBDIR)

install-pkgconfig-symlink:
	@${INSTALL_DIR} ${DLIBDIR}/pkgconfig
	cd pkgcfg ; for a in *.pc ; do ln -fs $${PWD}/$$a ${DLIBDIR}/pkgconfig/$$a ; done

symstall install-symlink: install-man-symlink install-doc-symlink install-pkgconfig-symlink symstall-www
	cd libr && ${MAKE} install-symlink PREFIX=${PREFIX} DESTDIR=${DESTDIR}
	cd binr && ${MAKE} install-symlink PREFIX=${PREFIX} DESTDIR=${DESTDIR}
	for a in ${DATADIRS} ; do (\
		cd $$a ; \
		echo $$a ; \
		${MAKE} install-symlink LIBDIR=${LIBDIR} PREFIX=${PREFIX} DESTDIR=${DESTDIR} ); \
	done
	mkdir -p ${DLIBDIR}/radare2/${VERSION}/hud
	ln -fs ${PWD}/libr/core/hud/main ${DLIBDIR}/radare2/${VERSION}/hud/main

deinstall uninstall:
	cd libr && ${MAKE} uninstall PARENT=1 PREFIX=${PREFIX} DESTDIR=${DESTDIR}
	cd binr && ${MAKE} uninstall PARENT=1 PREFIX=${PREFIX} DESTDIR=${DESTDIR}
	cd libr/syscall/d && ${MAKE} uninstall PARENT=1 PREFIX=${PREFIX} DESTDIR=${DESTDIR} LIBDIR=${LIBDIR}
	@echo
	@echo "Run 'make purge' to also remove installed files from previous versions of r2"
	@echo

purge-doc:
	rm -rf ${DESTDIR}/${PREFIX}/share/doc/radare2
	cd man ; for a in *.1 ; do rm -f ${MDR}/man1/$$a ; done
	rm -f ${MDR}/man1/r2.1

R2BINS=$(shell cd binr ; echo r*2)
purge-dev:
	rm -rf ${DESTDIR}/${LIBDIR}/libr_*.a
	rm -rf ${DESTDIR}/${LIBDIR}/pkgconfig/r_*.pc
	rm -rf ${DESTDIR}/${INCLUDEDIR}/libr
	rm -f ${DESTDIR}/${LIBDIR}/radare2/${VERSION}/-*
	# XXX: this must be in purge-sym ?
	-for a in ${R2BINS} ; do ${STRIP} -s ${DESTDIR}/${BINDIR}/$$a 2> /dev/null ; done
	-for a in ${DESTDIR}/${LIBDIR}/libr_*.so ; do ${STRIP} -s $$a ; done

purge: purge-doc purge-dev
	for a in ${R2BINS} ; do rm -f ${DESTDIR}/${BINDIR}/$$a ; done
	rm -f ${DESTDIR}/${BINDIR}/ragg2-cc
	rm -f ${DESTDIR}/${LIBDIR}/libr_*
	rm -rf ${DESTDIR}/${LIBDIR}/radare2
	rm -rf ${DESTDIR}/${INCLUDEDIR}/libr

beta: dist r2-bindings-dist
	scp ../radare2-${VERSION}.tar.gz ${REMOTE}
	scp r2-bindings-${VERSION}.tar.gz ${REMOTE}

version:
	@echo ${VERSION}

r2-bindings-dist:
	cd r2-bindings && ${MAKE} dist

dist:
	git log $$(git show-ref `git tag |tail -n1`)..HEAD > ChangeLog
	DIR=`basename $$PWD` ; \
	FILES=`git ls-files | sed -e s,^,radare2-${VERSION}/,` ; \
	cd .. && mv $${DIR} radare2-${VERSION} && \
	${TAR} radare2-${VERSION}.tar.gz $${FILES} radare2-${VERSION}/ChangeLog ;\
	mv radare2-${VERSION} $${DIR}

shot:
	DATE=`date '+%Y%m%d'` ; \
	FILES=`git ls-files | sed -e s,^,radare2-${DATE}/,` ; \
	cd .. && mv radare2 radare2-$${DATE} && \
	${TAR} radare2-$${DATE}.tar.gz $${FILES} ;\
	mv radare2-$${DATE} radare2 && \
	scp radare2-$${DATE}.tar.gz radare.org:/srv/http/radareorg/get/shot

tests:
	@if [ -d r2-regressions ]; then \
		cd r2-regressions ; git pull ; \
	else \
		git clone git://github.com/vext01/r2-regressions.git ; \
	fi
	cd r2-regressions ; ${MAKE}

include ${MKPLUGINS}

.PHONY: all clean mrproper install symstall uninstall deinstall dist shot pkgcfg
.PHONY: r2-bindings r2-bindings-dist libr binr install-man version w32dist tests
