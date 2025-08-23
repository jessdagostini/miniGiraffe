#include <iostream>
#include <unistd.h>

int main() {
    long cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (cache_line_size == -1) {
        std::cerr << "Unable to determine cache line size." << std::endl;
    } else {
        std::cout << "Cache line size: " << cache_line_size << " bytes" << std::endl;
    }
    return 0;
}