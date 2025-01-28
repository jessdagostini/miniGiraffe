#include <iostream>

typedef struct { char data[sizeof(long long int)]; } handle_t;
typedef std::pair<handle_t, int64_t> seed_type;

int main() {
    printf("Size of pair: %zu bytes\n", sizeof(seed_type));
}