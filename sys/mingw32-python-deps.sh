#!/bin/sh
cd `dirname $PWD/$0`
cd _work
wget -c http://www.python.org/ftp/python/2.7.2/python-2.7.2.msi
msiexec /i python-2.7.2.msi 
