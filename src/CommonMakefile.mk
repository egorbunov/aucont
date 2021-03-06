# common part for almost every makefile for aucont utilities
# moust be included after specifying `BIN_NAME`

BIN_REL_DIR = ../../bin
BIN_DIR = $(realpath $(BIN_REL_DIR))
CXX = g++
CXX_FLAGS = -std=c++11 -Wall -Werror -Wextra -pedantic-errors -I../libaucont_common/src
LD_FLAGS = -L$(BIN_DIR) -laucont_common -Wl,-rpath,$(BIN_DIR)
SRC_EXT = cpp
SRC_PATH = src
SCRIPTS_PATH = scripts
SCRIPTS_EXT = sh

SOURCES = $(shell find $(SRC_PATH) -name '*.$(SRC_EXT)' -printf '%T@\t%p\n' | sort -k 1nr | cut -f2-)
OBJECTS = $(SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BIN_DIR)/%.o)
SCRIPTS = $(shell if [ -d "$(SCRIPTS_PATH)" ]; then find $(SCRIPTS_PATH) -name '*.$(SCRIPTS_EXT)' -printf '%p\n'; fi)
TARGET_SCRIPTS = $(SCRIPTS:$(SCRIPTS_PATH)/%.$(SCRIPTS_EXT)=$(BIN_DIR)/%.$(SCRIPTS_EXT))

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)/$(BIN_NAME) $(OBJECTS) $(TARGET_SCRIPTS)

.PHONY: all
all: $(BIN_DIR) $(BIN_DIR)/$(BIN_NAME) $(TARGET_SCRIPTS)

$(BIN_DIR):
	mkdir $(BIN_DIR)

$(BIN_DIR)/$(BIN_NAME): $(OBJECTS) 
	$(CXX) $(OBJECTS) $(LD_FLAGS) -o $@

$(BIN_DIR)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	$(CXX) $(CXX_FLAGS) -c $< -o $@

$(BIN_DIR)/%.$(SCRIPTS_EXT): $(SCRIPTS_PATH)/%.$(SCRIPTS_EXT)
	cp $< $@

