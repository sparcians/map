#!/bin/sh

for f in `find . -name CMakeLists.txt`; do
    awk '
    /This file/ { next; }
    /target_link_/ { print "include(../TestingMacros.cmake)"; next; }
    /sparta_test/ { gsub("sparta","sparta"); }
    { print; }
' $f > $f.back;
    mv $f.back $f
done
