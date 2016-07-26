LIB_NAME = libaucont_common.so
BIN_REL_DIR = ../../bin
BIN_DIR = $(realpath $(BIN_REL_DIR))

.PHONY: all
all: $(BIN_DIR) $(BIN_DIR)/$(LIB_NAME)

$(BIN_DIR):
	mkdir $(BIN_DIR)

$(BIN_DIR)/$(LIB_NAME): src/aucont_common.cpp src/aucont_common.h
	g++ -fPIC -shared -std=c++11 -Werror -Wall -pedantic-errors src/aucont_common.cpp -o $@

# .PHONY: install
# install: $(BIN_DIR)/$(LIB_NAME)
# 	sudo cp $(BIN_DIR)/$(LIB_NAME) /usr/lib

.PHONY: clean
clean: 
	rm -rf $(BIN_DIR)/$(LIB_NAME)
