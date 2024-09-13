follow_paths(graph, state, forward, lambda(next_state, direction)):
    set index with state.node (getting node from graph)
    for edge in outdegree(index):
        set next_state using the edge information from next index
        lambda(next_state, direction)

matching**(extension, sequence, graph):
    set left_minimal
    set node_offset
    while left > 0:
        set length
        set A with bytes memcpy of seq interval
        set B with bytes memcpy of node interval
        if A == B:
            set read_interval with length
        else:
            for char in length:
                if seq[read_interval] not equal target[node_offset]:
                    if mismatch_score > mismatch_limit:
                        return
                    set mismatch_score + 1
                set read_interval + 1
                set node_offset + 1
        set left - length
    return node_offset


lambda(next_state, direction):
    set handle getting next_state
    set next_extension

    if direction is forward:
        match_forward(next_extension, sequence, node(handle))
    else:
        match_backward(next_extension, sequence, node(handle))
    set next_extension.path
    set if it is left or right maximal

    set_score(next_extension)
    add next_extension to extensions
    return true

--------------------------------- MAIN CODE ----------------------------------

read_inputs()
for each sequences:
    
    set struct to store results
    set best_alignment

    for each seeds from sequence:
        get_node_offset()
        get_read_offset()

        best_alignment < result and best_alignment.score == 0:
            if check_if_seed_is_on_full_alignment():
                go to next seed
        
        set best_match
        set extensions priority queue
        set match

        match_initial(match, sequence, node(seed))
        set_score(match)
        add match to extensions

        while extensions not empty:
            set cur with extension.top
            if curr not right maximal extension:
                follow_paths(graph, state, forward, lambda(next_state, direction))
                if num_extensions < outgoing_edges:
                    reach_right_maximal_on_read()
                go to next seed
            if curr not left maximal extension:
                follow_paths(graph, state, backward, lambda(next_state, direction))
                if not found extension:
                    set curr.left_maximal = true
                else
                    go to next seed

            if best_match.score < curr.score
                best_match = curr
        
        if best_match is not empty:
            check if best_match is better than current best_alignment
            set best_alignment as size of results struct
            move best_match to the end of the vector
        
    if results has full length alignments:
        remove_repeated_full_length_alignments(results)
        find_mismatches(sequence, graph, results)
    else:
        remove_duplicates(results)
        find_mismatches(sequence, graph, results)
        for each extension from results:
            trim_mismatches(extension, graph)
        if trimmed:
            remove_duplicates(results)            
