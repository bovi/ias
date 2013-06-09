#!/usr/bin/env bash

rm -Rf libmruby.a
cd mruby_src
./minirake clean
./minirake
cd ..
cp mruby_src/build/Arduino\ Due/lib/libmruby.a .
./make.rb $1
