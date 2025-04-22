#include <iostream>
#include <getopt.h>

int main(int argc, char* argv[]) {
  int option;
  std::string name;
  int count = 1;

  while ((option = getopt(argc, argv, "hn:c:")) != -1) {
    switch (option) {
        case 'h':
            std::cout << "Help" << std::endl;
            break;
      case 'n':
        name = optarg;
        break;
      case 'c':
        count = std::stoi(optarg);
        break;
      case '?':
        if (optopt == 'n' || optopt == 'c') {
          std::cerr << "Option -" << (char)optopt << " requires an argument." << std::endl;
        } else {
          std::cerr << "Unknown option -" << (char)optopt << std::endl;
        }
        return 1;
      default:
        return 1;
    }
  }

  for (int i = 0; i < count; ++i) {
    std::cout << "Hello, " << name << "!" << std::endl;
  }

  return 0;
}