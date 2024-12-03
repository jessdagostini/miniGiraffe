#include <iostream>
#include <vector>
#include <functional>
#include <cstdlib>

class Base {
public:
    virtual void execute() {
        // Virtual function to create indirect calls
        //std::cout << "Base execute\n";
	int j = 0 + 1;
    }
};

class Derived : public Base {
public:
    void execute() override {
        //std::cout << "Derived execute\n";
	int j = 0 + 1;
    }
};

// A hot function that is likely to be optimized by BOLT
void hotFunction() {
    for (int i = 0; i < 1000; ++i) {
        if (i % 100 == 0) {
            //std::cout << "Processing hot path\n";
	    int j = 0 + 1;
        }
    }
}

// A cold function that rarely gets called
void coldFunction() {
    for (int i = 0; i < 10; ++i) {
        if (i % 2 == 0) {
            //std::cout << "Processing cold path\n";
	    int j = 0 + 1;
        }
    }
}

int main() {
    // A vector of function pointers to simulate indirect calls
    std::vector<std::function<void()>> functions;
    functions.push_back(hotFunction);
    functions.push_back(coldFunction);

    Derived d;
    Base *ptr = &d;

    // Simulate high forward branches with conditionals
    for (int i = 0; i < 1000000; ++i) {
        if (i % 500 == 0) {
            functions[0]();  // Calls hotFunction frequently
        } else if (i % 250000 == 0) {
            functions[1]();  // Calls coldFunction occasionally
        }

        // Simulate indirect calls
        ptr->execute();

        // Forward and backward branches
        if (i % 3 == 0) {
            // Forward branch, often not taken
            //std::cout << "Branch taken\n";
	    i = i;
        } else if (i % 100 == 0) {
            // Backward branch
            i -= 10;
        }
    }

    return 0;
}
