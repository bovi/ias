Interactive Arduino Shell
===

This is a very early version of an interactive Ruby shell for the Arduino Due.

see http://blog.mruby.sh/201305201003.html

The code is very unstable and quite a mess at the moment. Usage is only suggested to the brave!

I also added ias.cpp.bin to the repository. This is the ARM binary which includes libmruby
and the IAS code already cross-compiled.

You can take the ias.cpp.bin and flash it by yourself:

bossac --port=#{USB_PORT} -U false -e -w -v -b ias.cpp.bin -R
