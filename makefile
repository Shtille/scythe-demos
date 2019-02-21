# Makefile for scythe-demos

TARGET = demos
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
export TARGET_PATH
export TARGET_EXT
export LDFLAGS

# Main routine
SUBDIRS = \
	atmospheric_scattering \
	shadows \
	ray_trace \
	pbr

all: $(TARGET)

create_dir:
	@test -d $(TARGET_PATH) || mkdir $(TARGET_PATH)

.PHONY: clean
clean:
	@$(foreach directory, $(SUBDIRS), $(MAKE) -C $(directory) clean ;)

.PHONY: install
install: create_dir uninstall
	@$(foreach directory, $(SUBDIRS), $(MAKE) -C $(directory) install ;)

.PHONY: uninstall
uninstall:
	@$(foreach directory, $(SUBDIRS), $(MAKE) -C $(directory) uninstall ;)

.PHONY: help
help:
	@echo available targets: all clean install uninstall

$(TARGET): $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	@$(MAKE) -C $@ $@
