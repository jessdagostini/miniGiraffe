#include <iostream>
#include <fstream>
#include <gbwtgraph/gbz.h>
#include <gbwtgraph/cached_gbwtgraph.h>
#include <queue>
#include <omp.h>
#include <cstdlib>

#include "time-utils.h"

// #include "perf-event.hpp"
// #include "perf-utils.h"

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
using namespace gbwtgraph;

// typedef struct { char data[sizeof(long long int)]; } handle_t;
typedef pair<handle_t, int64_t> seed_type;

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

IOQueue Q[50];
std::atomic_int finished_threads(0);

struct Source {
    string sequence;
    pair_hash_set seeds;
};

struct GaplessExtension {
    // For proxy
    // string sequence;
     // In the graph.
    std::vector<handle_t>     path;
    size_t                    offset;
    gbwt::BidirectionalState  state;

    // In the read.
    std::pair<size_t, size_t> read_interval; // where to start when going backward (first) and forward (second) 
    std::vector<size_t>       mismatch_positions;

    // Alignment properties.
    int32_t                   score;
    bool                      left_full, right_full;

    // For internal use.
    bool                      left_maximal, right_maximal;
    uint32_t                  internal_score; // Total number of mismatches.
    uint32_t                  old_score;      // Mismatches before the current flank.

    bool full() const { return (this->left_full & this->right_full); }

    size_t length() const { return this->read_interval.second - this->read_interval.first; }

    bool empty() const { return (this->length() == 0); }

    /// Is the extension an exact match?
    bool exact() const { return this->mismatch_positions.empty(); }
    
    bool operator<(const GaplessExtension& another) const {
        return (this->score < another.score);
    }

    bool operator==(const GaplessExtension& another) const {
        return (this->read_interval == another.read_interval && this->state == another.state && this->offset == another.offset);
    }

    bool operator!=(const GaplessExtension& another) const {
        return !(this->operator==(another));
    }

    bool contains(const HandleGraph& graph, seed_type seed, size_t node_offset, size_t read_offset) const {
        handle_t expected_handle = seed.first;
        size_t expected_node_offset = node_offset;
        size_t expected_read_offset = read_offset;

        size_t new_read_offset = this->read_interval.first;
        size_t new_node_offset = this->offset;
        for (handle_t handle : this->path) {
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
                size_t len = std::min({ graph.get_length(*this_iter) - this_offset,
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

vector<handle_t> get_path(const vector<handle_t>& first, handle_t second) {
    vector<handle_t> result;
    result.reserve(first.size() + 1);
    result.insert(result.end(), first.begin(), first.end());
    result.push_back(second);
    return result;
}

vector<handle_t> get_path(handle_t first, const vector<handle_t>& second) {
    vector<handle_t> result;
    result.reserve(second.size() + 1);
    result.push_back(first);
    result.insert(result.end(), second.begin(), second.end());
    return result;
}

vector<handle_t> get_path(const vector<handle_t>& first, gbwt::node_type second) {
    return get_path(first, gbwtgraph::GBWTGraph::node_to_handle(second));
}

vector<handle_t> get_path(gbwt::node_type reverse_first, const vector<handle_t>& second) {
    return get_path(gbwtgraph::GBWTGraph::node_to_handle(gbwt::Node::reverse(reverse_first)), second);
}

void match_initial(GaplessExtension& match, const std::string& seq, gbwtgraph::view_type target) {
    size_t node_offset = match.offset;
    size_t left = std::min(seq.length() - match.read_interval.second, target.second - node_offset);
    // cout << "Seq length: " << seq.length() << endl;
    // cout << "Read_interval.second: " << match.read_interval.second << endl;
    // cout << "Targer.second: " << target.second << endl;
    // cout << "Node offset: " << node_offset << endl;
    while (left > 0) {
        // cout << "Left: " << left << endl;
        size_t len = std::min(left, sizeof(std::uint64_t)); // Number of bytes to be copied
        std::uint64_t a = 0, b = 0; // 8 bytes
        std::memcpy(&a, seq.data() + match.read_interval.second, len); // Setting the pointer on seq.data to slot where to start and copying len bytes
        std::memcpy(&b, target.first + node_offset, len); // Setting the pointer in the node offset, copying len bytes
        // cout << a << " " << b << endl;
        if (a == b) { // byte comparision
            match.read_interval.second += len;
            node_offset += len;
        } else {
            for (size_t i = 0; i < len; i++) {
                // cout << "[INITIAL] Seq: " << seq[match.read_interval.second] << " Target: " << target.first[node_offset] << endl;
                if (seq[match.read_interval.second] != target.first[node_offset]) {
                    match.internal_score++;
                }
                match.read_interval.second++;
                node_offset++;
            }
        }
        left -= len;
    }
    match.old_score = match.internal_score;
}

// Match forward but stop before the mismatch count reaches the limit.
// Updates internal_score; use set_score() to recompute score.
// Returns the tail offset (the number of characters matched).
size_t match_forward(GaplessExtension& match, const std::string& seq, gbwtgraph::view_type target, uint32_t mismatch_limit) {
    size_t node_offset = 0;
    size_t left = std::min(seq.length() - match.read_interval.second, target.second - node_offset);
    while (left > 0) {
        size_t len = std::min(left, sizeof(std::uint64_t));
        std::uint64_t a = 0, b = 0;
        std::memcpy(&a, seq.data() + match.read_interval.second, len);
        std::memcpy(&b, target.first + node_offset, len);
        // cout << a << " " << b << endl;
        if (a == b) {
            match.read_interval.second += len;
            node_offset += len;
        } else {
            for (size_t i = 0; i < len; i++) {
                // cout << "[FORWARD] Seq: " << seq[match.read_interval.second] << " Target: " << target.first[node_offset] << endl;
                if (seq[match.read_interval.second] != target.first[node_offset]) {
                    if (match.internal_score + 1 >= mismatch_limit) {
                        // cout << "If: " << node_offset << std::endl;
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
    // cout << "Full return " << node_offset << std::endl;
    return node_offset;
}

void match_backward(GaplessExtension& match, const std::string& seq, gbwtgraph::view_type target, uint32_t mismatch_limit) {
    size_t left = std::min(match.read_interval.first, match.offset);
    while (left > 0) {
        size_t len = std::min(left, sizeof(std::uint64_t));
        std::uint64_t a = 0, b = 0;
        std::memcpy(&a, seq.data() + match.read_interval.first - len, len);
        std::memcpy(&b, target.first + match.offset - len, len);
        if (a == b) {
            match.read_interval.first -= len;
            match.offset -= len;
        } else {
            for (size_t i = 0; i < len; i++) {
                // cout << "[BACKWARD] Seq: " << seq[match.read_interval.first - 1] << " Target: " << target.first[match.offset] << endl;
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
void handle_full_length(const gbwtgraph::GBWTGraph& graph, std::vector<GaplessExtension>& result, double overlap_threshold) {
    std::sort(result.begin(), result.end(), [](const GaplessExtension& a, const GaplessExtension& b) -> bool {
        if (a.full() && b.full()) {
            return (a.internal_score < b.internal_score);
        }
        return a.full();
    });

    // Debug
    // cout << "handle_full_length" << endl;
    // for (size_t i = 0; i < result.size(); i++) {
    //     cout << result[i].offset << " " << result[i].score << " " << result[i].read_interval.first << " " << result[i].read_interval.second;
    //     vector<size_t>::iterator mit;
    //     for (mit = result[i].mismatch_positions.begin(); mit != result[i].mismatch_positions.end(); mit++) {
    //         cout << " " << *mit;
    //     }
    //     cout << endl;
    // }
    // cout << endl;

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
            result[tail] = std::move(result[i]);
        }
        tail++;
    }
    result.resize(tail);
}

// Sort the extensions from left to right. Remove duplicates and empty extensions.
void remove_duplicates(std::vector<GaplessExtension>& result) {
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
    std::sort(result.begin(), result.end(), sort_order);

    // Debug
    // cout << "remove_duplicates" << endl;
    // for (size_t i = 0; i < result.size(); i++) {
    //     cout << result[i].offset << " " << result[i].score << " " << result[i].read_interval.first << " " << result[i].read_interval.second;
    //     vector<size_t>::iterator mit;
    //     for (mit = result[i].mismatch_positions.begin(); mit != result[i].mismatch_positions.end(); mit++) {
    //         cout << " " << *mit;
    //     }
    //     cout << endl;
    // }

    size_t tail = 0;
    for (size_t i = 0; i < result.size(); i++) {
        if (result[i].empty()) {
            continue;
        }
        if (tail == 0 || result[i] != result[tail - 1]) {
            if (i > tail) {
                result[tail] = std::move(result[i]);
            }
            tail++;
        }
    }
    result.resize(tail);
}

// Realign the extensions to find the mismatching positions.
void find_mismatches(const std::string& seq, const gbwtgraph::GBWTGraph& graph, std::vector<GaplessExtension>& result) {
    for (GaplessExtension& extension : result) {
        if (extension.internal_score == 0) {
            continue;
        }
        extension.mismatch_positions.reserve(extension.internal_score);
        size_t node_offset = extension.offset, read_offset = extension.read_interval.first;
        for (const handle_t& handle : extension.path) {
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
    // cout << extension.score << endl;
    // cout << extension.internal_score << endl;
}

template<class Element>
void in_place_subvector(std::vector<Element>& vec, size_t head, size_t tail) {
    if (head >= tail || tail > vec.size()) {
        vec.clear();
        return;
    }
    if (head > 0) {
        for (size_t i = head; i < tail; i++) {
            vec[i - head] = std::move(vec[i]);
        }
    }
    vec.resize(tail - head);
}

bool trim_mismatches(GaplessExtension& extension, const gbwtgraph::GBWTGraph& graph) {

    if (extension.exact()) {
        return false;
    }

    // Start with the initial run of matches.
    auto mismatch = extension.mismatch_positions.begin();
    std::pair<size_t, size_t> current_interval(extension.read_interval.first, *mismatch);
    int32_t current_score = (current_interval.second - current_interval.first) * default_match;
    if (extension.left_full) {
        current_score += default_full_length_bonus;
    }

    // Process the alignment and keep track of the best interval we have seen so far.
    std::pair<size_t, size_t> best_interval = current_interval;
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

    size_t best_alignment = std::numeric_limits<size_t>::max(); // Metric to find the best extension from each seed        
    for (seed_type seed : seeds) {
        
        size_t node_offset = get_node_offset(seed);
        size_t read_offset = get_read_offset(seed);

        // Check if the seed is contained in an exact full-length alignment.
        if (best_alignment < result.size() && result[best_alignment].internal_score == 0) {
            if (result[best_alignment].contains(*graph, seed, node_offset, read_offset)) {
                // cout << "Contains on result" << endl;
                continue;
            }
        }

        GaplessExtension best_match {
            { }, static_cast<size_t>(0), gbwt::BidirectionalState(),
            { static_cast<size_t>(0), static_cast<size_t>(0) }, { },
            std::numeric_limits<int32_t>::min(), false, false,
            false, false, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()
        };
        
        priority_queue<GaplessExtension> extensions;
        
        GaplessExtension match {
            { seed.first }, node_offset, graph->get_bd_state(seed.first),
            { read_offset, read_offset }, { },
            static_cast<int32_t>(0), false, false,
            false, false, static_cast<uint32_t>(0), static_cast<uint32_t>(0)
        };
        
        match_initial(match, sequence, graph->get_sequence_view(seed.first));
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

            // Case 1: Extend to the right.
            if (!curr.right_maximal) {
                size_t num_extensions = 0;
                // Always allow at least max_mismatches / 2 mismatches in the current flank.
                uint32_t mismatch_limit = max(
                    static_cast<uint32_t>(MAX_MISMATCHES + 1),
                    static_cast<uint32_t>(MAX_MISMATCHES / 2 + curr.old_score + 1));
                
                // graph->follow_paths(state, bool, bool or lamda?);
                graph->follow_paths(curr.state, false, [&](const gbwt::BidirectionalState& next_state) -> bool {
                    handle_t handle = gbwtgraph::GBWTGraph::node_to_handle(next_state.forward.node);

                    GaplessExtension next {
                        { }, curr.offset, next_state,
                        curr.read_interval, { },
                        curr.score, curr.left_full, curr.right_full,
                        curr.left_maximal, curr.right_maximal, curr.internal_score, curr.old_score
                    };

                    size_t node_offset = match_forward(next, sequence, graph->get_sequence_view(handle), mismatch_limit);
                    if (node_offset == 0) { // Did not match anything.
                        return true;
                    }
                    next.path = get_path(curr.path, handle);
                    // Did the extension become right-maximal?
                    if (next.read_interval.second >= sequence.length()) {
                        next.right_full = true;
                        next.right_maximal = true;
                        next.old_score = next.internal_score;
                    } else if (node_offset < graph->get_length(handle)) {
                                next.right_maximal = true;
                        next.old_score = next.internal_score;
                    }
                    set_score(next);
                    num_extensions += next.state.size();
                    extensions.push(move(next));
                    return true;
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
                bool found_extension = false;
                // Always allow at least max_mismatches / 2 mismatches in the current flank.
                uint32_t mismatch_limit = max(
                    static_cast<uint32_t>(MAX_MISMATCHES + 1),
                    static_cast<uint32_t>(MAX_MISMATCHES / 2 + curr.old_score + 1));
                graph->follow_paths(curr.state, true, [&](const gbwt::BidirectionalState& next_state) -> bool {
                    handle_t handle = gbwtgraph::GBWTGraph::node_to_handle(gbwt::Node::reverse(next_state.backward.node));
                    size_t node_length = graph->get_length(handle);
                    GaplessExtension next {
                        { }, node_length, next_state,
                        curr.read_interval, { },
                        curr.score, curr.left_full, curr.right_full,
                        curr.left_maximal, curr.right_maximal, curr.internal_score, curr.old_score
                    };
                    match_backward(next, sequence, graph->get_sequence_view(handle), mismatch_limit);
                    if (next.offset >= node_length) { // Did not match anything.
                        return true;
                    }
                    next.path = get_path(handle, curr.path);
                    // Did the extension become left-maximal?
                    if (next.read_interval.first == 0) {
                        next.left_full = true;
                        next.left_maximal = true;
                        // No need to set old_score.
                    } else if (next.offset > 0) {
                        next.left_maximal = true;
                        // No need to set old_score.
                    }
                    set_score(next);
                    extensions.push(move(next));
                    found_extension = true;
                    return true;
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
                // cout << "Best match " << best_match.score << endl;
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
    // cout << "Saving " << full_result[element_index].sequence << " on full result\n";

    // for (int i = 0; i < result.size(); i++) {
    //     full_result[element_index].extensions[i] = result[i];
    // }
}

void write_extensions(ExtensionResult* results, int size) {
    // Specify the file name
    time_t now = time(0);

    // convert now to string form
    char buffer[80];
    strftime(buffer, 80, "%Y%m%d%H%M", localtime(&now));
    string date_time(buffer);

    string fileName = "/soe/jessicadagostini/miniGiraffe/data/proxy_extensions_" + date_time + ".bin";

    // Create an output file stream
    std::ofstream outFile(fileName, std::ios::binary | std::ios::app);

    if (!outFile) {
        std::cerr << "Error opening file: " << fileName << std::endl;
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
        // cout << tmpData.sequence << endl;
        file.read(reinterpret_cast<char*>(&qnt), sizeof(qnt));
        // std::cout << "Qnt " << qnt << endl;
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
                std::cerr << "Error reading from the file." << std::endl;
                return;
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
}

double get_wall_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time); // or CLOCK_REALTIME
    return time.tv_sec + time.tv_nsec / 1000000000.0;
}

void parallel_enq(int chunk_size, int size, int tid, int num_threads) {
  int start = chunk_size * tid;
  int end = std::min(start + chunk_size, size); // Make sure we don't index out of array.
  for (int i = start; i < end; i++) {
    Q[tid].enq(i); // Enqueue index as work item for local worklist.
  }
}

void work_stealing(vector<Source>& data, const gbwtgraph::GBWTGraph* graph, ExtensionResult* full_result, int batchsize, int tid, int num_threads) {
    // int batch[batchsize];
    int* batch = (int*)malloc(batchsize * sizeof(int));
    for (int r = Q[tid].deq_batch(batch, batchsize); r != -1; r = Q[tid].deq_batch(batch, batchsize)) {
        for (int j=0; j<batchsize; j++) {
            int i = batch[j];
            double start = get_wall_time();
            extend(data[i].sequence, data[i].seeds, graph, i, full_result);
            double end = get_wall_time();
            time_utils_add(start, end, 5, tid);
        }
    }

    // std::printf("Thread %d finished it's local workload!\n", tid);
    finished_threads.fetch_add(1);

    // Steal tasks from other worklists.
    int target = num_threads - ((tid + 1) % num_threads);
    while (finished_threads.load() != num_threads) {
        if (target == tid) {
            target = (target + 1) % num_threads;
        }
        // std::printf("Thread %d helping thread %d\n", tid, target);
        int r;
        while ((r = Q[target].deq_batch(batch, batchsize)) != -1) {
            for(int j=0; j<batchsize; j++) {
                int i = batch[j];
                double start = get_wall_time();
                extend(data[i].sequence, data[i].seeds, graph, i, full_result);
                double end = get_wall_time();
                time_utils_add(start, end, 5, tid);
            }
        }

        target = (target + 1) % num_threads; // Round robin.
    }
}

int main(int argc, char *argv[]) {
    vector<Source> data;
    int batch_size = DEFAULT_PARALLEL_BATCHSIZE;
    GBZ gbz;
    
    string filename_dump = argv[1];
    string filename_gbz = argv[2];
    int num_threads = atoi(argv[3]);
    if (argv[4]!=NULL) {
        batch_size = atoi(argv[4]);
    }

    load_seeds(filename_dump, data);
    
    cout << "Reading GBZ" << endl;
    
    sdsl::simple_sds::load_from(gbz, filename_gbz);
    const GBWTGraph* graph = &gbz.graph;

    int size = data.size();

    ExtensionResult* full_result = (ExtensionResult*)malloc(size * sizeof(ExtensionResult));
    if (full_result == NULL) {
        printf("Memory allocation failed!\n");
        return 1;
    }

    cout << "Starting mapping with " << num_threads << " and batch size " << batch_size << endl;
    
    thread* thread_array = new std::thread[num_threads];
    int chunk_size = (size + (num_threads - 1)) / num_threads;
    for (int i = 0; i < num_threads; i++) {
        // Initialize local worklists.
        Q[i].init(chunk_size);

        // Launch threads to enqueue to local worklists.
        thread_array[i] = std::thread(parallel_enq, chunk_size, size, i, num_threads);
    }

    // Join.
    for (int i = 0; i < num_threads; i++) {
        thread_array[i].join();
    }

    // Spawn threads to carry out work.
    for (int i = 0; i < num_threads; i++) {
        thread_array[i] = std::thread(work_stealing, std::ref(data), graph, full_result, batch_size, i, num_threads);
    }

    // Join.
    for (int i = 0; i < num_threads; i++) {
        thread_array[i].join();
    }


    // for (auto d : data) {
    //     extend(d.sequence, d.seeds, graph, full_result);
    // }
    cout << "Finished mapping" << endl;
    cout << "Writing extensions" << endl;
    time_utils_dump();
    // write_extensions(full_result, size);

    return 0;
}
