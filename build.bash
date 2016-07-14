#! /bin/bash

cd libaucont_file
sudo make clean install
cd ../

for d in aucont_*/; do
	echo "Building $d"
	cd "$d"
	make clean all
	cd ../
done

