#!/usr/bin/python
from r_bin import *
import ctypes

a = RBin()
if not a.load("/bin/ls", False):
	print "Fuck. cannot load /bin/ls"
	exit(1)

print ("------")
info = a.get_info ()
print ("type: "+info.type)
print ("arch: "+info.arch)
print ("mach: "+info.machine)
print ("os: "+info.os)
print ("subsys: "+info.subsystem)

print ("------")

o = a.get_object ()
print ("object: "+str(o))

baddr = a.get_baddr()
print ("base address: "+str(baddr))
print ("------")

for s in a.get_sections():
	print (s.name,s.rva)

exit(0)
