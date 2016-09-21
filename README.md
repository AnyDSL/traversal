# Traversal #

This is the main repository for the AnyDSL traversal code.

# Requirements

For the CPU version, an x86 processor with the AVX, AVX2 and FMA instruction sets is required.
For the GPU version, an nVidia Maxwell 

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

### Tools ###

This repository also includes some tools to generate ray distributions for primary rays, and to convert the output
of the frontend into a PNG image. You need libpng installed to compile them. Run them with `-h` to get the list of options.
