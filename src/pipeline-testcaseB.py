import os
import shutil
import subprocess
import shlex
import pandas as pd
import io
import logging
import logging.handlers
import requests

source_folder = os.getcwd()
destination_folder = "/lscratch/jessicadagostini/miniGiraffe/"
tmp_stdout = "./tmp_stdout.txt"
tmp_stderr = "./tmp_stderr.txt"

def configure_log(LOG_FILENAME):
    logger = logging.getLogger('miniGiraffe')

    logger.setLevel(logging.INFO)

    # Defines the format of the logger
    formatter = logging.Formatter(
        '%(asctime)s %(module)s - %(levelname)s - %(message)s'
    )

    # Configure the log rotation
    handler = logging.handlers.RotatingFileHandler(
        LOG_FILENAME,
        maxBytes=268435456,
        backupCount=50,
        encoding='utf8'
    )

    handler.setFormatter(formatter)

    logger.addHandler(handler)

    return logger

def download_zenodo_record(record_id, output_dir="."):
    """Downloads all files from a Zenodo record.

    Args:
        record_id (int or str): The Zenodo record ID.
        output_dir (str): The directory to save the files.
    """
    # Zenodo API endpoint for records
    api_url = f"https://zenodo.org/api/records/{record_id}"

    try:
        # Get the record data
        response = requests.get(api_url)
        response.raise_for_status()  # Raise an exception for bad status codes
        record_data = response.json()

        # Extract file information
        files = record_data['files']

        for file_info in files:
            file_url = file_info['links']['self']
            filename = file_info['key']  # Use 'key' for the filename
            filepath = os.path.join(output_dir, filename)

            # Check if the file already exists
            if os.path.exists(filepath):
                logger.warning(f"[Download Zenodo] File already exists: {filepath}")
                continue

            # Download the file
            logger.info(f"[Download Zenodo] Downloading: {filename}")
            with requests.get(file_url, stream=True) as download_response:
                download_response.raise_for_status()
                with open(filepath, 'wb') as f:
                    for chunk in download_response.iter_content(chunk_size=8192):
                        f.write(chunk)
            logger.info(f"[Download Zenodo] Downloaded: {filename} to {filepath}")

    except requests.exceptions.RequestException as e:
        logger.error(f"[Download Zenodo] An error occurred: {e}")
    except KeyError as e:
         logger.error(f"[Download Zenodo] Error: Key not found in response: {e}.  Check record ID and API.")
    except Exception as e:
        logger.error(f"[Download Zenodo] An unexpected error occurred: {e}")

def copy_files(source_path, destination_folder):
    try:
        # Check if source file exists
        if not os.path.exists(source_path):
            raise FileNotFoundError(f"Source file not found: {source_path}")

        # Check if destination is a directory
        if os.path.isdir(destination_folder):
            # Construct the full destination path including the filename
            destination_path = os.path.join(destination_folder, os.path.basename(source_path))

        # Check if destination file already exists
        if os.path.exists(destination_path):
            raise FileExistsError(f"Destination file already exists: {destination_path}")

        # Move the file
        shutil.copy(source_path, destination_path)
        logger.info(f"[Copy Files] File copied successfully from {source_path} to {destination_path}")

    except FileNotFoundError as e:
        logger.error(f"[Copy Files] {e}")
    except FileExistsError as e:
        logger.error(f"[Copy Files] {e}")
    except OSError as e:  # Catch other OS-related errors (permissions, etc.)
        logger.error(f"[Copy Files] {e}")
    except Exception as e: # Catch any other unexpected error
        logger.error(f"[Copy Files] An unexpected error occurred: {e}")


def run_binary(binary_path, args=None, env=''):
    try:
        if args is None:
            args = []

        command_list = [binary_path] + args

        command = " ".join(shlex.quote(arg) for arg in command_list) #Builds safe string
        logger.info(f"[Run Binary] {command}")
        with open(tmp_stdout, "w") as fstdout, open(tmp_stderr, "w") as fstderr:
            result = subprocess.run(command, stdout=fstdout, stderr=fstderr, text=True, check=True, shell=True, env=env)
        
        return result

    except FileNotFoundError:
        logger.error(f"[Run Binary] Error: Binary not found at {binary_path}")
        return None
    except subprocess.CalledProcessError as e:
        logger.error(f"[Run Binary] Error running binary: {e}")
        logger.error(f"[Run Binary] Return code: {e.returncode}")
        logger.error(f"[Run Binary] Stdout: {e.stdout}")
        logger.error(f"[Run Binary] Stderr: {e.stderr}")
        return None
    except Exception as e:
         logger.error(f"[Run Binary] An unexpected error occurred: {e}")
         return None

def get_cpu_info(cpu_info):
    try:
        result = subprocess.run(['lscpu'], capture_output=True, text=True, check=True)
        output = result.stdout
        for line in output.splitlines():
            if ':' in line:
                key, value = line.strip().split(':', 1)
                cpu_info[key.strip()] = value.strip()
    except subprocess.CalledProcessError as e:
        logger.error(f"[CPU Info] Error running lscpu: {e}")
        logger.error(f"[CPU Info] Return code: {e.returncode}")
        logger.error(f"[CPU Info] Stdout: {e.stdout}")
        logger.error(f"[CPU Info] Stderr: {e.stderr}")


### MAIN SCRIPT STARTS HERE ###

# Setup logging
logger = configure_log("miniGiraffeB.log")
logger.info("***Starting miniGiraffe pipeline***")

mG_files = ["miniGiraffeNC", "miniGiraffe256", "miniGiraffe64", "miniGiraffe16", "perf-utils.so", "time-utils.so", "set-env.sh"]
# mG_files = ["miniGiraffeThread256", "miniGiraffeThread128", "miniGiraffeThread16", "miniGiraffeThread512" "perf-utils.so", "time-utils.so", "set-env.sh"]

# Move to tmp to be a common folder
try:
    # Create the tmp folder if it does not exist
    if not os.path.exists(destination_folder):
        os.makedirs(destination_folder)
        logger.info(f"[Main] Created destination folder: {destination_folder}")
    else:
        logger.warning(f"[Main] Destination folder already exists: {destination_folder}")
        # Remove any existing miniGiraffe binaries from existing folder
        for file in mG_files:
            existing_file_path = os.path.join(destination_folder, file)
            if os.path.exists(existing_file_path):
                os.remove(existing_file_path)
                logger.info(f"[Main] Removed existing file: {existing_file_path}")

except Exception as e: # Catch any other unexpected error
    logger.error(f"[Main] An unexpected error occurred: {e}")

# Download test case files
# download_zenodo_record(14990368, destination_folder)

# # Copy test cases
# source_path = os.path.join(source_folder, "dump_proxy_novaseq.bin")
# copy_files(source_path, destination_folder)

# source_path = os.path.join(source_folder, "1000GPlons_hs38d1_filter.giraffe.gbz")
# copy_files(source_path, destination_folder)

# Copy miniGiraffe binary and .so files
for file in mG_files:
    source_path = os.path.join(source_folder, file)
    copy_files(source_path, destination_folder)

# Setup the environment with paths
env_path = os.environ.copy()
env_path['LD_LIBRARY_PATH'] = f"$LD_LIBRARY_PATH:{source_folder}/deps/gbwt/lib:{source_folder}/deps/libhandlegraph/build/usr/local/lib:{source_folder}/:{source_folder}"
# logger.info('[Main] LD_LIBRARY_PATH')

# Setup paths
testcase_folder = "/lscratch/jessicadagostini/yeast/"
seed_path = os.path.join(testcase_folder, "dump_proxy_yeast_all_16.bin")
gbz_path = os.path.join(testcase_folder, "yeast_all.giraffe.gbz")

# Run warmup test case
# logger.info("[Main] Running warmup test case")
# if run_binary(binary_path, [seed_path, gbz_path], env_path):
#     logger.info("[Main] Warmup test case ran successfully")
# else:
#     logger.error("[Main] Error running warmup test case")
#     exit(-1)

# Setup folder for tests based on machine info
cpu_info = {}
get_cpu_info(cpu_info)
try:
    model = cpu_info['Model name'].lower().replace("(r)", "").replace(" ", "").replace(".", "")
    logger.info(f"[Main] Machine model: {model}")
except KeyError as e:
    logger.error(f"[Main] Error getting machine model {e}")
    model = "unknown"

try:
    # Create the machine folder if it does not exist
    machine_folder = os.path.join(destination_folder, model)
    if not os.path.exists(machine_folder):
        os.makedirs(machine_folder)
        logger.info(f"[Main] Created machine folder: {machine_folder}")
    else:
        logger.info(f"[Main] Machine folder already exists: {machine_folder}")
except Exception as e: # Catch any other unexpected error
    logger.error(f"[Main] An unexpected error occurred: {e}")

# Define array of tests
# batch_size = [256, 512, 1024, 2048, 4096]
batch_size = [256, 512, 1024]

cache_size = ["miniGiraffeNC", "miniGiraffe256", "miniGiraffe64", "miniGiraffe16"]
# cache_size = ["miniGiraffeThread256", "miniGiraffeThread128", "miniGiraffeThread16", "miniGiraffeThread512"]

# scheduler = ['ws', 'omp']
scheduler = ['omp']

# max_num_threads = os.cpu_count()

# num_threads = []
# i = max_num_threads
# while i > 10:
#     num_threads.append(i)
#     i = int(i / 2)

# num_threads.append(1)
# num_threads.append(2)
# num_threads = [16, 32]
# num_threads = [1]
num_threads = [64, 32, 16, 8, 4, 2, 1]

repetitions = 2

# options = {
#     'timing' :  [
#         '-p'
#     ],
#     'hwmetrics1' : [
#         '-m',
#         'IPC,L1CACHE,LLCACHE'
#     ],
#     'hwmetrics2' : [
#         '-m',
#         'BRANCHES,DTLB,ITLB'
#     ]
    
# }

# options = {
#     'timing-sort' :  [
#         '-p',
#         '-o'
#     ]
    
# }

options = {
    'yeast' :  [
        '-p'
    ]
    
}

# Change to machine folder as current working directory
os.chdir(machine_folder)

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
                        logger.info(f"[Main] Running test with cache size {cache} batch size {batch}, threads {threads}, scheduler {sched}, and options {opt} on iteration {rep}")
                        output_csv = f"{batch}_{threads}_{sched}_{rep}_{opt}_{cache}.csv"

                        if output_csv in results:
                            logger.info(f"[Main] Test already run: {output_csv}")
                            continue

                        binary_path = os.path.join(destination_folder, cache)
                        
                        output = run_binary(binary_path, [seed_path, gbz_path, f'-b{batch}', f'-t{threads}', f'-s{sched}'] + options[opt],  env_path)

                        # Parse and save output
                        if output is not None:
                            # If the output is from a timing measurement, we need to do some parsing
                            if 'timing' in opt:
                                data = pd.read_csv(tmp_stderr, delimiter=",", header=None)
                                data[4] = data[2] - data[1]
                                grouped  = data.groupby([0, 3]).sum().reset_index()
                                grouped[[0, 4, 3]].to_csv(output_csv, index=False)
                            else:
                                # Otherwise we just need to rename the stderr file
                                os.rename(tmp_stderr, output_csv)
                        else:
                            logger.erro("[Main] Error running test")
                            exit(-1)

os.chdir(source_folder)
# Copy results to a common folder
try:
    shutil.copytree(machine_folder, os.path.join(source_folder, os.path.basename(machine_folder)))
    logger.info(f"[Main] Results copied to {os.path.join(source_folder, os.path.basename(machine_folder))}")
except Exception as e: # Catch any other unexpected error
    logger.error(f"[Main] An unexpected error occurred: {e}")

logger.info("[Main] miniGiraffe pipeline completed")
