import pandas as pd
import sys

tmp_stderr = sys.argv[1] # 512_1_omp_0_yeast_miniGiraffe16.csv
output_csv = sys.argv[2] # 512_1_omp_0_yeast_miniGiraffe16.csv

data = pd.read_csv(tmp_stderr, delimiter=",", header=None)
data[4] = data[2] - data[1]
grouped  = data.groupby([0, 3]).sum().reset_index()
grouped[[0, 4, 3]].to_csv(output_csv, index=False)