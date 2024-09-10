#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <gbwtgraph/gbz.h>
#include <gbwtgraph/cached_gbwtgraph.h>
#include <fstream>

using namespace gbwtgraph;

// typedef struct { char data[sizeof(long long int)]; } handle_t;

typedef struct {
    handle_t first;
    int64_t second;
} seed_type;

typedef struct {
    char** sequence;
    seed_type** seeds;
    int seq_size;
    size_t max_size;
    size_t total_data;
} source;

void free_source(source *s) {
    if (!s) return;

    if (s->sequence) {
        for (int i = 0; i < s->max_size; i++) {
            free(s->sequence[i]);
        }
        free(s->sequence);
    }

    if (s->seeds) {
        for (int i = 0; i < s->max_size; i++) {
            free(s->seeds[i]);
        }
        free(s->seeds);
    }

    free(s);
}

void allocate_source(source *data) {
    data->max_size = 50000000;
    data->seq_size = 151;

    // Allocate memory for the sequences array of array (array of strings)
    data->sequence = (char**)malloc(data->max_size * sizeof(char*));
    if (data->sequence == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(data);
        exit(EXIT_FAILURE);
    }

    // Allocate memory for each string
    for (int i = 0; i < data->max_size; i++) {
        data->sequence[i] = (char *)malloc(data->seq_size * sizeof(char));
        if (data->sequence[i] == NULL) {
            perror("Failed to allocate memory for sequence element");
            // Free previously allocated memory
            for (int j = 0; j < i; ++j) {
                free(data->sequence[j]);
            }
            free(data->sequence);
            free(data);
            exit(EXIT_FAILURE);
        }
    }

    // Allocate memory for the seed pairs (each different entry has its own array of seeds)
    data->seeds = (seed_type**)malloc(data->max_size * sizeof(seed_type*));
    if (data->seeds == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(data);
        exit(EXIT_FAILURE);
    }
}

void load_seeds(const char* filename, source *data) {
    // Open the file in binary mode for reading
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        printf("Error opening the file!\n");
        return;
    }

    char seq[200];
    seed_type tmp;
    int i = 0;
    size_t qnt;
    char ch;

    while (!file.eof()) {
        int j = 0;
        file.read(reinterpret_cast<char*>(data->sequence[i]), sizeof(char) * data->seq_size);
        
        file.read(reinterpret_cast<char*>(&qnt), sizeof(qnt));
        // printf("Qnt %ld\n", qnt);
        
        data->seeds[i] = (seed_type*)malloc(qnt * sizeof(seed_type));
        if (data->seeds[i] == NULL) {
            perror("Error allocating memory for seeds");
            for (int j=0; j<i; j++) {
                free(data->seeds[j]);
            }
            free(data->seeds);
            free(data);
            file.close();
            exit(EXIT_FAILURE);
        }
        
        for(size_t j = 0; j < qnt; j++) {
            file.read(reinterpret_cast<char*>(&tmp), sizeof(seed_type));
            data->seeds[i][j] = std::move(tmp);
            // printf("%ld\n", data->seeds[i][j].second);
        }

        // Check if reading was successful
        if (!file) {
            if (file.eof()) {
                // End of file reached
                break;
            } else {
                printf("Error reading from the file.\n");
                return;
            }
        }

        // if (++i == 10) {
        //     break;
        // }
        i++;
    }
    data->total_data = i;

    // Close the file
    file.close();
}


int main(int argc, char *argv[]) {
    const char *filename_dump = argv[1];
    const char *filename_gbz = argv[2];
    int num_threads = atoi(argv[3]);

    // Alocate memory for the data struct
    source *data = (source*)malloc(sizeof(source));
    if (data == NULL) {
        perror("Failed to allocate memory for source");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for all structs
    allocate_source(data);

    // Read data from dump
    load_seeds(filename_dump, data);

    printf("Sequence [0] %s\n", data->sequence[0]);

    // Free after usage
    free_source(data);

    return 0;
}