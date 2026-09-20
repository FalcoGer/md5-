# md5++
md5sum written in modern c++26.

The utility uses boost's memory mapped file source, which is then placed into a lazy pipe that chunks most of the file into the 64 byte hash function. The last remaining is then concatenated into a view with the padding and file size.

Static assertions are used to catch UB and test the functionality at compile time.

The main function uses boost's thread pool for parallel execution. On my SSD the speedup is substatial.

## Performance
All values compared to `md5sum (uutils coreutils) 0.8.0`.
Compiled with ninja using cmake's `Release` configuration with `gcc (GCC) 17.0.0 20260831 (experimental)`. Slightly worse performance (about +3% of coreutils was achieved with `Ubuntu clang version 23.0.0 (++20260707085028+ec9e62cb609a-1~exp1~20260707085040.121)` linked against the gcc standard library.
The iso files I used for testing were just the first large files that I found that had on hand.
All tests were ran on my Lenovo ThinkPad E16 using an AMD Ryzen 5 7535U, 32GB of memory and running Ubuntu 26.04 LTS.

### Runtime

#### `~/VirtualMachines/**/*`

```
find ~/VirtualMachines/ -type f -exec stat -c %s {} + | awk '{s+=$1} END {print s " bytes (" s/1024/1024 " MB)"}'
49999727371 bytes (47683.5 MB)

files=(~/VirtualMachines/**/*)
echo $#files
61

time ./md5++ ~/VirtualMachines/**/*
[...]
./md5++ ~/VirtualMachines/**/*  107,97s user 55,97s system 575% cpu 28,489 total

time md5sum ~/VirtualMachines/**/*
md5sum ~/VirtualMachines/**/*  77,15s user 23,58s system 98% cpu 1:42,01 total
```

#### Win11_EnglishInternational_x64v1.iso

```
wc --bytes ~/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
5567117312 /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
```

| Metric | `./build/md5++` (Run 1 / Cold) | `./build/md5++` (Runs 2–10 Warm) | GNU `md5sum` (Mean) |
| :--- | :--- | :--- | :--- |
| **Total Time** | 13.05s | **8.92s ± 0.07s** | 9.01s ± 0.03s |
| **User Time** | 8.68s | 8.31s | 8.27s |
| **System Time** | 1.64s | **0.56s** | 0.71s |
| **CPU Utilization** | 79% | 98.9% | 99.0% |

My best guess for why the first run is an outlier is that the system takes that much longer to memory map the file. Once done the map is kept cached for subsequent runs, resulting in lower runtimes. The same happens even if md5sum is running first.

##### Raw Data

Running time 10 times

```bash
for i in {1..10}; do
    time ./build/md5++ "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"
    time md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"
done
```

```
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,68s user 1,64s system 79% cpu 13,053 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,28s user 0,72s system 99% cpu 9,033 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,32s user 0,57s system 99% cpu 8,928 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,32s user 0,70s system 99% cpu 9,041 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,34s user 0,57s system 98% cpu 9,044 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,26s user 0,71s system 99% cpu 9,041 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,33s user 0,53s system 99% cpu 8,897 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,28s user 0,69s system 99% cpu 9,028 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,37s user 0,52s system 99% cpu 8,983 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,28s user 0,72s system 99% cpu 9,031 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,32s user 0,58s system 99% cpu 8,922 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,21s user 0,73s system 99% cpu 8,950 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,20s user 0,60s system 99% cpu 8,820 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,24s user 0,70s system 99% cpu 8,954 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,28s user 0,56s system 99% cpu 8,845 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,30s user 0,68s system 99% cpu 8,998 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,30s user 0,55s system 99% cpu 8,874 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,28s user 0,71s system 99% cpu 9,024 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
./build/md5++   8,37s user 0,56s system 99% cpu 8,952 total
5d6f14dc16f0433fcc2dff3454b5c8a2  /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso
md5sum "$HOME/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"  8,28s user 0,74s system 99% cpu 9,037 total
```

#### WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO

```
❯ wc --bytes ~/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.\(WPE\).ISO
3477340160 /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
```

| Metric | `./build/md5++` (Run 1 / Cold) | `./build/md5++` (Runs 2–10 Warm) | GNU `md5sum` (Mean) |
| :--- | :--- | :--- | :--- |
| **Total Time** | 7.05s | **5.61s ± 0.04s** | 5.66s ± 0.07s |
| **User Time** | 5.42s | 5.24s | 5.20s |
| **System Time** | 1.60s | **0.35s** | 0.44s |
| **CPU Utilization** | 99% | 99.0% | 99.0% |

##### Raw Data

```bash
for i in {1..10}; do
    time ./build/md5++ ~/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.\(WPE\).ISO
    time md5sum ~/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.\(WPE\).ISO
done
```

```
./build/md5++ ~/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.\(WPE\).ISO
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,42s user 1,60s system 99% cpu 7,054 total
md5sum   5,18s user 0,42s system 99% cpu 5,613 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,34s user 0,33s system 99% cpu 5,680 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,27s user 0,42s system 99% cpu 5,709 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,30s user 0,35s system 99% cpu 5,660 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,24s user 0,46s system 99% cpu 5,714 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,27s user 0,36s system 99% cpu 5,641 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,22s user 0,43s system 99% cpu 5,663 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,21s user 0,35s system 99% cpu 5,567 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,18s user 0,44s system 99% cpu 5,633 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,18s user 0,36s system 99% cpu 5,552 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,15s user 0,46s system 99% cpu 5,626 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,22s user 0,35s system 99% cpu 5,620 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,18s user 0,43s system 99% cpu 5,639 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,22s user 0,34s system 99% cpu 5,600 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,20s user 0,39s system 99% cpu 5,621 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,22s user 0,34s system 99% cpu 5,572 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,15s user 0,44s system 99% cpu 5,606 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
./build/md5++   5,20s user 0,36s system 99% cpu 5,582 total
1a68b357113d168dd001295396782969  /home/user/VirtualMachines/images/WIN11.PRO.22H2.SUPERLITE+SE+COMPACT.U3.(WPE).ISO
md5sum   5,23s user 0,54s system 99% cpu 5,823 total
```


### Memory

#### Running time -v

Mine:

```
	Command being timed: "./build/md5++ /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"
	User time (seconds): 8.28
	System time (seconds): 0.55
	Percent of CPU this job got: 99%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:08.84
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 5441256
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 0
	Minor (reclaiming a frame) page faults: 85162
	Voluntary context switches: 1
	Involuntary context switches: 588
	Swaps: 0
	File system inputs: 0
	File system outputs: 0
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0
```

coreutils

```
	Command being timed: "md5sum /home/user/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso"
	User time (seconds): 8.24
	System time (seconds): 0.69
	Percent of CPU this job got: 99%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:08.97
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 7564
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 0
	Minor (reclaiming a frame) page faults: 458
	Voluntary context switches: 1
	Involuntary context switches: 912
	Swaps: 0
	File system inputs: 0
	File system outputs: 0
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0
```

#### Running heaptrack

Mine:

```
heaptrack ./build/md5++ ~/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso

heaptrack stats:
	allocations:          	15
	leaked allocations:   	1
	temporary allocations:	2
```

coreutils

```
heaptrack md5sum ~/VirtualMachines/images/Win11_EnglishInternational_x64v1.iso

heaptrack stats:
	allocations:          	390
	leaked allocations:   	99
	temporary allocations:	20
```
