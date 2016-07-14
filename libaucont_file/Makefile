LIB_NAME = libaucont_file.so

.PHONY: all
all: bin bin/$(LIB_NAME)

bin/$(LIB_NAME): src/aucont_file.cpp src/aucont_file.h
	g++ -fPIC -shared -std=c++11 -Werror -Wall -pedantic-errors src/aucont_file.cpp -o $@

.PHONY: install
install: bin/$(LIB_NAME)
	sudo cp bin/$(LIB_NAME) /usr/lib

.PHONY: clean
clean: 
	rm -rf bin/*
