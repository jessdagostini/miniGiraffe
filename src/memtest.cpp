#include <iostream>
#include <fstream>
#include <string>
#include <sstream> // Required for stringstream


// double numeric_value = 0;

void print_memory_usage(const std::string& message) {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    std::string trash;
    double numeric_value = 0;
    while (std::getline(status_file, line)) {
        if (line.find("VmRSS:") == 0 || line.find("VmSize:") == 0) {
            // std::cout << message << " " << line << std::endl;
            std::stringstream ss(line);
            ss >> trash >> numeric_value;
            std::cout << numeric_value << std::endl;
        }
        // std::cout << line << std::endl;
    }
}

int main() {
    // Example usage
    print_memory_usage("Current memory usage:");
    return 0;
}