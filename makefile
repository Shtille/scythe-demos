# Makefile for scythe-demos

ROOT_PATH = ../..
LIBRARY_PATH = -L$(ROOT_PATH)/scythe/lib
TARGET_PATH = bin

# Platform-specific defines
ifeq ($(OS),Windows_NT)
    # Windows
    MAKE := mingw32-make.exe
    LDFLAGS = -s -mwindows
    CCP = g++
    TARGET_EXT = .exe
else
	MAKE := make
	LDFLAGS =
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        # Linux
        CCP = g++
        TARGET_EXT = .app
    endif
    ifeq ($(UNAME_S),Darwin)
        # Mac OS X
        CCP = clang++
        TARGET_EXT = .app
    endif
endif

# Exports
export ROOT_PATH
export LIBRARY_PATH
export TARGET_EXT
export CCP
export LDFLAGS

# Main routine
SUBDIRS = \
	demos/AtmosphericScattering

all: $(SUBDIRS)

create_dir:
	@test -d $(TARGET_PATH) || mkdir $(TARGET_PATH)

install: create_dir
	@find demos -name "*.app" -type f -exec mv {} $(TARGET_PATH) \;

$(SUBDIRS):
	@echo Get down to $@
	@$(MAKE) -C $@

.PHONY: $(SUBDIRS)
