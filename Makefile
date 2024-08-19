CXX = g++
#CXX = ~/esbmc/bin/esbmc
CXXFLAGS = -fopenmp -O3 -g
LDFLAGS = -shared
LIBS = -lm
INCLUDES = -I${HOME}/giraffe-proxy \
           -I${HOME}/giraffe-proxy/deps/gbwtgraph/include \
           -I${HOME}/giraffe-proxy/deps/gbwt/include \
           -I${HOME}/giraffe-proxy/deps/sdsl-lite/include \
           -I${HOME}/giraffe-proxy/deps/libhandlegraph/lib/usr/local/include

LIBS = -L${HOME}/giraffe-proxy/deps/sdsl-lite/lib \
       -L${HOME}/giraffe-proxy/deps/gbwt/lib \
       -L${HOME}/giraffe-proxy/deps/libhandlegraph/lib/usr/local/lib \
       -L${HOME}/giraffe-proxy/deps/gbwtgraph/lib

LDLIBS = -lgbwtgraph -lgbwt -lhandlegraph -lsdsl -ldivsufsort -ldivsufsort64

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

check-input: check-input.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f main openmp time-utils.so
