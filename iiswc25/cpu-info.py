import pandas as pd
import os

destination_folder = "/lscratch/jessicadagostini/miniGiraffe/intelxeonplatinum8260cpu@240ghz/grch38"
os.chdir(destination_folder)

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

all_dataframes = []
if not results:
    print("No CSV files found in the directory.")
else:
    for output_csv in results:
        try:
            df = pd.read_csv(output_csv, names=['query', 'runtime', 'thread'], header=None, delimiter=",")
            source = os.path.basename(output_csv)
            # 2048_96_ws_0_tuning_512.csv
            batch, threads, scheduler, rep, opt, cache = source.split("_")[:6]
            makespan['batch_size'].append(int(batch))
            makespan['num_threads'].append(int(threads))
            makespan['scheduler'].append(scheduler)
            makespan['cache_size'].append(int(cache.split('.')[0]))  # Remove .csv extension

            local_makespan = df.groupby(['query'])['runtime'].max().reset_index(name='makespan')
            # print(local_makespan)
            makespan['runtime'].append(float(local_makespan[local_makespan['query'] == 'seeds-loop']['makespan'].values[0]))
            print(makespan)
            break               
        except pd.errors.EmptyDataError:
            print(f"Warning: {output_csv} is empty and will be skipped.")
        except Exception as e:
            print(f"Error reading {output_csv}: {e}")
