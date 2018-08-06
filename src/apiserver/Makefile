appname := apiserver

CXX := ~/tools/host/bin/arm-linux-g++
CXXFLAGS := -std=c++14 -Wno-psabi -s -Os
LDLIBS := -lcrypt -lpthread -lboost_system

# srcfiles := $(shell find src -name "*.cpp")
srcfiles := src/main.cpp src/CgMinerMain.cpp src/CrowMain.cpp src/utils/CgMinerUtils.cpp src/utils/utils.cpp src/utils/base64.cpp src/handlers/Handlers.cpp
objects  := $(patsubst %.cpp, %.o, $(srcfiles))

all: $(appname)

$(appname): $(objects)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o bin/$(appname) $(objects) $(LDLIBS)

depend: .depend

.depend: $(srcfiles)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;

clean:
	rm -f bin/$(appname)

dist-clean: clean
	rm -f *~ .depend

include .depend
