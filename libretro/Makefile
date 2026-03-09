TARGET_NAME := geranes
CORE_SUFFIX := _libretro

CXX ?= g++
CC ?= gcc
AR ?= ar

SRC_DIR := ../src
CORE_DIR := $(SRC_DIR)/libretro

SOURCES_CXX := \
	$(CORE_DIR)/GeraNESLibretro.cpp \
	$(SRC_DIR)/signal/signal.cpp

SOURCES_C := \
	$(SRC_DIR)/zip/zip.c

OBJECTS := $(SOURCES_CXX:.cpp=.o) $(SOURCES_C:.c=.o)

INCLUDES := -I.. -I$(SRC_DIR)
DEFINES := -DGERANES_DISABLE_PATCHES

CXXFLAGS += -std=gnu++20 -fPIC $(INCLUDES) $(DEFINES)
CFLAGS += -std=gnu11 -fPIC $(INCLUDES) $(DEFINES)
LDFLAGS +=
LIBS +=

EXT := so
SHARED := -shared

ifeq ($(platform), osx)
	EXT := dylib
	SHARED := -dynamiclib
	LDFLAGS += -Wl,-dead_strip
endif

ifneq (,$(findstring ios,$(platform)))
	EXT := dylib
	SHARED := -dynamiclib
endif

ifneq (,$(findstring win,$(platform)))
	EXT := dll
	SHARED := -shared
	LDFLAGS += -Wl,--no-undefined
endif

ifneq (,$(findstring mingw,$(platform)))
	EXT := dll
	SHARED := -shared
	LDFLAGS += -Wl,--no-undefined
endif

ifeq ($(platform), emscripten)
	EXT := bc
	SHARED :=
endif

TARGET := $(TARGET_NAME)$(CORE_SUFFIX).$(EXT)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(SHARED) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
