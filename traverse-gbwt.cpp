#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <gbwtgraph/gbz.h>
#include <gbwtgraph/cached_gbwtgraph.h>
#include <fstream>
#include <iostream>
#include <queue>

using namespace gbwtgraph;
using namespace std;

typedef struct {
    gbwt::node_type root;
    int distance;
} node_info;


int main(int argc, char *argv[]) {
    GBZ gbz;
    const char *filename_gbz = argv[1];
    
    printf("Reading gbz\n");
    sdsl::simple_sds::load_from(gbz, filename_gbz);
    const GBWTGraph* graph = &gbz.graph;
    nid_t min = graph->min_node_id();
    nid_t max = graph->max_node_id();

    cout << "Total nodes: " << graph->get_node_count() << endl;
    cout << min << endl;
    cout << max << endl;

    vector<bool> visited(graph->get_node_count(), false);
    queue<gbwt::node_type> q;
    handle_t h;
    gbwt::node_type curr;
    gbwt::CachedGBWT cache = graph->get_single_cache();

    if (graph->has_node(min)) {
        h = graph->get_handle(i);
        visited[min] = true;
        q.push(min);
    }

    while(!q.empty()) {
        curr = graph->handle_to_node(q.front());
        q.pop();

        gbwt::size_type cache_index = cache.findRecord(curr);
        
        for(gbwt::rank_type outrank = 0; outrank < cache.outdegree(cache_index); outrank++)
        {
            gbwt::node_type next = cache.successor(cache_index, outrank);
        }
    }





    return 0;
}