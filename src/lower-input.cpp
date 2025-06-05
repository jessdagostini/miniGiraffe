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

string modifyFilenameString(const string& filename, const string& suffix) {
    size_t lastDotPos = filename.rfind('.');
    if (lastDotPos == string::npos) { // No extension
        return filename + suffix;
    }
    string baseName = filename.substr(0, lastDotPos);
    string extension = filename.substr(lastDotPos);
    return baseName + suffix + extension;
}

void load_seeds(string filename, vector<Source> &data) {
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
                cerr << "Error reading from the file." << endl;
                return;
            }
        }
        
        data.push_back(tmpData);
        tmpData = Source();
        // if (++q == size) {
        //     break;
        // }
    }

    // Close the file
    file.close();
}

void write_seeds(string fileName, vector<Source> &data, size_t size) {
    // Create an output file stream
    ofstream outFile(fileName, ios::binary | ios::app);

    if (!outFile) {
        cerr << "Error opening file: " << fileName << endl;
    }

    int q = 0;

    for (auto d:data) {
        // Write string to the file
        if (d.sequence.empty() || d.sequence.size() == 0) {
            cerr << "EMPTY SEQUENCE" << endl;
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

        if (++q == size) {
            break;
        }
    }

    cout << "Final q value: " << q << endl;

    // Close the file stream
    outFile.close();
}

int main(int argc, char *argv[]) {
    vector<Source> dump;

    string filename_dump = argv[1];

    load_seeds(filename_dump, dump);

    size_t new_size = dump.size() * 10 / 100; // Reduce to 10% of the original size

    cout << "Dump size: " << dump.size() << endl;
    cout << "New size: " << new_size << endl;

    string filename_new_dump = argv[2];
    cout << "New dump filename: " << filename_new_dump << endl;

    write_seeds(filename_new_dump, dump, new_size);

    return 0;
}