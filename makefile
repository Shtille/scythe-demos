# Makefile for scythe-demos

TARGET = demos
ROOT_PATH = ../..
BINARY_PATH := bin
TARGET_PATH = ../$(BINARY_PATH)

# Platform-specific defines
ifeq ($(OS),Windows_NT)
	# Windows
	LDFLAGS = -s -mwindows
	TARGET_EXT = .exe
else
	LDFLAGS =
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		# Linux
		TARGET_EXT = .app
	endif
	ifeq ($(UNAME_S),Darwin)
		# Mac OS X
		TARGET_EXT = .app
	endif
endif

# Exports
export ROOT_PATH
export TARGET_PATH
export TARGET_EXT
export LDFLAGS

# Main routine
SUBDIRS = \
	atmospheric_scattering \
	shadows \
	ray_trace \
	pbr

ifeq ($(OS),Windows_NT)
	CREATE_DIR = if not exist $(BINARY_PATH) mkdir $(BINARY_PATH)
else
	CREATE_DIR = test -d $(BINARY_PATH) || mkdir -p $(BINARY_PATH)
endif

all: $(TARGET)

create_dir:
	@$(CREATE_DIR)

.PHONY: clean
clean:
	@$(foreach directory, $(SUBDIRS), $(MAKE) -C $(directory) clean ;)

.PHONY: help
help:
	@echo available targets: all clean

$(TARGET): create_dir $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	@$(MAKE) -C $@ $@
