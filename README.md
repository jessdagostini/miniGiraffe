# miniGiraffe: proxy application from Pangenomics mapping
miniGiraffe is a proxy application based on [VG Giraffe](https://github.com/vgteam/vg) Mapping Tool for computational improvements exploration of Pangenome mapping kernels. It provides an easier and more lightweight application to test the behavior of a Pangeomics kernel in different enrivonments. It is coded in C++14 (following VG Giraffe original code) and parallelized using OpenMP and C++ Threads.


Building
------------------------------------------
Clone this repository using the recursive option (to download dependencies from `deps` folder)

```git clone --recursive git@github.com:jessdagostini/miniGiraffe.git```

After the download, navigate to the folder. We first install the dependencies, and then build miniGiraffe. It's about two command lines to have miniGiraffe ready to run.

```
bash install-deps.sh
make miniGiraffe
```

Input Description
------------------------------------------
To execute, miniGiraffe expects two different inputs
- The input with pairs of sequences + seeds, generated from VG Giraffe
- The pangenome graph in GBZ file format

Given the size of the inputs, they are not available directly in this repository.
We host four input sets in this link.

Users can also generate their own set of inputs using a modified version of VG Giraffe available in this Docker image. To generate new binaries with the pairs of squences + seeds, follow below steps:

```
Steps to generate input with Docker
```

Running miniGiraffe
------------------------------------------
Having the input files, you can execute miniGiraffe using

```
Usage ./miniGiraffe [/path/to/seeds/dump.bin] [/path/to/gbz/file.gbz] [options]
Options: 
   -t, number of threads (default: max # threads in system)
   -b, batch size (default: 512)
   -s, scheduler [omp, ws] (default: omp)
   -p, enable profiling (default: disabled)
   -m <list>, comma-separated list of hardware measurement to enable (default: disabled)
              Available counters: IPC, L1CACHE, LLCACHE, BRANCHES, DTLB, ITLB
              Not recommended to enable more than 3 hw measurement per run
              given hardware counters constraints
```

There are two possible ways to run miniGiraffe in parallel. First is using OpenMP dynamic scheduler, identified as `omp`. The second is using the work-stealing scheduler identified as `ws`. Default scheduler is `omp`.

miniGiraffe maps batches of sequences spread across multiple threads, processing one sequence at a time in each thread. If enabling profiling (`p` option), it will time the execution of each mapped sequence, and will output at the end of the execution over `std` output. miniGiraffe also generates a .bin file with the extensions found over it's execution. This file can be used to comparison and validation of the procedure made by miniGiraffe and Giraffe.


Citation
------------------------------------------
To reference miniGiraffe in a publication, please cite the following paper