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

int main(int argc, char *argv[]) {
    vector<Source> dump;

    string filename_dump = argv[1];

    load_seeds(filename_dump, dump);

    for (auto d : dump) {
        for (auto seed : d.seeds) {
            d.new_seeds.push_back(new_seed_type(gbwtgraph::GBWTGraph::handle_to_node(seed.first), seed.second));
        }
    }

    sort(dump.begin(), dump.end(), [](const Source& a, const Source& b) -> bool {
        return a.sequence < b.sequence;
    });
    
    for (auto d : dump) {
        cout << "Sequence: " << d.sequence << endl;
        cout << "Seeds: " << d.seeds.size() << endl;
        for (auto seed : d.new_seeds) {
            cout << "Seed: " << seed.first 
                 << ", Position: " << seed.second << endl;
        }
    }
    return 0;
}