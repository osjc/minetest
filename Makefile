# Makefile for Irrlicht Examples
# It's usually sufficient to change just the target name and source file list
# and be sure that CXX is set to a valid compiler
TARGET = minerworld
SOURCE_FILES = voxel.cpp mapblockobject.cpp inventory.cpp debug.cpp serialization.cpp light.cpp filesys.cpp connection.cpp environment.cpp client.cpp server.cpp socket.cpp mapblock.cpp mapsector.cpp heightmap.cpp map.cpp player.cpp utility.cpp main.cpp test.cpp
SOURCES = $(addprefix src/, $(SOURCE_FILES))
OBJECTS = $(SOURCES:.cpp=.o)
FASTTARGET = minerworld-fast

IRRLICHTPATH = ../irrlicht/irrlicht-1.7.1
JTHREADPATH = ../jthread/jthread-1.2.1

CPPFLAGS = -I$(IRRLICHTPATH)/include -I/usr/X11R6/include -I$(JTHREADPATH)/src

#CXXFLAGS = -O2 -ffast-math -Wall -fomit-frame-pointer -pipe
CXXFLAGS = -O2 -ffast-math -Wall -g -pipe
#CXXFLAGS = -O1 -ffast-math -Wall -g
#CXXFLAGS = -Wall -g -O0

#CXXFLAGS = -O3 -ffast-math -Wall
#CXXFLAGS = -O3 -ffast-math -Wall -g
#CXXFLAGS = -O2 -ffast-math -Wall -g

FASTCXXFLAGS = -O3 -ffast-math -Wall -fomit-frame-pointer -pipe -funroll-loops -mtune=i686
#FASTCXXFLAGS = -O3 -ffast-math -Wall -fomit-frame-pointer -pipe -funroll-loops -mtune=i686 -fwhole-program

#Default target

all: test

ifeq ($(HOSTTYPE), x86_64)
LIBSELECT=64
endif

# Target specific settings

test fasttest: LDFLAGS = -L/usr/X11R6/lib$(LIBSELECT) -L$(IRRLICHTPATH)/lib/Linux -L$(JTHREADPATH)/src/.libs -lIrrlicht -lGL -lXxf86vm -lXext -lX11 -ljthread

# Name of the binary

DESTPATH = bin/$(TARGET)$(SUF)
FASTDESTPATH = bin/$(FASTTARGET)$(SUF)

# Build commands

test: $(DESTPATH)

fasttest: $(FASTDESTPATH)

$(FASTDESTPATH): $(SOURCES)
	$(CXX) -o $(FASTDESTPATH) $(SOURCES) $(CPPFLAGS) $(FASTCXXFLAGS) $(LDFLAGS) -DUNITTEST_DISABLE
	@# Errno doesn't work ("error: ‘__errno_location’ was not declared in this scope")
	@#cat $(SOURCES) | $(CXX) -o $(FASTDESTPATH) -x c++ - -Isrc/ $(CPPFLAGS) $(FASTCXXFLAGS) $(LDFLAGS) -DUNITTEST_DISABLE -DDISABLE_ERRNO

$(DESTPATH): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS)

.cpp.o:
	$(CXX) -c -o $@ $< $(CPPFLAGS) $(CXXFLAGS)

clean:
	@$(RM) $(OBJECTS) $(DESTPATH) $(FASTDESTPATH)

.PHONY: all test fasttest clean
