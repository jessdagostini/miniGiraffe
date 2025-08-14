import os
import sys
import pandas as pd
from MiniGiraffePipeline import MiniGiraffePipeline

# Setup pipeline
source_folder = os.path.join(os.getenv('HOME'), "miniGiraffe")
destination_folder = os.path.join(os.getenv('HOME'), "miniGiraffe", "iiswc25")
pipeline = MiniGiraffePipeline(source_folder, destination_folder)

sequence_path = ""
gbz_path = ""
input_set = ""

# Try to get the input files from args
if len(sys.argv) < 4:
    # Download test case
    pipeline.download_zenodo_record(14990368)

    # Setup paths
    sequence_path = os.path.join(pipeline.destination_folder, "dump_proxy_novaseq.bin")
    gbz_path = os.path.join(pipeline.destination_folder, "1000GPlons_hs38d1_filter.giraffe.gbz")
    input_set = "1000GP"
else:
    sequence_path = sys.argv[1]
    gbz_path = sys.argv[2]
    input_set = sys.argv[3]

pipeline.logger.info(f"[Main] Sequence path: {sequence_path}")
pipeline.logger.info(f"[Main] GBZ path: {gbz_path}")

# Start moving files to the destination folder
mG_files = ["lower-input", "miniGiraffe", "perf-utils.so", "time-utils.so", "set-env.sh"]

pipeline.remove_existing_files(mG_files)

# Copy miniGiraffe binary and .so files
for file in mG_files:
    source_path = os.path.join(pipeline.source_folder, file)
    pipeline.copy_files(source_path, pipeline.destination_folder)

# Lower sequence input to run testing
pipeline.logger.info("[Main] Lowering sequence input for testing")
sequence_path_lowered = pipeline.lower_sequence_input(sequence_path)

# Setup the environment with paths
env_path = os.environ.copy()
env_path['LD_LIBRARY_PATH'] = f"$LD_LIBRARY_PATH:{pipeline.source_folder}/deps/gbwt/lib:{pipeline.source_folder}/deps/libhandlegraph/build/usr/local/lib:{pipeline.source_folder}/:{pipeline.source_folder}"

# Setup folder for tests based on machine info + define how many threads to use
cpu_info = pipeline.get_cpu_info()

total_threads = None
total_cpu = None

# Collect how many threads I can run in the machine
if "CPU(s)" in cpu_info:
    total_cpus_str = cpu_info["CPU(s)"]
    try:
        total_threads = int(total_cpus_str)
        pipeline.logger.info(f"[Main] Total threads available: {total_threads}")
    except ValueError:
        pipeline.logger.error(f"[Main] Could not parse 'CPU(s)': {total_cpus_str} as an integer.")
else:
    pipeline.logger.warning("[Main] 'CPU(s)' key not found in lscpu output.")

if "Core(s) per socket" in cpu_info and "Socket(s)" in cpu_info:
    cores_per_socket_str = cpu_info["Core(s) per socket"]
    sockets_str = cpu_info["Socket(s)"]
    try:
        cores_per_socket = int(cores_per_socket_str)
        sockets = int(sockets_str)
        total_cpu = cores_per_socket * sockets
        pipeline.logger.info(f"[Main] Total physical cores available: {total_cpu}")
    except ValueError:
        pipeline.logger.error(f"[Main] Could not parse core/socket info: Cores per socket='{cores_per_socket_str}', Sockets='{sockets_str}'.")

# Collect machine model
try:
    model = cpu_info['Model name'].lower().replace("(r)", "").replace(" ", "").replace(".", "")
    pipeline.logger.info(f"[Main] Machine model: {model}")
except KeyError as e:
    pipeline.logger.error(f"[Main] Error getting machine model {e}")
    model = "unknown"

pipeline.set_machine_folder(os.path.join(model, input_set))

# Define array of tests
batch_size = [128, 256, 512, 1024, 2048]

cache_size = [256, 512, 1024, 2048, 4096]

scheduler = ['omp', 'ws']

num_threads = [total_threads]

options = {
    'tuning' :  [
        '-p'
    ]
    
}

pipeline.generate_design_matrix(batch_size, cache_size, scheduler, num_threads)

# Define repetitions
repetitions = 1

# Change to machine folder as current working directory
os.chdir(pipeline.machine_folder)

# Look for tests that have already been run and skip them (in case we need to restart the pipeline)
results = os.listdir()
results = [x for x in results if x.endswith(".csv")]

makespan = {
    'runtime': [],
    'batch_size': [],
    'num_threads': [],
    'scheduler': [],
    'cache_size': []
}

for exp in pipeline.full_fact_design:
    batch, cache, sched, threads = exp
    for opt in options:
        for rep in range(repetitions):
            pipeline.logger.info(f"[Main] Running test with cache size {cache} batch size {batch}, threads {threads}, scheduler {sched}, and options {opt} on iteration {rep}")
            output_csv = f"{batch}_{threads}_{sched}_{rep}_{opt}_{cache}.csv"

            if output_csv in results:
                pipeline.logger.info(f"[Main] Test already run: {output_csv}. Adding content to makespan dictionary.")
                try:
                    df = pd.read_csv(output_csv, names=['query', 'runtime', 'thread'], header=None, delimiter=",")
                    local_makespan = df.groupby(['query'])['runtime'].max().reset_index(name='makespan')

                    # 2048_96_ws_0_tuning_512.csv
                    source = os.path.basename(output_csv)
                    batch, threads, scheduler, rep, opt, cache = source.split("_")[:6]
                    makespan['batch_size'].append(int(batch))
                    makespan['num_threads'].append(int(threads))
                    makespan['scheduler'].append(scheduler)
                    makespan['cache_size'].append(int(cache.split('.')[0]))  # Remove .csv extension
                    makespan['runtime'].append(float(local_makespan[local_makespan['query'] == 'seeds-loop']['makespan'].values[0]))
                    pipeline.logger.debug(f"[Main] Makespan dictionary updated: {makespan}")          
                except pd.errors.EmptyDataError:
                    print(f"Warning: {csv_file} is empty and will be skipped.")
                except Exception as e:
                    print(f"Error reading {csv_file}: {e}")
                continue

            binary_path = os.path.join(pipeline.destination_folder, mG_files[1])

            # output = pipeline.run_binary(binary_path, [sequence_path_lowered, gbz_path, f'-b{batch}', f'-t{threads}', f'-s{sched}', f'-c{cache}'] + options[opt],  env_path)
            output = pipeline.run_binary(binary_path, [sequence_path, gbz_path, f'-b{batch}', f'-t{threads}', f'-s{sched}', f'-c{cache}'] + options[opt],  env_path)

            # Parse and save output
            if output is not None:
                pipeline.logger.info(f"[Main] Test completed successfully, saving results on file and on makespan: {output_csv}")
                data = pd.read_csv(pipeline.tmp_stderr, delimiter=",", header=None)
                data[4] = data[2] - data[1]
                grouped  = data.groupby([0, 3]).sum().reset_index()
                local_makespan = grouped.groupby([0])[4].max().reset_index(name='makespan')
                
                makespan['batch_size'].append(int(batch))
                makespan['num_threads'].append(int(threads))
                makespan['scheduler'].append(sched)
                makespan['cache_size'].append(int(cache))
                makespan['runtime'].append(float(local_makespan[local_makespan[0] == 'seeds-loop']['makespan'].values[0]))
                grouped[[0, 4, 3]].to_csv(output_csv, index=False)
            else:
                pipeline.logger.error("[Main] Error running test")
                exit(-1)

pipeline.logger.info("[Main] All tests completed successfully.")

# Aggregate results and find the best configuration
pipeline.logger.info("[Main] Aggregating results and finding the best configuration")

# # Transform the makespan dictionary into a DataFrame
makespan_df = pd.DataFrame(makespan)
pipeline.logger.debug(f"[Main] Makespan DataFrame created successfully: {makespan_df}")

# # Find the best configuration based on the maximum runtime for each grouping
grouping_columns = ['batch_size', 'num_threads', 'scheduler', 'cache_size']
max_runtime_df_indexed = makespan_df.groupby(grouping_columns)['runtime'].max().reset_index(name='makespan')
# filtered_by_q1 = max_runtime_df_indexed[max_runtime_df_indexed['query'] == "seeds-loop"]
filtered_by_q1 = max_runtime_df_indexed[max_runtime_df_indexed['makespan'].min() == max_runtime_df_indexed['makespan']]
pipeline.logger.info(f"[Main] Best configuration found: {filtered_by_q1}")
batch, threads, scheduler, cache, runtime = filtered_by_q1.values.tolist()[0]

# # Log the best configuration
pipeline.logger.info(f"[Main] Best configuration found: Batch Size: {batch}, Threads: {threads}, Scheduler: {scheduler}, Cache Size: {cache}, Runtime: {runtime}")

# Run the best configuration with the original sequence input
pipeline.logger.info("[Main] Running the best configuration with the original sequence input")
output_csv = f"{batch}_{threads}_{scheduler}_{0}_full_{cache}.csv"
binary_path = os.path.join(pipeline.destination_folder, mG_files[1])
output = pipeline.run_binary(binary_path, [sequence_path, gbz_path, f'-b{batch}', f'-t{threads}', f'-s{scheduler}', f'-c{cache}'] + options['tuning'], env_path)
if output is not None:
    data = pd.read_csv(pipeline.tmp_stderr, delimiter=",", header=None)
    data[4] = data[2] - data[1]
    grouped  = data.groupby([0, 3]).sum().reset_index()
    grouped[[0, 4, 3]].to_csv(output_csv, index=False)
else:
    pipeline.logger.error("[Main] Error running the best configuration test")
    exit(-1)

# Run the default configuration with the original sequence input
pipeline.logger.info("[Main] Running the default configuration with the original sequence input")
output_csv = f"512_{threads}_omp_0_full_256.csv"
output = pipeline.run_binary(binary_path, [sequence_path, gbz_path, f'-t{threads}'] + options['tuning'], env_path)
if output is not None:
    data = pd.read_csv(pipeline.tmp_stderr, delimiter=",", header=None)
    data[4] = data[2] - data[1]
    grouped  = data.groupby([0, 3]).sum().reset_index()
    grouped[[0, 4, 3]].to_csv(output_csv, index=False)
else:
    pipeline.logger.error("[Main] Error running the default configuration test")
    exit(-1)

pipeline.logger.info("[Main] miniGiraffe tuning pipeline completed successfully")
