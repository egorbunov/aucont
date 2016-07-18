BIN_NAME = aucont_start
BIN_DIR = bin
CXX = g++
CXX_FLAGS = -std=c++11 -Wall -Werror -Wextra -pedantic-errors -I../libaucont_file/src
LD_FLAGS = -laucont_file
SRC_EXT = cpp
SRC_PATH = src

SOURCES = $(shell find $(SRC_PATH) -name '*.$(SRC_EXT)' -printf '%T@\t%p\n' \
                                                    | sort -k 1nr | cut -f2-)
OBJECTS = $(SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BIN_DIR)/%.o)

.PHONY: dirs
dirs:
	mkdir $(BIN_DIR)

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)/*    

all: $(BIN_DIR) $(BIN_DIR)/$(BIN_NAME)

$(BIN_DIR):
	mkdir $(BIN_DIR)

$(BIN_DIR)/$(BIN_NAME): $(OBJECTS) 
	$(CXX) $(OBJECTS) $(LD_FLAGS) -o $@

$(BIN_DIR)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	$(CXX) $(CXX_FLAGS) -c $< -o $@
