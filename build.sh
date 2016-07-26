#! /bin/bash

mkdir -p bin
cd src

cd libaucont_common
pwd
echo ============ Building libaucont_file ============
make clean all
cd ../

for d in aucont_*/; do
	cd "$d"
    echo ============ Building $d ============
	make clean all
	cd ../
done
cd ../

rm bin/*.o

