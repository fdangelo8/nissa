#!/bin/bash

mkdir comp
cd comp
wget http://valgrind.org/downloads/valgrind-3.11.0.tar.bz2 -O -|tar jxf -
cd val*

./configure --prefix=$HOME
make -j 4
make install
cd ..
rm -fr val*
cd ..
rm -fr comp

echo "add \"export VALGRIND_LIB=\"$HOME/lib/valgrind\"\" to your .bashrc"
echo "Use: LD_PRELOAD=$(ls $HOME/lib/valgrind/libmpiwrap-*.so) mpirun -np np $HOME/bin/valgrind --track-origins=yes program"
