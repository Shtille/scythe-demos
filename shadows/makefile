# Makefile

TARGET = Shadows
TARGET_FILE = $(TARGET)$(TARGET_EXT)

INCLUDE = \
	-I$(ROOT_PATH)/scythe/include \
	-I$(ROOT_PATH)/scythe/src
DEFINES = 

CFLAGS = -g -Wall -O3 -std=c++11
CFLAGS += $(INCLUDE)
CFLAGS += $(DEFINES)

SRC_DIRS = src
SRC_FILES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))

OBJECTS = $(SRC_FILES:.cpp=.o)

LIBRARIES = -lscythe -lstdc++ -lfreetype -ljpeg -lpng -lz
ifeq ($(OS),Windows_NT)
	LIBRARIES += -lgdi32 -lglew -lopengl32
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		# TODO: Linux-specific libraries
	endif
	ifeq ($(UNAME_S),Darwin)
		LIBRARIES += -framework Cocoa -framework OpenGL -framework Foundation
	endif
endif

all: $(SRC_FILES) clean $(TARGET_FILE)
	@echo $(TARGET_FILE) is done!

clean:
	@find src -name "*.o" -type f -delete

$(TARGET_FILE): $(OBJECTS)
	@echo making binary
	@$(CCP) $(LDFLAGS) -o $(TARGET_FILE) $^ $(LIBRARIES) -L$(LIBRARY_PATH)

%.o : %.cpp
	@echo compiling file $<
	@$(CCP) $(CFLAGS) -c $< -o $@