#!/usr/bin/env bash

rm -Rf libmruby.a
cd mruby
./minirake clean
./minirake
cd ..
cp mruby/build/Arduino\ Due/lib/libmruby.a .
./make.rb
