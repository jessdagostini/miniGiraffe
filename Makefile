CXX = g++
CXXFLAGS = -fopenmp -O3 -g #-fno-reorder-blocks-and-partition

LDFLAGS = -shared
LIBS = -lm

CURR_DIR := $(shell pwd)

INCLUDES = -I$(CURR_DIR) \
           -I$(CURR_DIR)/deps/gbwtgraph/include \
           -I$(CURR_DIR)/deps/gbwt/include \
           -I$(CURR_DIR)/deps/sdsl-lite/include \
           -I$(CURR_DIR)/deps/libhandlegraph/build/usr/local/include \
		   -I$(CURR_DIR)/src
	# -I$(CURR_DIR)/spack/opt/spack/linux-ubuntu22.04-cascadelake/gcc-11.3.0/caliper-topdown_csx-lulccioywdcebe7odyjwvanhqklu7bsd/include
		#    -I/opt/intel/oneapi/vtune/2023.2.0/sdk/include/

LIBS = -L$(CURR_DIR)/deps/sdsl-lite/lib \
       -L$(CURR_DIR)/deps/gbwt/lib \
       -L$(CURR_DIR)/deps/libhandlegraph/build/usr/local/lib \
       -L$(CURR_DIR)/deps/gbwtgraph/lib
	#   -L$(CURR_DIR)/spack/opt/spack/linux-ubuntu22.04-cascadelake/gcc-11.3.0/caliper-topdown_csx-lulccioywdcebe7odyjwvanhqklu7bsd/lib
	#    -L/opt/intel/oneapi/vtune/2023.2.0/lib64

LDLIBS = -lgbwtgraph -lgbwt -lhandlegraph -lsdsl -ldivsufsort -ldivsufsort64 # -lcaliper -littnotify

metric-utils.so: src/metric-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

miniGiraffe: src/miniGiraffe.cpp metric-utils.so
	$(CXX) -fopenmp -pthread -O3 -g $(INCLUDES) $(LIBS) $^ $(LDLIBS) -Wl,--emit-relocs -o $@

miniGiraffeMPI: tests/miniGiraffeMPI.cpp metric-utils.so
	mpic++ -fopenmp -pthread -O3 -g $(INCLUDES) $(LIBS) $^ $(LDLIBS) -Wl,--emit-relocs -o $@

reordering-input: tests/reordering-input.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

read-input: tests/read-input.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

get-sample: tests/get-sample.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

lower-input: tests/lower-input.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -f miniGiraffe time-utils.so perf-utils.so
