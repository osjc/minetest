# Makefile for FastMiner

#############################################################################
# USER CONFIGURABLE STUFF
#############################################################################

IRRLICHTPATH = ../irrlicht/irrlicht-1.7.1
JTHREADPATH = ../jthread/jthread-1.2.1

CPPFLAGS = -I$(IRRLICHTPATH)/include -I/usr/X11R6/include \
           -I$(JTHREADPATH)/src

COMMON_FLAGS = -Wall -Werror -pipe

NORMAL_OPTIMIZATIONS = -O0 -g

FAST_OPTIMIZATIONS = -O3 -ffast-math -fomit-frame-pointer \
                     -funroll-loops -mtune=i686 -DUNITTEST_DISABLE

#############################################################################
# SOURCE, LIBRARY AND OUTPUT DESCRIPTION
#############################################################################

TARGET = minerworld

FASTTARGET = minerworld-fast

SOURCE_FILES = \
	voxel.cpp \
	mapblockobject.cpp \
	inventory.cpp \
	debug.cpp \
	serialization.cpp \
	light.cpp \
	filesys.cpp \
	connection.cpp \
	environment.cpp \
	client.cpp \
	server.cpp \
	socket.cpp \
	mapblock.cpp \
	mapsector.cpp \
	heightmap.cpp \
	map.cpp \
	player.cpp \
	utility.cpp \
	main.cpp \
	test.cpp

ifeq ($(HOSTTYPE), x86_64)
LIBSELECT=64
endif

LIBDIRS = \
	-L/usr/X11R6/lib$(LIBSELECT) \
	-L$(IRRLICHTPATH)/lib/Linux \
	-L$(JTHREADPATH)/src/.libs

LIBRARIES = \
	-lIrrlicht \
	-ljthread \
	-lGL \
	-lXxf86vm \
	-lX11 \
	-lXext

#############################################################################
# THE BUILD SYSTEM ITSELF
#############################################################################

#Default target

all: test

# Target specific settings

test: OPTIMIZATIONS = $(NORMAL_OPTIMIZATIONS)
fasttest: OPTIMIZATIONS = $(FAST_OPTIMIZATIONS)

# Name of the binary

DESTPATH = bin/$(TARGET)$(SUF)
FASTDESTPATH = bin/$(FASTTARGET)$(SUF)

# Build commands

SOURCES = $(addprefix src/, $(SOURCE_FILES))
OBJECTS = $(SOURCES:.cpp=.o)

COMPILEOPTS = $(OPTIMIZATIONS) $(COMMON_FLAGS) $(CPPFLAGS)

LINKOPTS = $(OBJECTS) $(LDFLAGS) $(LIBDIRS) $(LIBRARIES)

test: $(DESTPATH)

fasttest: $(FASTDESTPATH)

$(FASTDESTPATH): $(OBJECTS)
	mkdir -p bin
	$(CXX) -o $@ $(LINKOPTS)

$(DESTPATH): $(OBJECTS)
	mkdir -p bin
	$(CXX) -o $@ $(LINKOPTS)

.cpp.o:
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(COMPILEOPTS)

clean:
	@$(RM) $(OBJECTS) $(DESTPATH) $(FASTDESTPATH)

.PHONY: all makedir test fasttest clean
