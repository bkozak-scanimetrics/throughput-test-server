# -*- makefile -*-

# Add .d to Make's recognized suffixes.
SUFFIXES += .d

#compiler and linker
CC=gcc
LD=$(CC)

#directories
OBJ_PATH=./obj
BIN_PATH=./bin
INCLUDE_PATH=./include
SRC_PATH=./src
DEP_PATH=./deps

BINARY= $(BIN_PATH)/testServer

INCLUDES = -I$(INCLUDE_PATH)

PLATFORM = TS

ifeq ($(PLATFORM), PC)
DEFINES += -DPLATFORM_PC
endif

ifeq ($(PLATFORM), TS)
DEFINES += -DPLATFORM_TS
endif

DEFINES += -D_GNU_SOURCE

CFLAGS += -std=gnu99
CFLAGS += -Wall
CFLAGS += -Os
CFLAGS += $(DEFINES)
CFLAGS += $(INCLUDES)
CFLAGS += -c

#don't generate dependencies if running these targets
NODEPS:= clean commit

LINKER_FLAGS += -Os
LINKER_FLAGS += -pthread

SRCS += $(wildcard ./$(SRC_PATH)/*.c)
HEADERS += $(wildcard ./$(INCLUDE_PATH)/*.h)
DEP_FILES += $(patsubst %c,$(DEP_PATH)/%d,$(notdir $(SRCS)))
OBJECTS += $(patsubst %c,$(OBJ_PATH)/%o,$(notdir $(SRCS)))

all: directories $(BINARY)

directories:
	@"mkdir" -p $(DEP_PATH)
	@"mkdir" -p $(BIN_PATH)
	@"mkdir" -p $(OBJ_PATH)

$(DEP_FILES): $(DEP_PATH)/%.d: $(SRC_PATH)/%.c
	$(CC) $(CFLAGS) -MM -MT \
	$(patsubst %c,$(OBJ_PATH)/%o,$(notdir $<)) $< > $@

ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
-include $(DEP_FILES)
endif

#see static pattern rules of gnu make for explanation
$(OBJECTS): $(OBJ_PATH)/%.o: $(SRC_PATH)/%.c $(DEP_PATH)/%.d
	$(CC) $(CFLAGS) $< -o $@

$(BINARY): $(OBJECTS)
	$(CC) $(LINKER_FLAGS) $(OBJECTS) -o $@

clean:
	rm -rf $(OBJ_PATH)/* $(BIN_PATH)/* $(DEP_PATH)/*
	rm -f $(SRC_PATH)/*~ $(INCLUDE_PATH)/*~
	rm -f $(SRC_PATH)/*# $(INCLUDE_PATH)/*#

commit: clean
	@"svn" add $(SRC_PATH)/* $(INCLUDE_PATH)/* 2> /dev/null
	svn commit
