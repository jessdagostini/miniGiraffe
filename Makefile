CLANGXX = clang++
CLANGXXFLAGS = -O3 -g -fsanitize=address

CXX = g++
CXXFLAGS = -fopenmp -O3 -g #-fno-reorder-blocks-and-partition

LDFLAGS = -shared
LIBS = -lm
INCLUDES = -I${HOME}/miniGiraffe \
           -I${HOME}/miniGiraffe/deps/gbwtgraph/include \
           -I${HOME}/miniGiraffe/deps/gbwt/include \
           -I${HOME}/miniGiraffe/deps/sdsl-lite/include \
           -I${HOME}/miniGiraffe/deps/libhandlegraph/lib/usr/local/include \
		   -I${HOME}/Caliper/build/include/
		#    -I/opt/intel/oneapi/vtune/2023.2.0/sdk/include/

LIBS = -L${HOME}/miniGiraffe/deps/sdsl-lite/lib \
       -L${HOME}/miniGiraffe/deps/gbwt/lib \
       -L${HOME}/miniGiraffe/deps/libhandlegraph/lib/usr/local/lib \
       -L${HOME}/miniGiraffe/deps/gbwtgraph/lib \
	   -L${HOME}/Caliper/build/lib/
	#    -L/opt/intel/oneapi/vtune/2023.2.0/lib64

LDLIBS = -lgbwtgraph -lgbwt -lhandlegraph -lsdsl -ldivsufsort -ldivsufsort64 -lcaliper # -littnotify

count-seq-per-seeds: count-seq-per-seeds.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

time-utils.so: time-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

perf-utils.so: perf-utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -shared -fPIC $(LIBS) $^ $(LDLIBS) -o $@

main: main.cpp time-utils.so
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

openmp: main-openmp.cpp time-utils.so perf-utils.so
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

main-new: main-new.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

check-input: check-input.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

exp-gbwt: exploring-gbwt-graph.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f main main-new openmp time-utils.so
