import pandas as pd
import sys

tmp_stderr = sys.argv[1] # 512_1_omp_0_yeast_miniGiraffe16.csv
output_csv = sys.argv[2] # 512_1_omp_0_yeast_miniGiraffe16.csv
print(output_csv)

data = pd.read_csv(tmp_stderr, delimiter=",", header=None, skiprows=3, names=["Op", "Thread"])
grouped = data.groupby(["Op", "Thread"]).agg(Total=("Op", 'size')).reset_index()

grouped.to_csv(output_csv, index=False)