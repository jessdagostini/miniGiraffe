# miniGiraffe: proxy application from Pangenomics mapping
miniGiraffe is a proxy application based on [VG Giraffe](https://github.com/vgteam/vg) Mapping Tool for computational improvements and exploration of Pangenome mapping kernels. It provides a clean and easy-to-play application to test the behavior of a Pangeomics kernel in different environments. It is coded in C++14 (following VG Giraffe original code) and parallelized using OpenMP and C++ Threads.


Building
------------------------------------------
Clone this repository using the recursive option (to download dependencies from `deps` folder)

```git clone --recursive git@github.com:jessdagostini/miniGiraffe.git```

After the download, navigate to the folder.

*IMPORTANT*: Before running the following command, make sure you have `cmake` installed in your system:

We first install the dependencies and then build miniGiraffe. It's about two command lines to have miniGiraffe ready to run.

```
bash install-deps.sh
make miniGiraffe
```

Running miniGiraffe
------------------------------------------
You can execute miniGiraffe using

```
Usage miniGiraffe [seed_file] [gbz_file] [options]
Options: 
   -t, number of threads (default: max # threads in system)
   -c, initial GBWTcache capacity (default: 256)
   -b, batch size (default: 512)
   -s, scheduler [omp, ws] (default: omp)
   -p, enable profiling (default: disabled)
   -o, write extension output (default: disabled)
   -m <list>, comma-separated list of hardware measurements to enable (default: disabled)
              Available counters: IPC, L1CACHE, LLCACHE, BRANCHES, DTLB, ITLB
              Not recommended to enable more than 3 hw measurement per run
              given hardware counters constraints
```

To include the paths of the dependencies on `LD_LIBRARY_PATH`, you can give the following command
```source set-env.sh```
This will set all the paths for the miniGiraffe library dependencies.

miniGiraffe received two files as inputs: the sequence + seeds file and the Pangenome GBZ file. More information about the inputs is found in the [Input Section](#input-description)

miniGiraffe exposes 7 different options
- `-t` points to the number of threads to run this application in parallel. The default value is the maximum number of threads in the system (considering hyperthreading if its enabled)
- `-c` sets the initial GBWTCache capacity. GBWTCache is a feature from GBWTGraph (link to repository) that enables a software cache for workloads that repeatedly access the same nodes in the Pangneome graph multiple times. It has a dynamic capacity that increases as needed to accommodate the workload, but investigations have shown that the initial capacity set for execution can impact the application's performance. The default value is 256, and it only accepts values in powers of 2.
- `-b` sets the batch size. Each parallel thread will receive a _batch_ of _b_ reads to process each time. This parameter defines the size of the batches. Default size is 512.
- `-s` defines which parallel scheduling the application will use. There are two options available: `omp` sets the application to run with the OpenMP default scheduler (default scheduler at VG Giraffe and at miniGiraffe, too); `ws` defines the scheduler to be the work-stealing approach we implemented.
- `-p` enables profiling, outputting the time spent to map each read into a CSV format over the `stderr` output.
- `-o` write the extension output produced by miniGiraffe. It can be used to compare and validate that the proxy is generating the same matchings as VG Giraffe.
- `-m` exports hardware metrics in a CSV format. There is a list of available counters that are enabled to collection, and we recommend using no more than 3 options per run. Important to mention that, for those using Linux-based OS, to collect perf metrics, the user needs to run `sudo sysctl -w kernel.perf_event_paranoid=-1` to enable collection.

Input Description
------------------------------------------
To execute, miniGiraffe expects two different inputs
- The input with pairs of sequences + seeds, generated from VG Giraffe
- The pangenome graph in GBZ file format

We host a smaller example in the following Zenodo repository

[![DOI](https://zenodo.org/badge/doi/10.5281/zenodo.14990368.svg)](https://doi.org/10.5281/zenodo.14990368).

Users can also generate their own set of inputs using a modified version of VG Giraffe. This modified VG Giraffe version is available as a Docker Image [jessicadagostini/vg-dump:1.1](https://hub.docker.com/repository/docker/jessicadagostini/vg-dump/general).

Using the following command, the application will run the mapping and generate two files: `dump_miniGiraffe_seeds.bin` and `dump_miniGiraffe_extensions.bin`. The first contains the group of sequences + seeds needed to run the mapping process at miniGiraffe. The second is a file where the user can validate if the output of miniGiraffe is coherent and valid with the parent's application.

To collect these files, users should run:

```
docker run -v ~/path/on/host:/path/on/container \
    -w /path/on/container \
    jessicadagostini/vg-dump:1.1 \
    /vg/bin/vg giraffe \
    -Z <.gbz> -m <.min> -d <.dist> -f <.fastq> \
    -b default -t <threads> -p --track-correctness > test.gamcd
```

where `path/on/host` should refer to the host path where the files needed are available; `.gbz` refers to the GBWT format where the pangenome graph is stored; `.min` and `.dist` are VG indexes to aid in the mapping process; and the `.fastq` is the file format of the sequences to map.

The following are some suggestions of datasets to be used in the execution
 Pangenome (.gbz) | Mininimizer (.min) | Distance Index (.dist) | Fasta files (.fastq)|
| :------: | -------: | ------: | -----: |
| [Yeast Graph](https://cgl.gi.ucsc.edu/data/giraffe/mapping/graphs/generic/cactus/yeast_all/yeast_all.gbwt)  | [Yeast min](https://cgl.gi.ucsc.edu/data/giraffe/mapping/graphs/generic/cactus/yeast_all/yeast_all.min)  | [Yeast dist](https://cgl.gi.ucsc.edu/data/giraffe/mapping/graphs/generic/cactus/yeast_all/yeast_all.dist) | [SRR4074257.fastq](https://cgl.gi.ucsc.edu/data/giraffe/mapping/reads/real/yeast/SRR4074257.fastq.gz) |
| [Grch38 Graph](https://s3-us-west-2.amazonaws.com/human-pangenomics/pangenomes/freeze/freeze1/minigraph-cactus/hprc-v1.1-mc-grch38/hprc-v1.1-mc-grch38.gbz)  | [Grch38 min](https://s3-us-west-2.amazonaws.com/human-pangenomics/pangenomes/freeze/freeze1/minigraph-cactus/hprc-v1.1-mc-grch38/hprc-v1.1-mc-grch38.min)  | [Grch38 dist](https://s3-us-west-2.amazonaws.com/human-pangenomics/pangenomes/freeze/freeze1/minigraph-cactus/hprc-v1.1-mc-grch38/hprc-v1.1-mc-grch38.dist) | [D1_S1_L001_R1_004](https://s3-us-west-2.amazonaws.com/human-pangenomics/NHGRI_UCSC_panel/HG002/hpp_HG002_NA24385_son_v1/ILMN/NIST_Illumina_2x250bps/D1_S1_L001_R1_004.fastq.gz) <br /> [D1_S1_L001_R2_004](https://s3-us-west-2.amazonaws.com/human-pangenomics/NHGRI_UCSC_panel/HG002/hpp_HG002_NA24385_son_v1/ILMN/NIST_Illumina_2x250bps/D1_S1_L001_R2_004.fastq.gz) | 
| [CHM13 Graph](https://s3-us-west-2.amazonaws.com/human-pangenomics/pangenomes/freeze/freeze1/minigraph-cactus/hprc-v1.1-mc-chm13/hprc-v1.1-mc-chm13.gbz) | [CHM13 min](https://s3-us-west-2.amazonaws.com/human-pangenomics/pangenomes/freeze/freeze1/minigraph-cactus/hprc-v1.1-mc-chm13/hprc-v1.1-mc-chm13.d9.min) | [CHM13 dist](https://s3-us-west-2.amazonaws.com/human-pangenomics/pangenomes/freeze/freeze1/minigraph-cactus/hprc-v1.1-mc-chm13/hprc-v1.1-mc-chm13.d9.dist) | [D1_S1_L002_R1_001](https://s3-us-west-2.amazonaws.com/human-pangenomics/NHGRI_UCSC_panel/HG002/hpp_HG002_NA24385_son_v1/ILMN/NIST_Illumina_2x250bps/D1_S1_L002_R1_001.fastq.gz) <br/> [D1_S1_L002_R2_001](https://s3-us-west-2.amazonaws.com/human-pangenomics/NHGRI_UCSC_panel/HG002/hpp_HG002_NA24385_son_v1/ILMN/NIST_Illumina_2x250bps/D1_S1_L002_R2_001.fastq.gz) |

***Important**: to generate new data, users need a machine with minimal 48GB RAM due to the sizes of the data. Also, these pipelines require a significant time to execute, directly depending on how many parallel threads will be used. For instance, the last option in the table can take approx. 40 minutes to complete using 48 threads.*


Citation
------------------------------------------
To reference miniGiraffe in a publication, please cite the following paper
```
TO APPEAR IN THE IEEE IISWC'25 CONFERENCE PROCEEDINGS
```
