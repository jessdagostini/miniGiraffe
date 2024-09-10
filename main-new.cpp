#include <cstdlib>
#include <cstdio>
#include <cstring>
// #include <gbwtgraph/gbz.h>
// #include <gbwtgraph/cached_gbwtgraph.h>
#include <fstream>

// using namespace gbwtgraph;


typedef struct { char data[sizeof(long long int)]; } handle_t;

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

void load_seeds_wrong(const char* filename, source *data) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    size_t bytes_read;
    int i = 0;
    int qnt;
    char t[4];
    seed_type tmp;
    // handle_t *tmp_h = (handle_t*)malloc(sizeof(handle_t));
    // int64_t *tmp_s = (int64_t*)malloc(sizeof(int64_t));

    while ((bytes_read = fread(data->sequence[i], sizeof(char), data->seq_size, file)) > 0) {
        if (bytes_read != data->seq_size) {
            perror("Error reading seq");
            fclose(file);
            return;
        }
        printf("Sequence[%d]: %s\n", i, data->sequence[i]);

        bytes_read = fread(&qnt, sizeof(char), 1, file);
        if (bytes_read == 0) {
            perror("Error reading qnt");
            fclose(file);
            return;
        }

        printf("Qntd[%d]: %d\n", i, qnt);

        data->seeds[i] = (seed_type*)malloc(qnt * sizeof(seed_type));
        if (data->seeds[i] == NULL) {
            perror("Error allocating memory for seeds");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        for (int j = 0; j < qnt; j++) {
            bytes_read = fread(&tmp.first.data, sizeof(char), sizeof(long long int), file);
            bytes_read = fread(&tmp.second, sizeof(int64_t), 1, file);
            printf("First %s\n", tmp.first.data);
            printf("Second %ld\n", tmp.second);
        }


        // bytes_read = fread(data->seeds[i], sizeof(seed_pair), qnt, file);
        // printf("bytes_read %zu\n", bytes_read);
        // printf("%s\n", data->seeds[i][0].first.data);
        // if (bytes_read != qnt) {
        //     perror("Error reading seeds");
        //     free(data->seeds[i]); // Clean up allocated memory
        //     fclose(file);
        //     exit(EXIT_FAILURE);
        // }

        bytes_read = fread(&t, sizeof(char), 4, file);
        printf("%s\n", t);


        // if (feof(file)) {
        //     printf("End of file reached before reading the integer.\n");
        // } else if (ferror(file)) {
        //     perror("Error reading file");
        // }
        if (++i == 10) {
            break;
        } 
    }

    fclose(file);
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
            // file.read(reinterpret_cast<char*>(&h), sizeof(handle_t));
            // file.read(reinterpret_cast<char*>(&t), sizeof(char));
            // file.read(reinterpret_cast<char*>(&s), sizeof(int64_t));
            file.read(reinterpret_cast<char*>(&tmp), sizeof(seed_type));
            data->seeds[i][j] = std::move(tmp);
            // printf("%ld\n", data->seeds[i][j].second);
        }

        // file.read(reinterpret_cast<char*>(&t), sizeof(char) * 4);

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
    data->max_size = 50000000;
    data->seq_size = 151;

    // Allocate memory for the sequences array of array (array of strings)
    data->sequence = (char**)malloc(data->max_size * sizeof(char*));
    if (data->sequence == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(data);
        return -1;
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
            return -1;
        }
    }

    // Allocate memory for the seed pairs (each different entry has its own array of seeds)
    data->seeds = (seed_type**)malloc(data->max_size * sizeof(seed_type*));
    if (data->seeds == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(data);
        exit(EXIT_FAILURE);
    }

    // for (int i = 0; i < data->total_data; i++) {
    //     data->seeds[i] = (seed_pair*)malloc(20 * sizeof(seed_pair));
    //     if (data->seeds[i] == NULL) {
    //         perror("Failed to allocate memory for array of row pointers");
    //         free(data);
    //         exit(EXIT_FAILURE);
    //     }
    // }

    load_seeds(filename_dump, data);
    printf("Last data - Seq %s\n", data->sequence[9]);
    printf("Last data - Seed First%s\n", data->seeds[0][0].first.data);
    printf("Last data - Seed Second %ld\n", data->seeds[9][0].second);

    free_source(data);
    return 0;
}