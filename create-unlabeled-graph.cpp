#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <gbwtgraph/gbz.h>
#include <gbwtgraph/cached_gbwtgraph.h>
#include <fstream>
#include <iostream>

using namespace gbwtgraph;
using namespace std;

int main(int argc, char *argv[]) {
    GBZ gbz;
    const char *filename_gbz = argv[1];
    
    // printf("Reading gbz\n");
    sdsl::simple_sds::load_from(gbz, filename_gbz);
    const GBWTGraph* graph = &gbz.graph;
    nid_t min = graph->min_node_id();
    nid_t max = graph->max_node_id();

    // cout << "Total nodes: " << graph->get_node_count() << endl;
    // cout << min << endl;
    // cout << max << endl;

    size_t edges = 0;
    size_t false_nodes = 0;

    for (nid_t i = min; i < max; i++) {
        if (graph->has_node(i)) {
            handle_t handle = graph->get_handle(i);
            // Cache the node.
            gbwt::node_type curr = graph->handle_to_node(handle);
            gbwt::CachedGBWT cache = graph->get_single_cache();
            gbwt::size_type cache_index = cache.findRecord(curr);
            
            for(gbwt::rank_type outrank = 0; outrank < cache.outdegree(cache_index); outrank++)
            {
                gbwt::node_type next = cache.successor(cache_index, outrank);
                // if(next == gbwt::ENDMARKER) {
                //     cout << "Endmarker " << next;
                // }
                handle_t h_next = graph->node_to_handle(next);
                // cout << "Node type " << curr << " " << next << endl;

                nid_t id_next = graph->get_id(h_next);
                cout << i << " " << id_next << endl;

            }
            // cout << endl;


            edges += cache.outdegree(cache_index);
        } else {
            false_nodes++;
        }
        // printf("[%lld/%lld] Outdegree: %ld\n", i, max, outdegree);
    }

    // cout << "False nodes: " << false_nodes << endl;
    // cout << "Total edges: " << edges << endl;
    // handle_t handle = graph->get_handle(291742533);
    // cout << graph->get_sequence(handle) << endl;

    // handle = graph->get_handle(291738571);
    // cout << graph->get_sequence(handle) << endl;

    

    return 0;
}