CXX = g++
CXXFLAGS = -fopenmp -O3 -g #-fno-reorder-blocks-and-partition

LDFLAGS = -shared
LIBS = -lm

CURR_DIR := $(shell pwd)

INCLUDES = -I$(CURR_DIR) \
           -I$(CURR_DIR)/deps/gbwtgraph/include \
           -I$(CURR_DIR)/deps/gbwt/include \
           -I$(CURR_DIR)/deps/sdsl-lite/include \
           -I$(CURR_DIR)/deps/libhandlegraph/build/usr/local/include

LIBS = -L$(CURR_DIR)/deps/sdsl-lite/lib \
       -L$(CURR_DIR)/deps/gbwt/lib \
       -L$(CURR_DIR)/deps/libhandlegraph/build/usr/local/lib \
       -L$(CURR_DIR)/deps/gbwtgraph/lib

LDLIBS = -lgbwtgraph -lgbwt -lhandlegraph -lsdsl -ldivsufsort -ldivsufsort64

time-utils.so: time-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

perf-utils.so: perf-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

miniGiraffe: miniGiraffe.cpp time-utils.so perf-utils.so
	$(CXX) -fopenmp -pthread -O3 -g $(INCLUDES) $(LIBS) $^ $(LDLIBS) -Wl,--emit-relocs -o $@

main-new: main-new.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

check-input: check-input.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

exp-gbwt: exploring-gbwt-graph.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f miniGiraffe time-utils.so perf-utils.so