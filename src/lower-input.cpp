#include <iostream>
#include <fstream>
// #include <gbwtgraph/gbz.h>
// #include <gbwtgraph/cached_gbwtgraph.h>
#include <queue>
#include <unordered_map>

#ifdef USE_UNORDERED_SET
#include <unordered_set>
#else
#include <vector>
#endif

using namespace std;
// using namespace gbwtgraph;

typedef struct { char data[sizeof(long long int)]; } handle_t;
// typedef pair<handle_t, int64_t> seed_type;

typedef struct {
    handle_t first;
    int64_t second;
} seed_type;

#ifdef USE_UNORDERED_SET
typedef unordered_set<seed_type> pair_hash_set;
#else
typedef vector<seed_type> pair_hash_set;
#endif

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

struct Source {
    string sequence;
    pair_hash_set seeds;
};

void load_seeds(string filename, vector<Source> &data, size_t size) {
    // Open the file in binary mode for reading
    ifstream file(filename, ios::binary);

    if (!file.is_open()) {
        cerr << "Error opening the file!" << endl;
        return;
    }
    
    Source tmpData;
    size_t qnt;
    seed_type tmp;
    char ch;
    int q = 0;

    cout << "Reading seeds" << endl;

    while (!file.eof()) {
        while (file.get(ch) && ch != '\0') {
            tmpData.sequence += ch;
        }
        file.read(reinterpret_cast<char*>(&qnt), sizeof(qnt));
        for(size_t i = 0; i < qnt; i++) {
            file.read(reinterpret_cast<char*>(&tmp), sizeof(seed_type));
            tmpData.seeds.push_back(tmp);
        }

        // Check if reading was successful
        if (!file) {
            if (file.eof()) {
                // End of file reached
                break;
            } else {
                std::cerr << "Error reading from the file." << std::endl;
                return;
            }
        }
        
        data.push_back(tmpData);
        tmpData = Source();
        if (++q == size) {
            break;
        }
    }

    // Close the file
    file.close();
}

void write_seeds(string fileName, vector<Source> &data) {
    // Create an output file stream
    std::ofstream outFile(fileName, std::ios::binary | std::ios::app);

    if (!outFile) {
        std::cerr << "Error opening file: " << fileName << std::endl;
    }

    for (auto d:data) {
        // Write string to the file
        if (d.sequence.empty() || d.sequence.size() == 0) {
            std::cerr << "EMPTY SEQUENCE" << std::endl;
        }
        outFile.write(d.sequence.c_str(), d.sequence.size() + 1); // Include null terminator
        
        size_t seeds = d.seeds.size();
        // Write how many seeds
        outFile.write(reinterpret_cast<const char*>(&seeds), sizeof(seeds));

        for (auto seed: d.seeds) {
            // Write char array to the file
            outFile.write(seed.first.data, sizeof(seed.first.data));

            // Write int64_t to the file
            outFile.write(reinterpret_cast<const char*>(&seed.second), sizeof(seed.second));
        }
    }

    // Close the file stream
    outFile.close();
}

int main(int argc, char *argv[]) {
    vector<Source> dump;

    // size_t size = static_cast<size_t>(atoi(argv[1]));

    string filename_dump = argv[2];

    load_seeds(filename_dump, dump);

    // string filename_new_dump = argv[3];

    // write_seeds(filename_new_dump, dump);
    
    for (auto d : dump) {
        cout << "Sequence: " << d.sequence << endl;
        cout << "Seeds: " << d.seeds.size() << endl;
        for (auto seed : d.seeds) {
            cout << "Seed: " << string(seed.first.data, sizeof(seed.first.data)) 
                 << ", Position: " << seed.second << endl;
        }
    }
    return 0;
}