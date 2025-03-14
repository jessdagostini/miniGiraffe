#include <iostream>
#include <fstream>
#include <gbwtgraph/gbz.h>
#include <gbwtgraph/cached_gbwtgraph.h>
#include <queue>
#include <omp.h>
#include <cstdlib>
#include <unistd.h>

// For performance monitoring
#include "time-utils.h"
#include "perf-event.hpp"
#include "perf-utils.h"

#include <atomic>
#include <thread>
#include <time.h>
#include "IOQueue.h"

#ifdef USE_UNORDERED_SET
#include <unordered_set>
#elif USE_SET
#include <set>
#else
#include <vector>
#endif

using namespace std;

// typedef struct { char data[sizeof(long long int)]; } handle_t;
typedef pair<gbwtgraph::handle_t, int64_t> seed_type;

#ifdef USE_UNORDERED_SET
typedef unordered_set<seed_type> pair_hash_set;
#elif USE_SET
typedef set<seed_type> pair_hash_set;
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
const uint64_t DEFAULT_PARALLEL_BATCHSIZE = 512;
const uint32_t INFINITY_MISMATCHES = 1000000;
bool profile = false;
bool hw_counters = false;
PerfEvent e;

IOQueue Q[150];
atomic_int finished_threads(0);

struct Source {
    string sequence;
    pair_hash_set seeds;
};

struct GaplessExtension {
     // In the graph.
    vector<gbwtgraph::handle_t>     path;
    size_t                          offset;
    gbwt::BidirectionalState        state;

    // In the read.
    pair<size_t, size_t>            read_interval; // where to start when going backward (first) and forward (second) 
    vector<size_t>                  mismatch_positions;

    // Alignment properties.
    int32_t                         score;
    bool                            left_full, right_full;

    // For internal use.
    bool                            left_maximal, right_maximal;
    uint32_t                        internal_score; // Total number of mismatches.
    uint32_t                        old_score;      // Mismatches before the current flank.

    bool full() const { return (this->left_full & this->right_full); }

    size_t length() const { return this->read_interval.second - this->read_interval.first; }

    bool empty() const { return (this->length() == 0); }
    
    bool operator<(const GaplessExtension& another) const {
        return (this->score < another.score);
    }

    bool operator==(const GaplessExtension& another) const {
        return (this->read_interval == another.read_interval && this->state == another.state && this->offset == another.offset);
    }

    bool operator!=(const GaplessExtension& another) const {
        return !(this->operator==(another));
    }

    bool contains(const gbwtgraph::HandleGraph& graph, seed_type seed, size_t node_offset, size_t read_offset) const {
        gbwtgraph::handle_t expected_handle = seed.first;
        size_t expected_node_offset = node_offset;
        size_t expected_read_offset = read_offset;

        size_t new_read_offset = this->read_interval.first;
        size_t new_node_offset = this->offset;
        for (gbwtgraph::handle_t handle : this->path) {
            size_t len = graph.get_length(handle) - new_node_offset;
            new_read_offset += len;
            new_node_offset += len;
            if (handle == expected_handle && new_read_offset - expected_read_offset == new_node_offset - expected_node_offset) {
                return true;
            }
            new_node_offset = 0;
        }

        return false;
    }

    size_t overlap(const gbwtgraph::GBWTGraph& graph, const GaplessExtension& another) const {
        size_t result = 0;
        size_t this_pos = this->read_interval.first, another_pos = another.read_interval.first;
        auto this_iter = this->path.begin(), another_iter = another.path.begin();
        size_t this_offset = this->offset, another_offset = another.offset;
        while (this_pos < this->read_interval.second && another_pos < another.read_interval.second) {
            if (this_pos == another_pos && *this_iter == *another_iter && this_offset == another_offset) {
                size_t len = min({ graph.get_length(*this_iter) - this_offset,
                                        this->read_interval.second - this_pos,
                                        another.read_interval.second - another_pos });
                result += len;
                this_pos += len;
                another_pos += len;
                ++this_iter;
                ++another_iter;
                this_offset = 0;
                another_offset = 0;
            } else if (this_pos <= another_pos) {
                this_pos += graph.get_length(*this_iter) - this_offset;
                ++this_iter;
                this_offset = 0;
            } else {
                another_pos += graph.get_length(*another_iter) - another_offset;
                ++another_iter;
                another_offset = 0;
            }
        }
        return result;
    }

    // Use this function to copy another extension object
    void clone(const GaplessExtension& another) {
        this->offset = another.offset;
        this->state = another.state;
        this->read_interval = another.read_interval;
        this->score = another.score;
        this->left_full = another.left_full;
        this->right_full = another.right_full;
        this->left_maximal = another.left_maximal;
        this->right_maximal = another.right_maximal;
        this->internal_score = another.internal_score;
        this->old_score = another.old_score;
    }
};

struct ExtensionResult {
    string sequence;
    vector<GaplessExtension> extensions;
};

/// Get the node offset from a seed.
static size_t get_node_offset(seed_type seed) {
    return (seed.second < 0 ? -(seed.second) : 0);
}

/// Get the read offset from a seed.
static size_t get_read_offset(seed_type seed) {
    return (seed.second < 0 ? 0 : seed.second);
}

enum class HandlePosition {
    Forward,
    Backward
};

vector<gbwtgraph::handle_t> get_path(const vector<gbwtgraph::handle_t>& vec, gbwtgraph::handle_t handle, HandlePosition pos) {
    vector<gbwtgraph::handle_t> result;
    result.reserve(vec.size() + 1);

    if (pos == HandlePosition::Backward) {
        result.push_back(handle);
        result.insert(result.end(), vec.begin(), vec.end());
    } else {
        result.insert(result.end(), vec.begin(), vec.end());
        result.push_back(handle);
    }

    return result;
}

// Match forward but stop before the mismatch count reaches the limit.
// Updates internal_score; use set_score() to recompute score.
// Returns the tail offset (the number of characters matched).
size_t match_forward(GaplessExtension& match, const string& seq, gbwtgraph::view_type target, uint32_t mismatch_limit) {
    size_t node_offset = (mismatch_limit == INFINITY_MISMATCHES) ? match.offset : 0; // if I pass an infinity mismatch limit, I know that I need an node_offset using match.offset
    size_t left = min(seq.length() - match.read_interval.second, target.second - node_offset);

    while (left > 0) {
        size_t len = min(left, sizeof(uint64_t));
        uint64_t a = 0, b = 0;
        memcpy(&a, seq.data() + match.read_interval.second, len);
        memcpy(&b, target.first + node_offset, len);
        if (a == b) {
            match.read_interval.second += len;
            node_offset += len;
        } else {
            for (size_t i = 0; i < len; i++) {
                if (seq[match.read_interval.second] != target.first[node_offset]) {
                    if (match.internal_score + 1 >= mismatch_limit) { // if we passed the mismatch limit, return
                        return node_offset;
                    }
                    match.internal_score++;
                }
                match.read_interval.second++;
                node_offset++;
            }
        }
        left -= len;
    }

    if (mismatch_limit == INFINITY_MISMATCHES) match.old_score = match.internal_score;
    return node_offset;
}

void match_backward(GaplessExtension& match, const string& seq, gbwtgraph::view_type target, uint32_t mismatch_limit) {
    size_t left = min(match.read_interval.first, match.offset);
    while (left > 0) {
        size_t len = min(left, sizeof(uint64_t));
        uint64_t a = 0, b = 0;
        memcpy(&a, seq.data() + match.read_interval.first - len, len);
        memcpy(&b, target.first + match.offset - len, len);
        if (a == b) {
            match.read_interval.first -= len;
            match.offset -= len;
        } else {
            for (size_t i = 0; i < len; i++) {
                if (seq[match.read_interval.first - 1] != target.first[match.offset - 1]) {
                    if (match.internal_score + 1 >= mismatch_limit) {
                        return;
                    }
                    match.internal_score++;
                }
                match.read_interval.first--;
                match.offset--;
            }
        }
        left -= len;
    }
}

// Sort full-length extensions by internal_score, remove ones that are not
// full-length alignments, remove duplicates, and return the best extensions
// that have sufficiently low overlap.
void handle_full_length(const gbwtgraph::GBWTGraph& graph, vector<GaplessExtension>& result, double overlap_threshold) {
    sort(result.begin(), result.end(), [](const GaplessExtension& a, const GaplessExtension& b) -> bool {
        if (a.full() && b.full()) {
            return (a.internal_score < b.internal_score);
        }
        return a.full();
    });

    size_t tail = 0;
    for (size_t i = 0; i < result.size(); i++) {
        if (!(result[i].full())) {
            break; // No remaining full-length extensions.
        }
        bool overlap = false;
        for (size_t prev = 0; prev < tail; prev++) {
            if (result[i].overlap(graph, result[prev]) > overlap_threshold * result[prev].length()) {
                overlap = true;
                break;
            }
        }
        if (overlap) {
            continue;
        }
        if (i > tail) {
            result[tail] = move(result[i]);
        }
        tail++;
    }
    result.resize(tail);
}

// Sort the extensions from left to right. Remove duplicates and empty extensions.
void remove_duplicates(vector<GaplessExtension>& result) {
    auto sort_order = [](const GaplessExtension& a, const GaplessExtension& b) -> bool {
        if (a.read_interval != b.read_interval) {
            return (a.read_interval < b.read_interval);
        }
        if (a.state.backward.node != b.state.backward.node) {
            return (a.state.backward.node < b.state.backward.node);
        }
        if (a.state.forward.node != b.state.forward.node) {
            return (a.state.forward.node < b.state.forward.node);
        }
        if (a.state.backward.range != b.state.backward.range) {
           return (a.state.backward.range < b.state.backward.range);
        }
        if (a.state.forward.range != b.state.forward.range) {
           return (a.state.forward.range < b.state.forward.range);
        }
        return (a.offset < b.offset);
    };
    sort(result.begin(), result.end(), sort_order);

    size_t tail = 0;
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i].empty()) {
            continue;
        }
        if (tail == 0 || result[i] != result[tail - 1]) {
            if (i > tail) {
                result[tail] = move(result[i]);
            }
            tail++;
        }
    }
    result.resize(tail);
}

// Realign the extensions to find the mismatching positions.
void find_mismatches(const string& seq, const gbwtgraph::GBWTGraph& graph, vector<GaplessExtension>& result) {
    for (GaplessExtension& extension : result) {
        if (extension.internal_score == 0) {
            continue;
        }
        extension.mismatch_positions.reserve(extension.internal_score);
        size_t node_offset = extension.offset, read_offset = extension.read_interval.first;
        for (const gbwtgraph::handle_t& handle : extension.path) {
            gbwtgraph::view_type target = graph.get_sequence_view(handle);
            while (node_offset < target.second && read_offset < extension.read_interval.second) {
                if (target.first[node_offset] != seq[read_offset]) {
                    extension.mismatch_positions.push_back(read_offset);
                }
                node_offset++;
                read_offset++;
            }
            node_offset = 0;
        }
    }
}

// Compute the score based on read_interval, internal_score, left_full, and right_full.
void set_score(GaplessExtension& extension) {
    // Assume that everything matches.
    extension.score = static_cast<int32_t>((extension.read_interval.second - extension.read_interval.first) * default_match);
    // Handle the mismatches.
    extension.score -= static_cast<int32_t>(extension.internal_score * (default_match + default_mismatch));
    // Handle full-length bonuses.
    extension.score += static_cast<int32_t>(extension.left_full * default_full_length_bonus);
    extension.score += static_cast<int32_t>(extension.right_full * default_full_length_bonus);
}

bool process_next_state(const gbwt::BidirectionalState& next_state, GaplessExtension& curr, string sequence, const gbwtgraph::GBWTGraph* graph, uint32_t mismatch_limit, priority_queue<GaplessExtension>& extensions, size_t& num_extensions, bool& found_extension, HandlePosition direction) {
    GaplessExtension next;
    next.clone(curr);
    next.state = next_state;

    gbwtgraph::handle_t handle;
    size_t node_offset = 0;

    // Check if we are going right or left
    if (direction == HandlePosition::Forward) { // Right
        handle = gbwtgraph::GBWTGraph::node_to_handle(next_state.forward.node);

        node_offset = match_forward(next, sequence, graph->get_sequence_view(handle), mismatch_limit);

        if (node_offset == 0) return true;

        // Did the extension become right-maximal?
        if (next.read_interval.second >= sequence.length()) {
            next.right_full = true;
            next.right_maximal = true;
            next.old_score = next.internal_score;
        } else if (node_offset < graph->get_length(handle)) {
            next.right_maximal = true;
            next.old_score = next.internal_score;
        }
    } else { // Left
        // printf("Entrou left\n");
        handle = gbwtgraph::GBWTGraph::node_to_handle(gbwt::Node::reverse(next_state.backward.node));

        // If we are going to left, need to change next.offse
        next.offset = graph->get_length(handle);

        match_backward(next, sequence, graph->get_sequence_view(handle), mismatch_limit);

        if(next.offset >= graph->get_length(handle)) return true;

         // Did the extension become left-maximal?
        if (next.read_interval.first == 0) {
            next.left_full = true;
            next.left_maximal = true;
            // No need to set old_score.
        } else if (next.offset > 0) {
            next.left_maximal = true;
        }
    }

    next.path = get_path(curr.path, handle, direction);

    set_score(next);
    if (direction == HandlePosition::Forward) {
        num_extensions += next.state.size();
    } else {
        found_extension = true;
    }

    extensions.push(move(next));
    return true;
}

template<class Element>
void in_place_subvector(vector<Element>& vec, size_t head, size_t tail) {
    if (head >= tail || tail > vec.size()) {
        vec.clear();
        return;
    }
    if (head > 0) {
        for (size_t i = head; i < tail; i++) {
            vec[i - head] = move(vec[i]);
        }
    }
    vec.resize(tail - head);
}

bool trim_mismatches(GaplessExtension& extension, const gbwtgraph::GBWTGraph& graph) {

    if (extension.mismatch_positions.empty()) {
        return false;
    }

    // Start with the initial run of matches.
    auto mismatch = extension.mismatch_positions.begin();
    pair<size_t, size_t> current_interval(extension.read_interval.first, *mismatch);
    int32_t current_score = (current_interval.second - current_interval.first) * default_match;
    if (extension.left_full) {
        current_score += default_full_length_bonus;
    }

    // Process the alignment and keep track of the best interval we have seen so far.
    pair<size_t, size_t> best_interval = current_interval;
    int32_t best_score = current_score;
    while (mismatch != extension.mismatch_positions.end()) {
        // See if we should start a new interval after the mismatch.
        if (current_score >= default_mismatch) {
            current_interval.second++;
            current_score -= default_mismatch;
        } else {
            current_interval.first = current_interval.second = *mismatch + 1;
            current_score = 0;
        }
        ++mismatch;

        // Process the following run of matches.
        if (mismatch == extension.mismatch_positions.end()) {
            size_t length = extension.read_interval.second - current_interval.second;
            current_interval.second = extension.read_interval.second;
            current_score += length * default_match;
            if (extension.right_full) {
                current_score += default_full_length_bonus;
            }
        } else {
            size_t length = *mismatch - current_interval.second;
            current_interval.second = *mismatch;
            current_score += length * default_match;
        }

        // Update the best interval.
        if (current_score > best_score || (current_score > 0 && current_score == best_score && (current_interval.second - current_interval.first) > (best_interval.second - best_interval.first))) {
            best_interval = current_interval;
            best_score = current_score;
        }
    }

    // Special cases: no trimming or complete trimming.
    if (best_interval == extension.read_interval) {
        return false;
    }
    if ((best_interval.second - best_interval.first) == 0) {
        extension.path.clear();
        extension.read_interval = best_interval;
        extension.mismatch_positions.clear();
        extension.score = 0;
        extension.left_full = extension.right_full = false;
        return true;
    }

    // Update alignment statistics.
    bool path_changed = false;
    if (best_interval.first > extension.read_interval.first) {
        extension.left_full = false;
    }
    if (best_interval.second < extension.read_interval.second) {
        extension.right_full = false;
    }
    size_t node_offset = extension.offset, read_offset = extension.read_interval.first;
    extension.read_interval = best_interval;
    extension.score = best_score;

    // Trim the path.
    size_t head = 0;
    while (head < extension.path.size()) {
        size_t node_length = graph.get_length(extension.path[head]);
        read_offset += node_length - node_offset;
        node_offset = 0;
        if (read_offset > extension.read_interval.first) {
            extension.offset = node_length - (read_offset - extension.read_interval.first);
            break;
        }
        head++;
    }
    size_t tail = head + 1;
    while (read_offset < extension.read_interval.second) {
        read_offset += graph.get_length(extension.path[tail]);
        tail++;
    }
    if (head > 0 || tail < extension.path.size()) {
        in_place_subvector(extension.path, head, tail);
        extension.state = graph.bd_find(extension.path);
    }

    // Trim the mismatches.
    head = 0;
    while (head < extension.mismatch_positions.size() && extension.mismatch_positions[head] < extension.read_interval.first) {
        head++;
    }
    tail = head;
    while (tail < extension.mismatch_positions.size() && extension.mismatch_positions[tail] < extension.read_interval.second) {
        tail++;
    }
    in_place_subvector(extension.mismatch_positions, head, tail);

    return true;
}

void extend(string& sequence, pair_hash_set& seeds, const gbwtgraph::GBWTGraph* graph, int element_index, ExtensionResult* full_result) {
    // cout << "Extending " << sequence << endl;
    vector<GaplessExtension> result;
    result.reserve(seeds.size());

    size_t best_alignment = numeric_limits<size_t>::max(); // Metric to find the best extension from each seed        
    for (seed_type seed : seeds) {
        
        size_t node_offset = (seed.second < 0) ? -(seed.second) : 0;
        size_t read_offset = (seed.second < 0) ? 0 : seed.second;

        // Check if the seed is contained in an exact full-length alignment.
        if (best_alignment < result.size() && result[best_alignment].internal_score == 0) {
            if (result[best_alignment].contains(*graph, seed, node_offset, read_offset)) {
                continue;
            }
        }

        GaplessExtension best_match {
            { }, static_cast<size_t>(0), gbwt::BidirectionalState(),
            { static_cast<size_t>(0), static_cast<size_t>(0) }, { },
            numeric_limits<int32_t>::min(), false, false,
            false, false, numeric_limits<uint32_t>::max(), numeric_limits<uint32_t>::max()
        };
        
        priority_queue<GaplessExtension> extensions;
        
        GaplessExtension match {
            { seed.first }, node_offset, graph->get_bd_state(seed.first),
            { read_offset, read_offset }, { },
            static_cast<int32_t>(0), false, false,
            false, false, static_cast<uint32_t>(0), static_cast<uint32_t>(0)
        };
        
        size_t nd_of = match_forward(match, sequence, graph->get_sequence_view(seed.first), INFINITY_MISMATCHES);
        if (match.read_interval.first == 0) {
            match.left_full = true;
            match.left_maximal = true;
        }
        if (match.read_interval.second >= sequence.length()) {
            match.right_full = true;
            match.right_maximal = true;
        }
        set_score(match);
        extensions.push(move(match));

        while (!extensions.empty()) {
            GaplessExtension curr = move(extensions.top());
            extensions.pop();

            // Always allow at least max_mismatches / 2 mismatches in the current flank.
            uint32_t mismatch_limit = max(
                static_cast<uint32_t>(MAX_MISMATCHES + 1),
                static_cast<uint32_t>(MAX_MISMATCHES / 2 + curr.old_score + 1));

            size_t num_extensions = 0;
            bool found_extension = false;

            // Case 1: Extend to the right.
            if (!curr.right_maximal) {
                
                graph->follow_paths(curr.state, false, [&](const gbwt::BidirectionalState& next_state) -> bool {
                    return process_next_state(next_state, curr, sequence, graph, mismatch_limit, extensions, num_extensions, found_extension, HandlePosition::Forward);
                });
                
                // We could not extend all threads in 'curr' to the right. The unextended ones
                // may have different left extensions, so we must consider 'curr' right-maximal.
                if (num_extensions < curr.state.size()) {
                    curr.right_maximal = true;
                    curr.old_score = curr.internal_score;
                    extensions.push(move(curr));
                }
                continue;
            }

            // Case 2: Extend to the left.
            if (!curr.left_maximal) {

                graph->follow_paths(curr.state, true, [&](const gbwt::BidirectionalState& next_state) -> bool {
                    return process_next_state(next_state, curr, sequence, graph, mismatch_limit, extensions, num_extensions, found_extension, HandlePosition::Backward);
                });

                if (!found_extension) {
                    curr.left_maximal = true;
                    // No need to set old_score.
                } else {
                    continue;
                }
            }

            // Case 3: Maximal extension with a better score than the best extension so far.
            if (best_match < curr) {
                best_match = move(curr);
            }
        }

        // Add the best match to the result and update the best_alignment offset.
        if (!best_match.empty()) {
            if (best_match.full() && (best_alignment >= result.size() || best_match.internal_score < result[best_alignment].internal_score)) {
                best_alignment = result.size();
            }
            result.emplace_back(move(best_match));
        }
    }

    // If we have a good enough full-length alignment, return the best sufficiently
    // distinct full-length alignments.
    // size_t max_mismatches = 100;
    if (best_alignment < result.size() && result[best_alignment].internal_score <= MAX_MISMATCHES) {
        handle_full_length(*graph, result, OVERLAP_THRESHOLD);
        find_mismatches(sequence, *graph, result);
    }

    // Otherwise remove duplicates, find mismatches, and trim the extensions to maximize
    // score.
    else {
        remove_duplicates(result);
        find_mismatches(sequence, *graph, result);
        bool trimmed = false;
        for (GaplessExtension& extension : result) {
            trimmed |= trim_mismatches(extension, *graph);
        }
        if (trimmed) {
            // cout << "After trimmed" << endl;
            remove_duplicates(result);
        }
    }

    // Free the cache if we allocated it.
    // if (free_cache) {
    //     delete cache;
    //     cache = nullptr;
    // }
    
    full_result[element_index].sequence = sequence;
    full_result[element_index].extensions = result;
} 

void write_extensions(ExtensionResult* results, int size) {
    // Specify the file name
    time_t now = time(0);

    // convert now to string form
    char buffer[80];
    strftime(buffer, 80, "%Y%m%d%H%M", localtime(&now));
    string date_time(buffer);

    string fileName = "./proxy_extensions_" + date_time + ".bin";

    // Create an output file stream
    ofstream outFile(fileName, ios::binary | ios::app);

    if (!outFile) {
        cerr << "Error opening file: " << fileName << endl;
    }

    ExtensionResult r;

    for(int i=0; i<size; i++) {
        // Write string to the file
        r = results[i];
        outFile.write(r.sequence.c_str(), r.sequence.size() + 1); // Include null terminator

        // Write how many extensions we have
        size_t extensions = r.extensions.size();
        outFile.write(reinterpret_cast<const char*>(&extensions), sizeof(extensions));
        
        for (auto& e : r.extensions) {
            // Node offset
            outFile.write(reinterpret_cast<const char*>(&e.offset), sizeof(e.offset));

            // Read Interval
            outFile.write(reinterpret_cast<const char*>(&e.read_interval), sizeof(e.read_interval));

            //Score
            outFile.write(reinterpret_cast<const char*>(&e.score), sizeof(e.score));

            // Write how many mismatches
            size_t mismatches = e.mismatch_positions.size();
            outFile.write(reinterpret_cast<const char*>(&mismatches), sizeof(mismatches));

            // if (mismatches > 1000) {
            //     cout << mismatches << endl;
            // }

            // Mismatches
            if (mismatches > 0) {
                for (size_t i = 0; i < mismatches; i++) {
                    outFile.write(reinterpret_cast<const char*>(&e.mismatch_positions[i]), sizeof(e.mismatch_positions[i]));
                }
            }
        }
    }
    
    // Close the file stream
    outFile.close();
}

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
#ifdef USE_UNORDERED_SET
            tmpData.seeds.insert(tmp);
#elif USE_SET
            tmpData.seeds.insert(tmp);
#else
            tmpData.seeds.push_back(tmp);
#endif
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

double get_wall_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time); // or CLOCK_REALTIME
    return time.tv_sec + time.tv_nsec / 1000000000.0;
}

void dist_workload(int chunk_size, int size, int tid, int num_threads) {
  int start = chunk_size * tid;
  int end = min(start + chunk_size, size); // Make sure we don't index out of array.
  for (int i = start; i < end; i++) {
    Q[tid].enq(i); // Enqueue index as work item for local worklist.
  }
}

void work_stealing(vector<Source>& data, const gbwtgraph::GBWTGraph* graph, ExtensionResult* full_result, int batchsize, int tid, int num_threads) {
    double start, end;
    int* batch = (int*)malloc(batchsize * sizeof(int));
    for (int r = Q[tid].deq_batch(batch, batchsize); r != -1; r = Q[tid].deq_batch(batch, batchsize)) {
        for (int j=0; j<batchsize; j++) {
            int i = batch[j];
            if (profile) start = get_wall_time();
            extend(data[i].sequence, data[i].seeds, graph, i, full_result);
            if (profile) {
                end = get_wall_time();
                time_utils_add(start, end, TimeUtilsRegions::SEEDS_LOOP, tid);
            }
        }
    }

    // printf("Thread %d finished it's local workload!\n", tid);
    finished_threads.fetch_add(1);

    // Steal tasks from other worklists.
    int target = num_threads - ((tid + 1) % num_threads);
    while (finished_threads.load() != num_threads) {
        if (target == tid) {
            target = (target + 1) % num_threads;
        }
        // printf("Thread %d helping thread %d\n", tid, target);
        int r;
        while ((r = Q[target].deq_batch(batch, batchsize)) != -1) {
            for(int j=0; j<batchsize; j++) {
                int i = batch[j];
                if (profile) start = get_wall_time();
                extend(data[i].sequence, data[i].seeds, graph, i, full_result);
                if (profile) {
                    end = get_wall_time();
                    time_utils_add(start, end, TimeUtilsRegions::SEEDS_LOOP, tid);
                }
            }
        }

        target = (target + 1) % num_threads; // Round robin.
    }
}

int setup_counters(string optarg) {
    istringstream ss(optarg);
    string token;
    while (getline(ss, token, ',')) {
        if (token == "IPC") {
            e.registerCounter("instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
            e.registerCounter("cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
            e.nameToIndex["instructions"] = PerfUtilsCounters::INSTRUCTIONS;
            e.nameToIndex["cycles"] = PerfUtilsCounters::CYCLES;
        
        } else if (token == "L1CACHE") {
            e.registerCounter("L1-access", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_ACCESS<<16));
            e.registerCounter("L1-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_MISS<<16));
            e.nameToIndex["L1-access"] = PerfUtilsCounters::L1ACCESS;
            e.nameToIndex["L1-misses"] = PerfUtilsCounters::L1MISSES;
        
        } else if (token == "LLCACHE") {
            e.registerCounter("LLC-access", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_ACCESS<<16));
            e.registerCounter("LLC-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_MISS<<16));
            e.nameToIndex["LLC-access"] = PerfUtilsCounters::LLCACCESS;
            e.nameToIndex["LLC-misses"] = PerfUtilsCounters::LLCMISSES;
        
        } else if (token == "BRANCHES") {
            e.registerCounter("branch-issued", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
            e.registerCounter("branch-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);      
            e.nameToIndex["branch-issued"] = PerfUtilsCounters::BRANCHISSUED;
            e.nameToIndex["branch-misses"] = PerfUtilsCounters::BRANCHMISSES;
        
        } else if (token == "DTLB") {
            e.registerCounter("DTLB-access", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_DTLB|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_ACCESS<<16));
            e.registerCounter("DTLB-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_DTLB|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_MISS<<16));  
            e.nameToIndex["DTLB-access"] = PerfUtilsCounters::DTLBACCESS;
            e.nameToIndex["DTLB-misses"] = PerfUtilsCounters::DTLBMISSES;
        
        } else if (token == "ITLB") {
            e.registerCounter("ITLB-access", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_ITLB|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_ACCESS<<16));
            e.registerCounter("ITLB-misses", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_ITLB|(PERF_COUNT_HW_CACHE_OP_READ<<8)|(PERF_COUNT_HW_CACHE_RESULT_MISS<<16));  
            e.nameToIndex["ITLB-access"] = PerfUtilsCounters::ITLBACCESS;
            e.nameToIndex["ITLB-misses"] = PerfUtilsCounters::ITLBMISSES;
        
        } else {
            std::cerr << "Error: Invalid counter option: " << token << std::endl;
            // You could either exit here or continue processing other options.
            // I'm choosing to exit for cleaner error handling.
            return 1;
        }
    }
    return 0;
}

void usage() {
    cerr << "Usage miniGiraffe [seed_file] [gbz_file] [options]" << endl;
    cerr << "Options: " << endl;
    cerr << "   -t, number of threads (default: max # threads in system)" << endl;
    cerr << "   -b, batch size (default: 512)" << endl;
    cerr << "   -s, scheduler [omp, ws] (default: omp)" << endl;
    cerr << "   -p, enable profiling (default: disabled)" << endl;
    cerr << "   -m <list>, comma-separated list of hardware measurement to enable (default: disabled)" << endl;
    cerr << "              Available counters: IPC, L1CACHE, LLCACHE, BRANCHES, DTLB, ITLB" << std::endl;
    cerr << "              Not recommended to enable more than 3 hw measurement per run" << std::endl;
    cerr << "              given hardware counters constraints" << std::endl;
}

int main(int argc, char *argv[]) {
    vector<Source> data;
    gbwtgraph::GBZ gbz;
    int num_threads = omp_get_max_threads();
    int batch_size = DEFAULT_PARALLEL_BATCHSIZE;
    string scheduler = "omp";
    const gbwtgraph::GBWTGraph* graph;
    int opt;
    double start, end; // For profiling
    
    if(argc < 3) { usage(); return 0;}

    string filename_dump = argv[1];
    string filename_gbz = argv[2];
    

    while ((opt = getopt(argc, argv, "hpm:t:s:b:")) != -1) {
        switch (opt) {
            case 'h':
                usage();
                return 0;
            case 'p':
                profile = true;
                break;
            case 'm': {
                hw_counters = true;
                if (setup_counters(optarg) == 1) {
                    std::cerr << "Error: Invalid counter option.\n";
                    return 1;
                }
                break;
            }
            case 't':
                try {
                    num_threads = stoi(optarg);
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Error: Option -t requires an integer value.\n";
                    return 1;
                }
                break;
            case 's':
                try {
                    scheduler = optarg;
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Error: Option -t requires an integer value.\n";
                    return 1;
                }
                break;
            case 'b':
                try {
                    batch_size = atoi(optarg);
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Error: Option -b requires an integer value.\n";
                    return 1;
                }
                break;
            case '?': // getopt detects unknown options and sets opt to '?'
                std::cerr << "Unknown option: -" << (char)optopt << std::endl; // optopt has the unknown option char
                // fallthrough
                usage();
                break;
            default: // For '?' and potentially other errors
                usage();
                return 1;
        }
    }

    cout << "Reading seeds " << filename_dump << endl;
    if (profile) start = get_wall_time();
    if (load_seeds(filename_dump, data) == -1) { usage(); return -1;}
    if (profile) {
        double end = get_wall_time();
        time_utils_add(start, end, TimeUtilsRegions::READING_SEEDS, omp_get_thread_num());
    }
    
    cout << "Reading GBZ" << endl;
    try {
        if (profile) start = get_wall_time();
        sdsl::simple_sds::load_from(gbz, filename_gbz);
        graph = &gbz.graph;
        if (profile) {
            double end = get_wall_time();
            time_utils_add(start, end, TimeUtilsRegions::READING_GBZ, omp_get_thread_num());
        }
    } catch(const std::exception& e) {
        cerr << "Error reading GBZ file" << endl;
        usage();
        return -1;
    }

    int size = data.size();
    ExtensionResult* full_result = new ExtensionResult[size];

    cout << "Starting mapping with " << num_threads << " and batch size " << batch_size << " with " << scheduler << " scheduler." << endl;
    if (scheduler == "omp") {
        // OpenMP Scheduler
        omp_set_num_threads(num_threads);
        if (hw_counters) e.startCounters();
        #pragma omp parallel for shared(graph, full_result) schedule(dynamic, batch_size)
        for (int i = 0; i < size; i++) {
            double start, end;
            if (profile) start = get_wall_time();
            extend(data[i].sequence, data[i].seeds, graph, i, full_result);
            if (profile) {
                double end = get_wall_time();
                time_utils_add(start, end, TimeUtilsRegions::SEEDS_LOOP, omp_get_thread_num());
            }
        }
        if (hw_counters){
            e.stopCounters();
            for (unsigned j=0; j<e.names.size(); j++) {
                string name = e.names[j];
                perf_utils_add(e.events[j].readCounter(), e.nameToIndex[name], omp_get_thread_num());
            }
        }
    } else {
        // Work-stealing scheduler
        thread* thread_array = new thread[num_threads];
        int chunk_size = (size + (num_threads - 1)) / num_threads;
        for (int i = 0; i < num_threads; i++) {
            // Initialize local worklists.
            Q[i].init(chunk_size);

            // Launch threads to enqueue to local worklists.
            thread_array[i] = thread(dist_workload, chunk_size, size, i, num_threads);
        }

        // Join.
        for (int i = 0; i < num_threads; i++) {
            thread_array[i].join();
        }

        if (hw_counters) e.startCounters();
        // Spawn threads to carry out work.
        for (int i = 0; i < num_threads; i++) {
            thread_array[i] = thread(work_stealing, ref(data), graph, full_result, batch_size, i, num_threads);
        }
        if (hw_counters){
            e.stopCounters();
            for (unsigned j=0; j<e.names.size(); j++) {
                string name = e.names[j];
                perf_utils_add(e.events[j].readCounter(), e.nameToIndex[name], omp_get_thread_num());
            }
        }

        // Join.
        for (int i = 0; i < num_threads; i++) {
            thread_array[i].join();
        }
    }

    cout << "Finished mapping" << endl;
    cout << "Writing extensions" << endl;
    if (profile) start = get_wall_time();
    write_extensions(full_result, size);
    if (profile) {
        double end = get_wall_time();
        time_utils_add(start, end, TimeUtilsRegions::WRITING_OUTPUT, omp_get_thread_num());
    }

    if (profile) time_utils_dump();
    if (hw_counters) perf_utils_dump();

    return 0;
}
