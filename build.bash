#! /bin/bash

cd libaucont_file
pwd
echo ===========================================
sudo make clean install
cd ../

for d in aucont_*/; do
	cd "$d"
    pwd
    echo ===========================================
	make clean all
	cd ../
done
