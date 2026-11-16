# Linux-side test build for the platform-independent parts of the iOS port.
#   make test         build & run everything
#   make lane         render the end-to-end lane scene to /tmp/lane.ppm
CXX      ?= g++
CXXFLAGS := -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -I/usr/include/libpng16
LIBS     := -lpng
P        := ../Platform
CORE     := $(P)/DxCompat/DxCore.cpp DxSoftRenderer.cpp
PRE      := -include TestPrefix.h

all: test
test: /tmp/dx_tests /tmp/portable_tests
	/tmp/dx_tests && /tmp/portable_tests

/tmp/dx_tests: DxCompatTests.cpp $(CORE) TestUtil.h
	$(CXX) $(CXXFLAGS) DxCompatTests.cpp $(CORE) $(LIBS) -o $@

/tmp/portable_tests: PortableTests.cpp $(CORE) $(P)/SeaText.cpp $(P)/SeaPng.cpp $(P)/SeaTouchRouter.cpp $(P)/SeaKeyMap.cpp ../Seaurchin/Controller.cpp ../Seaurchin/Debug.cpp
	$(CXX) $(CXXFLAGS) $(PRE) -I$(P) $^ $(LIBS) -o $@

lane: /tmp/lane_scene
	/tmp/lane_scene
/tmp/lane_scene: LaneScene.cpp $(CORE) $(P)/SeaPng.cpp
	$(CXX) $(CXXFLAGS) -w LaneScene.cpp $(CORE) $(P)/SeaPng.cpp $(LIBS) -o $@
