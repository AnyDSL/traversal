# Traversal #

This is the main repository for the AnyDSL traversal code.

# Requirements

The traversal code comes in two flavours: 
  * A CPU version, which requires an x86 processor with the AVX, AVX2 and FMA instruction sets.
  * A GPU version, based on CUDA, which requires a Maxwell GPU or a higher model.

### Building ###

Run CMake and set the `BACKEND` variable to `cpu` to generate the CPU version, or `nvvm` to generate the GPU version:

    git clone https://user@github.com/AnyDSL/traversal.git
    cd traversal
    mkdir build
    cd build
    cmake-gui ..

### Running ###

The frontend benchmarks the traversal code and generates an output which contains the intersection parameters.
It takes as input the scene file (which contains the acceleration structure) and the ray distribution.
Run it with `-h` to get the list of options.

When runnning the traversal on the GPU, make sure to run the executables in the directory where the file containing the compiled kernels (e.g. `traversal.nvvm` for the NVVM backend) is located.

A sample BVH file and a primary ray distribution are provided for testing. You can use them with the frontend, like so:

    cd build/src
    ./frontend -a ../../testing/sibenik.bvh -r ../../testing/sibenik01.rays -n 80 -d 20 -o output.fbuf
    ./fbuf2pnf output.fbuf image.png

You can also use the BVH file with the `viewer` utility:

    cd build/src
    ./viewer -a ../../testing/sibenik.bvh

### Tools ###

This repository also includes some tools to generate ray distributions for primary rays, and to convert the output
of the frontend into a PNG image. You need libpng installed to compile them. Run them with `-h` to get the list of options.
