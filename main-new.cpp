#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <gbwtgraph/gbz.h>
#include <gbwtgraph/cached_gbwtgraph.h>
#include <fstream>

using namespace gbwtgraph;

// typedef struct { char data[sizeof(long long int)]; } handle_t;

// Index for helping finding temporary data in the GaplessExtension struct
const size_t BEST_MATCH = 0;
const size_t MATCH = 1;
const size_t HANDLE_T_PATH = 1000;

// Alignment score boosters
static constexpr int8_t default_match = 1;
static constexpr int8_t default_mismatch = 4;
static constexpr int8_t default_gap_open = 6;
static constexpr int8_t default_gap_extension = 1;
static constexpr int8_t default_full_length_bonus = 5;
static constexpr double default_gc_content = 0.5;
/// Two full-length alignments are distinct, if the fraction of overlapping
/// position pairs is at most this.
constexpr static double OVERLAP_THRESHOLD = 0.8;
constexpr static size_t MAX_MISMATCHES = 4;
const uint64_t DEFAULT_PARALLEL_BATCHSIZE = 512;

typedef struct {
    handle_t first;
    int64_t second;
} seed_type;

typedef struct {
    size_t first;
    size_t second;
} sizet_pair;

typedef struct {
    char** sequence;
    seed_type** seeds;
    size_t total_data;
    size_t* seeds_qnt;
    int seq_size;
    size_t max_size;
} source;

typedef struct {
    // In the graph
    handle_t** path;
    size_t* offset;
    gbwt::BidirectionalState* state;

    // In the read
    sizet_pair* read_interval;
    size_t** mismatch_positions;

    // Alignment properties
    int32_t* score;
    bool* left_full;
    bool* right_full;

    // For internal use
    bool* left_maximal;
    bool* right_maximal;
    uint32_t* internal_score;
    uint32_t* old_score;
} GaplessExtension;


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


    // Allocate memory to store how many seeds each sequence has
    data->seeds_qnt = (size_t*)malloc(data->max_size * sizeof(size_t));
    if (data->seeds_qnt == NULL) {
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

void allocate_result(GaplessExtension* result, size_t size) {
    
    result->path = (handle_t**)malloc(size * sizeof(handle_t*));
    if (result->path == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < size; i++) {
        result->path[i] = (handle_t*)malloc(HANDLE_T_PATH * sizeof(handle_t));
        if (result->path[i] == NULL) {
            perror("Failed to allocate memory for path element");
            // Free previously allocated memory
            for (int j = 0; j < i; ++j) {
                free(result->path[j]);
            }
            free(result->path);
            free(result);
            exit(EXIT_FAILURE);
        }
    }

    result->mismatch_positions = (size_t**)malloc(size * sizeof(size_t*));
    if (result->mismatch_positions == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < size; i++) {
        result->mismatch_positions[i] = (size_t*)malloc(HANDLE_T_PATH * sizeof(size_t));
        if (result->mismatch_positions[i] == NULL) {
            perror("Failed to allocate memory for mismatch_positions element");
            // Free previously allocated memory
            for (int j = 0; j < i; ++j) {
                free(result->mismatch_positions[j]);
            }
            free(result->mismatch_positions);
            free(result);
            exit(EXIT_FAILURE);
        }
    }

    result->offset = (size_t*)malloc(size * sizeof(size_t));
    if (result->offset == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->state = (gbwt::BidirectionalState*)malloc(size * sizeof(gbwt::BidirectionalState));
    if (result->state == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->read_interval = (sizet_pair*)malloc(size * sizeof(sizet_pair));
    if (result->read_interval == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->score = (int32_t*)malloc(size * sizeof(int32_t));
    if (result->score == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->left_full = (bool*)malloc(size * sizeof(bool));
    if (result->left_full == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->right_full = (bool*)malloc(size * sizeof(bool));
    if (result->right_full == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->left_maximal = (bool*)malloc(size * sizeof(bool));
    if (result->left_maximal == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->right_maximal = (bool*)malloc(size * sizeof(bool));
    if (result->right_maximal == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->internal_score = (uint32_t*)malloc(size * sizeof(uint32_t));
    if (result->internal_score == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }

    result->old_score = (uint32_t*)malloc(size * sizeof(uint32_t));
    if (result->old_score == NULL) {
        perror("Failed to allocate memory for array of row pointers");
        free(result);
        exit(EXIT_FAILURE);
    }
}

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

void free_result(GaplessExtension *s, size_t size) {
    if (!s) return;

    free(s->score);
    free(s->internal_score);
    free(s->left_full);
    free(s->left_maximal);
    free(s->right_full);
    free(s->right_maximal);
    free(s->old_score);
    free(s->offset);
    free(s->state);
    free(s->read_interval);

    if (s->mismatch_positions) {
        for (int i = 0; i < size; i++) {
            free(s->mismatch_positions[i]);
        }
        free(s->mismatch_positions);
    }

    if (s->path) {
        for (int i = 0; i < size; i++) {
            free(s->path[i]);
        }
        free(s->path);
    }

    free(s);
}

/// Get the node offset from a seed.
static size_t get_node_offset(seed_type seed) {
    return (seed.second < 0 ? -(seed.second) : 0);
}

/// Get the read offset from a seed.
static size_t get_read_offset(seed_type seed) {
    return (seed.second < 0 ? 0 : seed.second);
}

void match_initial(GaplessExtension& match, size_t index, const std::string& seq, gbwtgraph::view_type target) {
    size_t node_offset = match.offset[index];
    size_t left = std::min(seq.length() - match.read_interval[index].second, target.second - node_offset);
    
    while (left > 0) {
        // cout << "Left: " << left << endl;
        size_t len = std::min(left, sizeof(std::uint64_t)); // Number of bytes to be copied
        std::uint64_t a = 0, b = 0; // 8 bytes
        std::memcpy(&a, seq.data() + match.read_interval[index].second, len); // Setting the pointer on seq.data to slot where to start and copying len bytes
        std::memcpy(&b, target.first + node_offset, len); // Setting the pointer in the node offset, copying len bytes
        // cout << a << " " << b << endl;
        if (a == b) { // byte comparision
            match.read_interval[index].second += len;
            node_offset += len;
        } else {
            for (size_t i = 0; i < len; i++) {
                // cout << "[INITIAL] Seq: " << seq[match.read_interval.second] << " Target: " << target.first[node_offset] << endl;
                if (seq[match.read_interval[index].second] != target.first[node_offset]) {
                    match.internal_score[index]++;
                }
                match.read_interval[index].second++;
                node_offset++;
            }
        }
        left -= len;
    }
    match.old_score[index] = match.internal_score[index];
}

void set_score(GaplessExtension *extension, size_t index) {
    extension->score[index] = static_cast<int32_t>((extension->read_interval[index].second - extension->read_interval[index].first) * default_match);
    // Handle the mismatches.
    extension->score[index] -= static_cast<int32_t>(extension->internal_score[index] * (default_match + default_mismatch));
    // Handle full-length bonuses.
    extension->score[index] += static_cast<int32_t>(extension->left_full[index] * default_full_length_bonus);
    extension->score[index] += static_cast<int32_t>(extension->right_full[index] * default_full_length_bonus);
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
        data->seeds_qnt[i] = qnt;

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

    printf("Reading seeds\n");

    // Read data from dump
    load_seeds(filename_dump, data);

    GBZ gbz;
    
    printf("Reading gbz\n");
    sdsl::simple_sds::load_from(gbz, filename_gbz);
    const GBWTGraph* graph = &gbz.graph;

    GaplessExtension* result;
    size_t seeds_size;
    
    for (int index = 0; index < data->total_data; index++) {
        printf("Sequence[%d]: %s\n", index, data->sequence[index]);
        printf("Total seeds[%d]: %ld\n", index, data->seeds_qnt[index]);

        result = (GaplessExtension*)malloc(sizeof(GaplessExtension));
        seeds_size = data->seeds_qnt[index] + 2;
        allocate_result(result, seeds_size);
        size_t best_alignment = std::numeric_limits<size_t>::max(); // Metric to find the best extension from each seed

        for (size_t s = 0, res_index = 2; s < data->seeds_qnt[index]; s++, res_index++) {
        // for (int s = 0; s < 1; s++) {
            size_t node_offset = get_node_offset(data->seeds[index][s]);
            size_t read_offset = get_read_offset(data->seeds[index][s]);

            // Initializing best match
            result->offset[BEST_MATCH] = 0;
            result->read_interval[BEST_MATCH] = { static_cast<size_t>(0), static_cast<size_t>(0) };
            result->score[BEST_MATCH] = std::numeric_limits<int32_t>::min();
            result->left_full[BEST_MATCH] = false;
            result->right_full[BEST_MATCH] = false;
            result->left_maximal[BEST_MATCH] = false;
            result->right_maximal[BEST_MATCH] = false;
            result->internal_score[BEST_MATCH] = std::numeric_limits<uint32_t>::max();
            result->old_score[BEST_MATCH] = std::numeric_limits<uint32_t>::max();

            // Initializing match
            result->path[MATCH][0] = data->seeds[index][s].first;
            result->offset[MATCH] = node_offset;
            result->state[MATCH] = graph->get_bd_state(data->seeds[index][s].first);
            result->read_interval[MATCH] = { read_offset, read_offset };
            result->score[MATCH] = static_cast<uint32_t>(0);
            result->left_full[MATCH] = false;
            result->right_full[MATCH] = false;
            result->left_maximal[MATCH] = false;
            result->right_maximal[MATCH] = false;
            result->internal_score[MATCH] = static_cast<uint32_t>(0);
            result->old_score[MATCH] = static_cast<uint32_t>(0);

            match_initial(*result, MATCH, data->sequence[index], graph->get_sequence_view(data->seeds[index][s].first));
            if (result->read_interval[MATCH].first == 0) {
                result->left_full[MATCH] = true;
                result->left_maximal[MATCH] = true;
            }
            if (result->read_interval[MATCH].second >= sizeof(data->sequence[index])/sizeof(char)) {
                result->right_full[MATCH] = true;
                result->right_maximal[MATCH] = true;
            }
            printf("Match %ld internal score: %d\n", MATCH, result->internal_score[MATCH]);

            set_score(result, MATCH);

            printf("Match %ld final score: %d\n", MATCH, result->score[MATCH]);
        }


        free_result(result, (data->seeds_qnt[index] + 2));       
    }

    // Free after usage
    free_source(data);

    return 0;
}