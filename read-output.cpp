#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>

using namespace std;

// hash[offset] = <vector>GaplessExtension
// for data in proxy_dump
    // data[offset]
    // d

struct GaplessExtension {
     // In the graph.
    size_t                    offset;
    
    // In the read.
    pair<size_t, size_t> read_interval; // where to start when going backward (first) and forward (second) 
    vector<size_t>       mismatch_positions;

    // Alignment properties.
    int32_t                   score;

    // Sequence
    string sequence;
};

void load_data(string filename, vector<GaplessExtension> &data){
    // Open the file in binary mode for reading
    ifstream file(filename, ios::binary);
    cout << "Reading " << filename << endl;

    if (!file.is_open()) {
        cerr << "Error opening the file " << filename << "!" << endl;
        return;
    }
    
    GaplessExtension e;
    string sequence;
    char ch;
    size_t extensions;

    int count = 0;

    while (!file.eof()) {
        while (file.get(ch) && ch != '\0') {
            sequence += ch;
        }

        // cout << sequence << endl;
        file.read(reinterpret_cast<char*>(&extensions), sizeof(extensions));

        for(size_t i=0; i < extensions; i++) {
            e.sequence = sequence; 
            file.read(reinterpret_cast<char*>(&e.offset), sizeof(e.offset));
            // cout << e.offset << endl;
            
            file.read(reinterpret_cast<char*>(&e.read_interval), sizeof(e.read_interval));
            // cout << e.read_interval.first << " " << e.read_interval.second << endl;
            
            file.read(reinterpret_cast<char*>(&e.score), sizeof(e.score));
            // cout << e.score << endl;
            
            size_t mismatches;
            file.read(reinterpret_cast<char*>(&mismatches), sizeof(mismatches));
            // cout << mismatches << endl;
            
            if (mismatches > 0) {
                size_t mismatch;
                for (size_t i = 0; i < mismatches; i++) {
                    file.read(reinterpret_cast<char*>(&mismatch), sizeof(mismatch));
                    // cout << mismatch << endl;
                    e.mismatch_positions.push_back(mismatch);
                }
            }

            data.push_back(e);
            // if (e.sequence.empty() || e.sequence.size() == 0) {
            //     cout << "EMPTY SEQUENCE" << endl;
            //     if (e.read_interval.first == 0 && e.read_interval.second == 0) {
            //         cout << "EMPTY READ INTERVAL" << endl;
            //     } else {
            //         cout << "NOT EMPTY READ INTERVAL" << endl;
            //     }
            //     if (e.score == 0) {
            //         cout << "EMPTY SCORE" << endl;
            //     } else {
            //         cout << "NOT EMPTY SCORE" << endl;
            //     }
            //     if (e.mismatch_positions.empty() || e.mismatch_positions.size() == 0) {
            //         cout << "EMPTY MISMATCHES" << endl;
            //     } else {
            //         cout << "NOT EMPTY MISMATCHES" << endl;
            //     }
            //     if (e.offset == 0) {
            //         cout << "EMPTY OFFSET" << endl;
            //     } else {
            //         cout << "NOT EMPTY OFFSET" << endl;
            //     }
            //     cout << count << endl;
            //     return;
            // }
            e = GaplessExtension();
        }
        sequence = "";

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

        count++;
        // if (count > 200) {
        //     break;
        // }
    }

    // Close the file
    file.close();
}

int main(int argc, char *argv[]) {
    string proxy_dump = argv[1];

    vector<GaplessExtension> proxy_data;

    load_data(proxy_dump, proxy_data);

    for (auto pd: proxy_data) {
        cout << pd.sequence << " " << pd.score << endl;
        cout << endl;
    }

    return 0;
}