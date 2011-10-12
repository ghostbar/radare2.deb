#!/bin/sh

cd `dirname $PWD/$0` ; cd ..

case "$1" in
"arm")
	NDK_ARCH=arm
	STATIC_BUILD=0
	;;
"x86")
	NDK_ARCH=x86
	STATIC_BUILD=0
	;;
arm-static|static-arm)
	NDK_ARCH=arm
	STATIC_BUILD=1
	;;
x86-static|static-x86)
	NDK_ARCH=x86
	STATIC_BUILD=1
	;;
"-h")
	echo "Usage: android-build.sh [arm|x86][-static]"
	exit 1
	;;
esac

[ -z "${NDK_ARCH}" ] && NDK_ARCH=arm
[ -z "${STATIC_BUILD}" ] && STATIC_BUILD=0
export NDK_ARCH
export STATIC_BUILD

echo "Using NDK_ARCH: ${NDK_ARCH}"
echo "Using STATIC_BUILD: ${STATIC_BUILD}"
sleep 2

make mrproper
if [ $STATIC_BUILD = 1 ]; then
	CFGFLAGS="--without-pic --with-nonpic"
fi
./configure --with-compiler=android --with-ostype=android \
	--without-ssl --prefix=/data/radare2 ${CFGFLAGS} || exit 1
make -j 4 || exit 1
PKG=`./configure --version|head -n1 |cut -d ' ' -f 1`
D=${PKG}-android-${NDK_ARCH}
rm -rf $D
mkdir -p $D

INSTALL_PROGRAM=`grep INSTALL_DATA config-user.mk|cut -d = -f 2`

make install INSTALL_PROGRAM="${INSTALL_PROGRAM}" DESTDIR=$PWD/$D || exit 1
# TODO: remove unused files like include files and so on
cd $D
tar czvf ../$D.tar.gz *
echo "${D}.tar.gz"
