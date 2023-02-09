# smoothxg

[![build and test](https://github.com/pangenome/smoothxg/actions/workflows/small_test_on_push.yml/badge.svg)](https://github.com/pangenome/smoothxg/actions/workflows/small_test_on_push.yml)
[![install with bioconda](https://img.shields.io/badge/install%20with-bioconda-brightgreen.svg?style=flat)](https://anaconda.org/bioconda/smoothxg)

## local reconstruction of variation graphs using partial order alignment

Pangenome graphs built from raw sets of alignments may have complex local structures generated by common patterns of genome variation.
These local nonlinearities can introduce difficulty in downstream analyses, visualization, and interpretation of variation graphs.

`smoothxg` finds blocks of paths that are collinear within a variation graph.
It applies partial order alignment to each block, yielding an acyclic variation graph.
Then, to yield a "smoothed" graph, it walks the original paths to lace these subgraphs together.
The resulting graph only contains cyclic or inverting structures larger than the chosen block size, and is otherwise manifold linear.
In addition to providing a linear structure to the graph, `smoothxg` can be used to extract the consensus pangenome graph by applying the _heaviest bundle_ algorithm to each chain.

# algorithm sketch

To find blocks, `smoothxg` applies a greedy algorithm that assumes that the graph nodes are sorted according to their occurence in the graph's embedded paths.
The path-guided stochastic gradient descent based 1D sort implemented in `odgi sort -Y` is designed to provide this kind of sort.
This sort is similar to a 1-dimensional graph layout.
After finding blocks

## input

`smoothxg` can operate an any input variation graph in GFA format.
The graph must have sequences represented as paths in P records, while the topology of the graph is in S and L records.
Path names should be unique.
`seqwish` is a standard way to make such a graph.

## installation

### building from source

`smoothxg` uses cmake to build itself and its dependencies. At least GCC version 9.3.0 is required for compilation. 
You can check your version via:

```
gcc --version
g++ --version
```

Clone the `smoothxg` git repository and build with:

```
sudo apt-get update && sudo apt-get install -y libatomic-ops-dev libgsl-dev zlib1g-dev libzstd-dev libjemalloc-dev

git clone --recursive https://github.com/pangenome/smoothxg.git
cd smoothxg
cmake -H. -Bbuild && cmake --build build -- -j 4
```

`libzstd-dev` must be of version 1.4 or higher.

#### Notes for distribution

If you need to avoid machine-specific optimizations, use the `CMAKE_BUILD_TYPE=Generic` build type:

```shell
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Generic && cmake --build build -- -j 3
```

### Bioconda

`smoothxg` recipes for Bioconda are available at https://anaconda.org/bioconda/smoothxg.
To install the latest version using `Conda` execute:

``` bash
conda install -c bioconda smoothxg
```

### Guix

#### installing via the guix-genomics git repository

First, clone the guix-genomics repository:

``` bash
git clone https://github.com/ekg/guix-genomics
```

And install the `smoothxg` package to your default GUIX environment:

``` bash
GUIX_PACKAGE_PATH=. guix package -i smoothxg
```

Now `smoothxg` is available as a global binary installation.

#### installing via the guix-genomics channel

Add the following to your ~/.config/guix/channels.scm:

``` scm
  (cons*
(channel
  (name 'guix-genomics)
  (url "https://github.com/ekg/guix-genomics.git")
  (branch "master"))
%default-channels)
```

First, pull all the packages, then install `smoothxg` to your default GUIX environment:

``` bash
guix pull
guix package -i smoothxg
```

If you want to build an environment only consisting of the `smoothxg` binary, you can do:

``` bash
guix environment --ad-hoc smoothxg
```

For more details about how to handle Guix channels, go to https://git.genenetwork.org/guix-bioinformatics/guix-bioinformatics.git.



#### Notes for debugging

To enable the `-A/--write-split-block-fastas` and `--write-poa-block-fastas` options and emit a table 
with POA block statistics, add the `-DPOA_DEBUG=ON` option:

```shell
cmake -H. -Bbuild -D CMAKE_BUILD_TYPE=Release -DPOA_DEBUG=ON && cmake --build build -- -j 3
```
