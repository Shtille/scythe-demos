# Makefile for scythe-demos

ROOT_PATH = ../..
LIBRARY_PATH = $(ROOT_PATH)/lib
TARGET_PATH = bin

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
export LIBRARY_PATH
export TARGET_EXT
export LDFLAGS

# Main routine
SUBDIRS = \
	atmospheric_scattering

all: $(SUBDIRS) install

create_dir:
	@test -d $(TARGET_PATH) || mkdir $(TARGET_PATH)

install: create_dir
	@find $(TARGET_PATH) -name "*$(TARGET_EXT)" -type f -delete
	@find . -name "*$(TARGET_EXT)" -type f -exec mv {} $(TARGET_PATH) \;

$(SUBDIRS):
	@echo Get down to $@
	@$(MAKE) -C $@

.PHONY: $(SUBDIRS)
