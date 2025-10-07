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


struct Source {
    string sequence;
    pair_hash_set seeds;
    new_pair_hash_set new_seeds; // For future use, if needed
};

struct SeedSource {
    seed_type seed;
    vector<int> seq_idx;
};

struct SeqOffset {
    int64_t offset;
    int seq_idx;
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

void write_seeds(string fileName, vector<Source> &data) {
    // Create an output file stream
    ofstream outFile(fileName, std::ios::binary | std::ios::app);

    if (!outFile) {
        cerr << "Error opening file: " << fileName << std::endl;
    }

    for (auto d:data) {
        // Write string to the file
        if (d.sequence.empty() || d.sequence.size() == 0) {
            cerr << "EMPTY SEQUENCE" << std::endl;
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

void write_seeds(string fileName, vector<string>&sequences, vector<pair<new_seed_type, SeedSource>> &data) {
    // Create an output file stream
    std::ofstream outFile(fileName, std::ios::binary | std::ios::app);

    if (!outFile) {
        std::cerr << "Error opening file: " << fileName << std::endl;
    }

    // First write the sequences
    size_t seqs = sequences.size();
    // Write how many sequences
    outFile.write(reinterpret_cast<const char*>(&seqs), sizeof(seqs));
    for (auto seq: sequences) {
        // Write string to the file
        if (seq.empty() || seq.size() == 0) {
            std::cerr << "EMPTY SEQUENCE" << std::endl;
        }
        outFile.write(seq.c_str(), seq.size() + 1); // Include null terminator
    }
    // Now write the seeds
    // First write the number of seeds
    size_t seeds = data.size();
    // Write how many seeds
    outFile.write(reinterpret_cast<const char*>(&seeds), sizeof(seeds));

    // Loop over the seeds
    for (auto d : data) {
        outFile.write(d.second.seed.first.data, sizeof(d.second.seed.first.data));

        outFile.write(reinterpret_cast<const char*>(&d.second.seed.second), sizeof(d.second.seed.second));

        size_t sequences = d.second.seq_idx.size();
        outFile.write(reinterpret_cast<const char*>(&sequences), sizeof(sequences));

        for (auto seq: d.second.seq_idx) {
            // Write int64_t to the file
            outFile.write(reinterpret_cast<const char*>(&seq), sizeof(seq));
        }
    }

    // Close the file stream
    outFile.close();
}

void write_seeds(string fileName, vector<string>&sequences, vector<pair<gbwtgraph::handle_t, vector<SeqOffset>>> &data) {
    // Create an output file stream
    std::ofstream outFile(fileName, std::ios::binary | std::ios::app);

    if (!outFile) {
        std::cerr << "Error opening file: " << fileName << std::endl;
    }

    // First write the sequences
    size_t seqs = sequences.size();
    // Write how many sequences
    outFile.write(reinterpret_cast<const char*>(&seqs), sizeof(seqs));
    for (auto seq: sequences) {
        // Write string to the file
        if (seq.empty() || seq.size() == 0) {
            std::cerr << "EMPTY SEQUENCE" << std::endl;
        }
        outFile.write(seq.c_str(), seq.size() + 1); // Include null terminator
    }
    // Now write the seeds
    // First write the number of seeds
    size_t seeds = data.size();
    // Write how many seeds
    outFile.write(reinterpret_cast<const char*>(&seeds), sizeof(seeds));

    // Loop over the seeds
    for (auto d : data) {
        outFile.write(reinterpret_cast<const char*>(&d.first), sizeof(d.first));

        size_t sequences = d.second.size();
        // Write how many sequences
        outFile.write(reinterpret_cast<const char*>(&sequences), sizeof(sequences));
        for (auto seq: d.second) {
            // Write int64_t to the file
            outFile.write(reinterpret_cast<const char*>(&seq.offset), sizeof(seq.offset));
            outFile.write(reinterpret_cast<const char*>(&seq.seq_idx), sizeof(seq.seq_idx));
        }
    }

    // Close the file stream
    outFile.close();
}

double get_wall_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time); // or CLOCK_REALTIME
    return time.tv_sec + time.tv_nsec / 1000000000.0;
}

int main(int argc, char *argv[]) {
    vector<Source> dump;

    // Map to reorder the input according to number of repeated seeds
    // map<new_seed_type, SeedSource> node_to_seq;
    map<gbwtgraph::handle_t, vector<SeqOffset>> node_to_seq;

    string filename_dump = argv[1];

    load_seeds(filename_dump, dump);

    vector<string> sequences;
    string past_seq = "";
    // sequences.reserve(dump.size());
    int idx = 0;

    double start, end;

    start = get_wall_time();
    sort(dump.begin(), dump.end(), [](const Source& a, const Source& b) -> bool {
        return a.sequence < b.sequence;
    });
    end = get_wall_time();
    cout << "Time to sort the input: " << end - start << endl;

    start = get_wall_time();
    for (auto d : dump) {
        // cout << "Processing sequence: " << d.sequence << endl;
        if (past_seq != d.sequence) {
            sequences.push_back(d.sequence);
            // cout << "Adding sequence: " << d.sequence << endl;
        }
        // else {
        //     cout << "Duplicate sequence: " << past_seq << " " << d.sequence << endl;
        // }

        idx = sequences.size() - 1;
        // cout << "Idx: " << idx << endl;

        // cout << "Idx" << idx << endl;
        for (auto seed : d.seeds) {
            // cout << "Sequence: " << d.sequence << endl;
            // Create a new seed using node id as the first element
            // new_seed_type new_seed = new_seed_type(gbwtgraph::GBWTGraph::handle_to_node(seed.first), seed.second);
            // gbwtgraph::nid_t seed_id = gbwtgraph::GBWTGraph::handle_to_node(seed.first);
            // gbwtgraph::handle_t node = gbwtgraph::GBWTGraph::node_to_handle(seed_id);
            // if (node == seed.first) {
            //     cout << "Convertion work" << endl;
            // } 


            // If the seed is not already in the map, create a new entry
            if (node_to_seq.find(seed.first) == node_to_seq.end()) {
                node_to_seq[seed.first] = vector<SeqOffset>();
                // cout << "Adding new seed: " << new_seed.first << " " << new_seed.second << endl;
            }
            // Add more sequences to the seed (i.e. we have repetitions)
            // node_to_seq[new_seed].seq_idx.push_back(idx);
            node_to_seq[seed.first].push_back({seed.second, idx});
            // cout << "Adding sequence: " << idx << " to seed " << new_seed.first << endl;
        }
        past_seq = d.sequence;
        // if (++i == 1000) {
        //     break;
        // }
    }
    end = get_wall_time();
    cout << "Time to create the map: " << end - start << endl;


    // Convert map into vector to sort by value (number of sequences)
    vector<pair<gbwtgraph::handle_t, vector<SeqOffset>>> mapVector(node_to_seq.begin(), node_to_seq.end());

    start = get_wall_time();
    // Sort it, getting seeds with more repetitions first
    sort(mapVector.begin(), mapVector.end(), [](const pair<gbwtgraph::handle_t, vector<SeqOffset>>& a, const pair<gbwtgraph::handle_t, vector<SeqOffset>>& b) -> bool {
        // return a.second.seq_idx.size() > b.second.seq_idx.size();
        return a.first < b.first;
    });
    end = get_wall_time();
    cout << "Time to sort the map: " << end - start << endl;

    int i = 0;
    // Print the sorted map
    cout << "Sorted map:" << endl;
    for (auto it : mapVector) {
        gbwtgraph::nid_t seed_id = gbwtgraph::GBWTGraph::handle_to_node(it.first);
        cout << "Node: " << seed_id << endl;
        // cout << "Seed: " << it.second.seed.first.data << " " << it.second.seed.second << endl;
        for (int j = 0; j < it.second.size(); j++) {
            cout << "Offset: " << it.second[j].offset << " Sequence: " << sequences[it.second[j].seq_idx] << endl;
        }
        // cout << "Sequences: " << it.second.seq_idx.size() << endl;
        // for (auto seq : it.second.seq_idx) {
        //     cout << sequences[seq] << endl;
        // }
        if (++i == 10) {
            break;
        }
    }

    write_seeds("dump_proxy_really_new.bin", sequences, mapVector);

    // Create the struct to the new input
    // vector<SeedSource> sorted_dump;

    // cout << "Starting looping over the map" << endl;
    // start = get_wall_time();
    // Loop over the sorted by value map
    // for (auto s : mapVector) {
    //     SeedSource tmp;
        
        // Find the seed in the new_seeds vector
        // auto it = find_if(
        //     new_seeds.begin(), new_seeds.end(),
        //     [s](const auto& p) { return p.first == s.first; });
    
        // If the iterator is not equal to the end of the
        // vector, the pair was found
        // if (it != new_seeds.end()) {
        //     cout << "Pair found with first element: "
        //          << it->first
        //          << " and second element: " << it->second
        //          << endl;
        // }
        
        // Convert the seed back to the original type, using the seed found in the map, and store in the new source struct
        // tmp.seed = seed_type(gbwtgraph::GBWTGraph::node_to_handle(it->first), it->second);
        // Loop over all sequences and store in the new source struct
        // for (auto seq : s.second) {
        //     tmp.sequences.push_back(seq);
        // }

        // Add the new source to the output
        // sorted_dump.push_back(tmp);
    // }
    // end = get_wall_time();
    // cout << "Time to create the new input: " << end - start << endl;

    // for (auto d : sorted_dump) {
    //     cout << "Seed: " << d.seed.first.data << " " << d.seed.second << endl;
    //     cout << "Sequences: " << d.sequences.size() << endl;
    //     for (auto seq : d.sequences) {
    //         cout << seq << endl;
    //     }
    // }

    // for (auto it : mapVector) {
    //     cout << "Node: " << it.first << endl;
    //     cout << "Sequences: " << it.second.size() << endl;
        // for (auto seq : it.second) {
        //     cout << seq << endl;
        // }
    // }

    // Save the binary file with the new sorted_dump
    // write_seeds("dump_proxy_yeast_seed_ordered.bin", sorted_dump);

    // for (auto d : dump) {
    //     for (auto seed : d.seeds) {
    //         d.new_seeds.push_back(new_seed_type(gbwtgraph::GBWTGraph::handle_to_node(seed.first), seed.second));
    //     }
    // }

    // sort(dump.begin(), dump.end(), [](const Source& a, const Source& b) -> bool {
    //     return a.sequence < b.sequence;
    // });
    
    // for (auto d : dump) {
    //     cout << "Sequence: " << d.sequence << endl;
    //     cout << "Seeds: " << d.seeds.size() << endl;
    //     for (auto seed : d.new_seeds) {
    //         cout << "Seed: " << seed.first 
    //              << ", Position: " << seed.second << endl;
    //     }
    // }
    return 0;
}