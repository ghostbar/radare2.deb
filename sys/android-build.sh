#!/bin/sh

PREFIX="/data/data/org.radare.installer/radare2"
if [ -z "${NDK}" ]; then
	echo "use ./android-{arm|mips|x86}.sh"
	exit 1
fi

cd `dirname $PWD/$0` ; cd ..

case "$1" in
"mips")
	NDK_ARCH=mips
	STATIC_BUILD=0
	STRIP=mips-linux-android-strip
	;;
"arm")
	NDK_ARCH=arm
	STATIC_BUILD=0
	STRIP=arm-eabi-strip
	;;
"x86")
	NDK_ARCH=x86
	STATIC_BUILD=0
	STRIP=strip
	;;
arm-static|static-arm)
	NDK_ARCH=arm
	STATIC_BUILD=1
	;;
x86-static|static-x86)
	NDK_ARCH=x86
	STATIC_BUILD=1
	;;
mips-static|static-mips)
	NDK_ARCH=mips
	# XXX: by default we should build all libs as .a but link binary dinamically
	STATIC_BUILD=1
	STRIP=mips-linux-android-strip
	;;
""|"-h")
	echo "Usage: android-build.sh [arm|x86|mips][-static]"
	exit 1
	;;
*)
	echo "Unknown argument"
	exit 1
	;;
esac

[ -z "${NDK_ARCH}" ] && NDK_ARCH=arm
[ -z "${STATIC_BUILD}" ] && STATIC_BUILD=0

# ow yeah
STATIC_BUILD=1
export NDK_ARCH
export STATIC_BUILD
PKG=`./configure --version|head -n1 |cut -d ' ' -f 1`
D=${PKG}-android-${NDK_ARCH}
echo NDK_ARCH: ${NDK_ARCH}

echo "Using NDK_ARCH: ${NDK_ARCH}"
echo "Using STATIC_BUILD: ${STATIC_BUILD}"

# start build
sleep 2

make mrproper
if [ $STATIC_BUILD = 1 ]; then
	CFGFLAGS="--without-pic --with-nonpic"
fi
# dup
echo ./configure --with-compiler=android --with-ostype=android \
	--without-ewf --without-ssl --prefix=${PREFIX} ${CFGFLAGS}

./configure --with-compiler=android --with-ostype=android \
	--without-ewf --without-ssl --prefix=${PREFIX} ${CFGFLAGS} || exit 1
make -s -j 4 || exit 1
rm -rf $D
mkdir -p $D

INSTALL_PROGRAM=`grep INSTALL_DATA config-user.mk|cut -d = -f 2`

make install INSTALL_PROGRAM="${INSTALL_PROGRAM}" DESTDIR=$PWD/$D || exit 1

make purge-dev DESTDIR=${PWD}/${D} STRIP="${STRIP}"
make purge-doc DESTDIR=${PWD}/${D} STRIP="${STRIP}"
rm -rf ${PWD}/${D}/share
rm -rf ${PWD}/${D}/include
rm -rf ${PWD}/${D}/lib/pkgconfig
rm -rf ${PWD}/${D}/lib/libsdb.a

echo rm -rf ${PWD}/${D}/${PREFIX}/bin/*
rm -rf ${PWD}/${D}/${PREFIX}/bin/*

#end build

# use busybox style symlinkz
HERE=${PWD}
cd binr/blob
make STATIC_BUILD=1
make install PREFIX="${PREFIX}" DESTDIR="${HERE}/${D}"
cd ../..

chmod +x ${PWD}/${D}/${PREFIX}/bin/*

# TODO: remove unused files like include files and so on
rm -f ${PWD}/${D}/${PREFIX}/lib/radare2/*/*.so
rm -f ${PWD}/${D}/${PREFIX}/lib/*.a
rm -rf ${PWD}/${D}/${PREFIX}/include
rm -rf ${PWD}/${D}/${PREFIX}/share
rm -rf ${PWD}/${D}/${PREFIX}/doc
eval `grep ^VERSION= ${PWD}/config-user.mk`
WWWROOT="/data/data/org.radare.installer/radare2/lib/radare2/${VERSION}/www"
ln -fs /data/data/org.radare.installer/radare2/${WWWROOT} \
	/data/data/org.radare.installer/www
cd $D
tar czvf ../$D.tar.gz *
cd ..
D2=`git log HEAD 2>/dev/null|head -n1|awk '{print $2}'|cut -c 1-8`
if [ -n "$D2" ]; then
	ln -fs $D.tar.gz "${D}${D2}".tar.gz
fi
echo `pwd`"/${D}.tar.gz"
echo `pwd`"/${D}${D2}.tar.gz"
