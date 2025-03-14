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
    string giraffe_dump = argv[1];
    string proxy_dump = argv[2];

    // cout << giraffe_dump << endl;
    // cout << proxy_dump << endl;

    vector<GaplessExtension> giraffe_data;
    vector<GaplessExtension> proxy_data;

    unordered_map<string, vector<GaplessExtension>> giraffe_hash;
    unordered_map<string, vector<GaplessExtension>> proxy_hash;
    unordered_map<string, int64_t> count_sequences_repetition;
    unordered_map<string, int64_t> count_sequences_match;
    unordered_map<string, int64_t> count_sequences_mismatch;

    load_data(giraffe_dump, giraffe_data);
    load_data(proxy_dump, proxy_data);

    cout << "Giraffe" << endl;
    cout << giraffe_data.size() << endl;
    for(auto gd: giraffe_data) {
        // cout << "Sequence: " << gd.sequence << endl;
        // cout << "Offset: " << gd.offset << " Score: " << gd.score << " Read Interval: <" << gd.read_interval.first << " " << gd.read_interval.second << ">" << endl;
        giraffe_hash[gd.sequence].push_back(gd);
        if (count_sequences_repetition.find(gd.sequence) == count_sequences_repetition.end()) {
            count_sequences_repetition[gd.sequence] = 1;
        } else {
            count_sequences_repetition[gd.sequence]++;
        }
    }

    giraffe_data.clear();

    // cout << endl;
    // cout << "Giraffe Hash" << endl;
    // cout << giraffe_hash.size() << endl;
    // for(auto gd: giraffe_data) {
    //     cout << "Sequence: " << gd.sequence << endl;
    //     auto hash = giraffe_hash[gd.sequence];
    //     for (auto g: hash) {
    //         cout << "Offset: " << g.offset << " Score: " << g.score << " Read Interval: <" << g.read_interval.first << " " << g.read_interval.second << ">" << endl;
    //     }
    //     cout << endl;
    // }

    cout << "Proxy" << endl;
    cout << proxy_data.size() << endl;
    bool found = false;
    int teste = 0;
    for(auto gd: proxy_data) {
        // cout << "Sequence: " << gd.sequence << endl;
        // cout << "Offset: " << gd.offset << " Score: " << gd.score << " Read Interval: <" << gd.read_interval.first << " " << gd.read_interval.second << ">" << endl;
        if (giraffe_hash.find(gd.sequence) == giraffe_hash.end()) {
            cout << gd.sequence << " not found\n\n";
        } else {
            auto hash_data = giraffe_hash[gd.sequence];
            for(auto h: hash_data) {
                if (found) {
                    break;
                }
                // cout << ++teste << endl;
                if (h.offset == gd.offset and h.score == gd.score and h.read_interval.first == gd.read_interval.first and h.read_interval.second == gd.read_interval.second) {
                    // cout << "Match" << endl;
                    // cout << "Sequence: " << gd.sequence << endl;
                    if (count_sequences_match.find(gd.sequence) == count_sequences_match.end()) {
                        count_sequences_match[gd.sequence] = 1;
                    } else {
                        count_sequences_match[gd.sequence]++;
                    }
                    found = true;
                    // cout << "Offset: " << gd.offset << " Score: " << gd.score << " Read Interval: <" << gd.read_interval.first << " " << gd.read_interval.second << ">" << endl;
                }
            }
            // teste = 0;

            if (!found) {
                // cout << "Proxy has found a different match for " << gd.sequence << endl;
                // vector<size_t>::iterator it;
                // cout << "Proxy: " << gd.sequence << " " << gd.offset << " " << gd.score << " " << gd.read_interval.first << " " << gd.read_interval.second << endl;
                // for(it = gd.mismatch_positions.begin(); it != gd.mismatch_positions.end(); it++) {
                //         cout << *it << " ";
                // }
                // for(auto h: hash_data) {
                //     cout << "Giraffe: " << h.sequence << " " << h.offset << " " << h.score << " " << h.read_interval.first << " " << h.read_interval.second << endl;
                //     vector<size_t>::iterator it;
                //     for(it = h.mismatch_positions.begin(); it != h.mismatch_positions.end(); it++) {
                //         cout << *it << " ";
                //     }
                //     cout << endl;
                // }
                // cout << endl;
                if (count_sequences_mismatch.find(gd.sequence) == count_sequences_mismatch.end()) {
                    count_sequences_mismatch[gd.sequence] = 1;
                } else {
                    count_sequences_mismatch[gd.sequence]++;
                }
            }

            found = false;
        }
    }

    unordered_map<string, int64_t>::iterator itr;
    int64_t matches, mismatches;
    cout << "Sequence, Expected, Matches, Mismatches" << endl;
    for (itr = count_sequences_repetition.begin(); itr != count_sequences_repetition.end(); itr++) {
        // Get total matches
        if (count_sequences_match.find(itr->first) == count_sequences_match.end()) {
            matches = 0;
        } else {
            matches = count_sequences_match[itr->first];
        }

        // Get total mismatches
        if (count_sequences_mismatch.find(itr->first) == count_sequences_mismatch.end()) {
            mismatches = 0;
        } else {
            mismatches = count_sequences_mismatch[itr->first];
        }
        if (mismatches > 0) {
            cout << itr->first << "," << itr->second << "," << matches << "," << mismatches << endl;
        }
    } 
     
}

