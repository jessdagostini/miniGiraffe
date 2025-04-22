#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>

#include <gbwtgraph/gbwtgraph.h>
#include <gbwtgraph/utils.h>


using namespace std;

// typedef struct { char data[sizeof(long long int)]; } handle_t;
typedef pair<gbwtgraph::handle_t, int64_t> seed_type;
typedef pair<gbwtgraph::nid_t, int64_t> new_seed_type;
typedef vector<seed_type> pair_hash_set;
typedef vector<new_seed_type> new_pair_hash_set;

int overall_qnt = 0;

struct Source {
    string sequence;
    pair_hash_set seeds;
    new_pair_hash_set new_seeds; // For future use, if needed
};

int load_seeds(string filename, vector<Source> &data) {
    // Open the file in binary mode for reading
    ifstream file(filename, ios::binary);

    if (!file.is_open()) {
        cerr << "Error opening the file!" << endl;
        return -1;
    }
    
    Source tmpData;
    size_t qnt;
    seed_type tmp;
    char ch;
    int q = 0;

    while (!file.eof()) {
        while (file.get(ch) && ch != '\0') {
            tmpData.sequence += ch;
        }
        // cout << tmpData.sequence << endl;
        file.read(reinterpret_cast<char*>(&qnt), sizeof(qnt));
        overall_qnt += qnt;
        // cout << "Qnt " << qnt << endl;
        for(size_t i = 0; i < qnt; i++) {
            file.read(reinterpret_cast<char*>(&tmp), sizeof(seed_type));
            // cout << tmp.second << endl;
            tmpData.seeds.push_back(tmp);
        }

        // Check if reading was successful
        if (!file) {
            if (file.eof()) {
                // End of file reached
                break;
            } else {
                cerr << "Error reading from the file." << endl;
                return -1;
            }
        }
        
        data.push_back(tmpData);
        tmpData = Source();
        // if (++q == 10) {
        //     break;
        // }
    }

    // Close the file
    file.close();
    return 0;
}

void write_seeds(vector<Source> &data) {
    // Specify the file name
    const char* fileName = "dump_proxy_sampled.bin";

    // Create an output file stream
    std::ofstream outFile(fileName, std::ios::binary | std::ios::app);

    if (!outFile) {
        std::cerr << "Error opening file: " << fileName << std::endl;
    }

    for (auto d: data) {
        // Write string to the file
        if (d.sequence.empty() || d.sequence.size() == 0) {
            std::cerr << "EMPTY SEQUENCE" << std::endl;
        }
        outFile.write(d.sequence.c_str(), d.sequence.size() + 1); // Include null terminator
        size_t seeds = d.seeds.size();

        // Write how many seeds
        outFile.write(reinterpret_cast<const char*>(&seeds), sizeof(seeds));

        for (seed_type seed : d.seeds) {
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

    string filename_dump = argv[1];

    load_seeds(filename_dump, dump);

    double sample_fraction = 0.0194; // 1.95% of the total
    // // double sample_fraction = 0.001;
    // size_t num_to_sample = static_cast<size_t>(dump.size() * sample_fraction);
    size_t num_to_sample = static_cast<size_t>(overall_qnt * sample_fraction);

    std::cout << "Total number of sources: " << overall_qnt << std::endl;
    std::cout << "Number of sources to sample (1.95%): " << num_to_sample << std::endl;

    // 1. Create a random number generator engine
    std::random_device rd;
    std::mt19937 gen(rd());

    // 2. Create a vector of indices from 0 to all_sources.size() - 1
    std::vector<size_t> indices(dump.size());
    std::iota(indices.begin(), indices.end(), 0); // Fill with 0, 1, 2, ...

    // 3. Shuffle the indices randomly
    std::shuffle(indices.begin(), indices.end(), gen);

    // 4. Take the first num_to_sample indices to get your random sample
    std::vector<Source> sampled_sources;
    // for (size_t i = 0; i < num_to_sample; ++i) {
    //     sampled_sources.push_back(dump[indices[i]]);
    // }

    int i = 0;
    while (i < num_to_sample) {
        sampled_sources.push_back(dump[indices[i]]);
        i = i + dump[indices[i]].seeds.size();
    }

    write_seeds(sampled_sources);
    std::cout << "Sampled " << sampled_sources.size() << " sources." << std::endl;

    // Example of accessing the sampled data:
    // std::cout << "\nSampled Sources:\n";
    // for (const auto& source : sampled_sources) {
    //     std::cout << "Sequence: " << source.sequence << ", Seeds size: " << source.seeds.size() << std::endl;
    // }

    return 0;
}