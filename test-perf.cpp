#include <cstdlib>
#include <iostream>
#include <vector>
#include <omp.h>

#include "perf-event.hpp"

int main(int argc, char* argv[]) {
  int num_elements = atoi(argv[1]);
  int num_threads = atoi(argv[2]);
  std::vector<int> v(num_elements);
  PerfEvent e;
  e.startCounters();
  omp_set_num_threads(num_threads);
  #pragma omp parallel for
  for (int i=0; i<num_elements; i++)
    v[i] = i*i;
  e.stopCounters();
  e.printReport(std::cout, 1);
  std::cout << std::endl;
  return 0;
}
