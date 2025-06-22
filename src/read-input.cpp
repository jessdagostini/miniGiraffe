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

int load_seeds(string filename, vector<string>&sequences, vector<SeedSource> &data) {
    // Open the file in binary mode for reading
    ifstream file(filename, ios::binary);

    if (!file.is_open()) {
        cerr << "Error opening the file!" << endl;
        return -1;
    }

    SeedSource tmpData;
    size_t seqs, sources, seq_idxs;
    char ch;

    file.read(reinterpret_cast<char*>(&seqs), sizeof(seqs));
    for (size_t i = 0; i < seqs; i++) {
        string tmpSeq;
        while (file.get(ch) && ch != '\0') {
            tmpSeq += ch;
        }

        // cout << "Seq: " << tmpSeq << endl;

        sequences.push_back(tmpSeq);

        if (!file) {
            if (file.eof()) {
                // End of file reached
                cerr << "End of file reached while reading sequences." << endl;
                break;
            } else {
                cerr << "Error reading from the file." << endl;
                return -1;
            }
        }
    }
    cout << "Done reading sequences" << endl;

    file.read(reinterpret_cast<char*>(&sources), sizeof(sources));
    for (size_t i = 0; i < sources; i++) {
        file.read(reinterpret_cast<char*>(&tmpData.seed), sizeof(seed_type));

        // Read how many sequences refer to this seed
        file.read(reinterpret_cast<char*>(&seq_idxs), sizeof(seq_idxs));
        cout << "Qnt " << seq_idxs << endl;
        
        for (size_t j = 0; j < seq_idxs; j++) {
            int idx;
            file.read(reinterpret_cast<char*>(&idx), sizeof(idx));
            // cout << "Idx " << idx << endl;
            tmpData.seq_idx.push_back(idx);
        }

        // Check if reading was successful
        if (!file) {
            if (file.eof()) {
                // End of file reached
                cerr << "End of file reached while reading seeds." << endl;
                break;
            } else {
                cerr << "Error reading from the file." << endl;
                return -1;
            }
        }
        
        data.push_back(tmpData);
        tmpData = SeedSource();
    }

    // Close the file
    file.close();
    return 0;
}

int main(int argc, char *argv[]) {
    vector<SeedSource> dump;
    vector<string> sequences;

    string filename_dump = argv[1];

    load_seeds(filename_dump, sequences, dump);

    // Print total number of reads
    cout << "Total number of seeds: " << dump.size() << endl;
    // Print first 10 reads
    for (size_t i = 0; i < 10 && i < dump.size(); i++) {
        cout << "Seed: " << dump[i].seed.first.data << endl;
        cout << "Sequences: " << dump[i].seq_idx.size() << endl;
        for (auto seq : dump[i].seq_idx) {
            cout << sequences[seq] << endl;
        }
    }

    return 0;
}