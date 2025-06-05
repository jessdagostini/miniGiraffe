import os
import pandas as pd
from MiniGiraffePipeline import MiniGiraffePipeline

# Start pipeline
source_folder = "/soe/jessicadagostini/miniGiraffe"
destination_folder = "/lscratch/jessicadagostini/miniGiraffe"
pipeline = MiniGiraffePipeline(source_folder, destination_folder)

mG_files = ["miniGiraffeNC", "miniGiraffe256", "miniGiraffe4096", "perf-utils.so", "time-utils.so", "set-env.sh"]

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
grch38_path = "/lscratch/jessicadagostini/grch38"
# seed_path = os.path.join(pipeline.destination_folder, "dump_proxy_yeast_all_16.bin")
# gbz_path = os.path.join(pipeline.destination_folder, "yeast_all.giraffe.gbz")
seed_path = os.path.join(grch38_path, "dump_proxy_d1s1l001.bin")
gbz_path = os.path.join(grch38_path, "hprc-v1.1-mc-grch38.gbz")

# Setup folder for tests based on machine info
cpu_info = pipeline.get_cpu_info()
try:
    model = cpu_info['Model name'].lower().replace("(r)", "").replace(" ", "").replace(".", "")
    pipeline.logger.info(f"[Main] Machine model: {model}")
except KeyError as e:
    pipeline.logger.error(f"[Main] Error getting machine model {e}")
    model = "unknown"

model = model + "/6-4"
pipeline.set_machine_folder(model)

# Define array of tests
batch_size = [512]

# cache_size = ["miniGiraffeNC", "miniGiraffe256", "miniGiraffe2048"]
cache_size = ["miniGiraffe4096"]

scheduler = ['ws', 'omp']

num_threads = [96, 72, 48, 24]

repetitions = 1

options = {
    'grch38' :  [
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
                            if 'grch38' in opt:
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
testing_folder = os.path.join(pipeline.source_folder, "iiswc25")
pipeline.run_rsync(pipeline.machine_folder, testing_folder)

pipeline.logger.info("[Main] miniGiraffe pipeline completed")
