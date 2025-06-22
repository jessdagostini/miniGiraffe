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

time-utils.so: src/time-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

perf-utils.so: src/perf-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

handle-utils.so: src/handle-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

miniGiraffe: src/miniGiraffe.cpp time-utils.so perf-utils.so handle-utils.so
	$(CXX) -fopenmp -pthread -O3 -g $(INCLUDES) $(LIBS) $^ $(LDLIBS) -Wl,--emit-relocs -o $@

miniGiraffeMPI: src/miniGiraffeMPI.cpp time-utils.so perf-utils.so
	mpic++ -fopenmp -pthread -O3 -g $(INCLUDES) $(LIBS) $^ $(LDLIBS) -Wl,--emit-relocs -o $@

miniGiraffeNC: src/miniGiraffe-noCache.cpp time-utils.so perf-utils.so
	$(CXX) -fopenmp -pthread -O3 -g $(INCLUDES) $(LIBS) $^ $(LDLIBS) -Wl,--emit-relocs -o $@

miniGiraffeInterchange: src/miniGiraffeInterchange.cpp time-utils.so perf-utils.so
	$(CXX) -fopenmp -pthread -O3 -g $(INCLUDES) $(LIBS) $^ $(LDLIBS) -Wl,--emit-relocs -o $@

main-new: src/main-new.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

reordering-input: src/reordering-input.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

read-input: src/read-input.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

get-sample: src/get-sample.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

exp-gbwt: src/exploring-gbwt-graph.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

test-size: src/test-size.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f miniGiraffe time-utils.so perf-utils.so
