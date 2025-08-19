import os
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

mG_files = ["miniGiraffe", "perf-utils.so", "time-utils.so", "set-env.sh"]

pipeline.remove_existing_files(mG_files)

# Copy miniGiraffe binary and .so files
for file in mG_files:
    source_path = os.path.join(pipeline.source_folder, file)
    pipeline.copy_files(source_path, pipeline.destination_folder)

# Setup the environment with paths
env_path = os.environ.copy()
env_path['LD_LIBRARY_PATH'] = f"$LD_LIBRARY_PATH:{pipeline.source_folder}/deps/gbwt/lib:{pipeline.source_folder}/deps/libhandlegraph/build/usr/local/lib:{pipeline.source_folder}/:{pipeline.source_folder}"

# Setup paths
# paths are different for Grch38 input set
# grch38_path = "/lscratch/jessicadagostini/chm13"
# yeast_path = "/lscratch/jessicadagostini/yeast"
# seed_path = os.path.join(pipeline.destination_folder, "dump_proxy_yeast_all_16.bin")
# gbz_path = os.path.join(pipeline.destination_folder, "yeast_all.giraffe.gbz")
# seed_path = os.path.join(grch38_path, "dump_proxy_d1s1l002.bin")
# gbz_path = os.path.join(grch38_path, "hprc-v1.1-mc-chm13.gbz")
# seed_path = os.path.join(yeast_path, "dump_proxy_yeast_all_16.bin")
# gbz_path = os.path.join(yeast_path, "yeast_all.giraffe.gbz")

# Setup folder for tests based on machine info
cpu_info = pipeline.get_cpu_info()
try:
    model = cpu_info['Model name'].lower().replace("(r)", "").replace(" ", "").replace(".", "")
    pipeline.logger.info(f"[Main] Machine model: {model}")
except KeyError as e:
    pipeline.logger.error(f"[Main] Error getting machine model {e}")
    model = "unknown"

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

model = model + "/" + input_set
pipeline.set_machine_folder(model)

# Define array of tests
batch_size = [512]

cache_size = ["miniGiraffe"]

scheduler = ['ws', 'omp']

tmp = total_threads

num_threads = []

while tmp > 0:
    num_threads.append(tmp)
    tmp //= 2

repetitions = 3

options = {
    f'{input_set}' :  [
        '-p'
    ]
    
}

# Change to machine folder as current working directory
os.chdir(pipeline.machine_folder)

# Look for tests that have already been run and skip them (in case we need to restart the pipeline)
results = os.listdir()
results = [x for x in results if x.endswith(".csv")]

# Run tests
for cache in cache_size:
    for batch in batch_size:
        for sched in scheduler:
            for threads in num_threads:
                for opt in options:
                    for rep in range(repetitions):
                        pipeline.logger.info(f"[Main] Running test with cache size {cache} batch size {batch}, threads {threads}, scheduler {sched}, and options {opt} on iteration {rep}")
                        output_csv = f"{batch}_{threads}_{sched}_{rep}_{opt}_{cache}.csv"

                        if output_csv in results:
                            pipeline.logger.info(f"[Main] Test already run: {output_csv}")
                            continue

                        binary_path = os.path.join(pipeline.destination_folder, cache)
                        
                        output = pipeline.run_binary(binary_path, [seed_path, gbz_path, f'-b{batch}', f'-t{threads}', f'-s{sched}'] + options[opt],  env_path)

                        # Parse and save output
                        if output is not None:
                            # If the output is from a timing measurement, we need to do some parsing
                            if input_set in opt:
                                data = pd.read_csv(pipeline.tmp_stderr, delimiter=",", header=None)
                                data[4] = data[2] - data[1]
                                grouped  = data.groupby([0, 3]).sum().reset_index()
                                grouped[[0, 4, 3]].to_csv(output_csv, index=False)
                            else:
                                # Otherwise we just need to rename the stderr file
                                os.rename(pipeline.tmp_stderr, output_csv)
                        else:
                            pipeline.logger.error("[Main] Error running test")
                            exit(-1)

os.chdir(pipeline.source_folder)
# Copy results to a common folder
pipeline.run_rsync(pipeline.machine_folder, pipeline.source_folder)

pipeline.logger.info("[Main] miniGiraffe pipeline completed")
