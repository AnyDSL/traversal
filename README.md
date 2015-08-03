# Traversal #

This is the main repository for the impala traversal code.

### Building ###

Run CMake and set the `BACKEND` variable to _cpu_ to generate the CPU version, or _nvvm_ to generate the GPU version.

### Running ###

The frontend benchmarks the traversal code and generates an output which contains the intersection parameters.
It takes as input the scene file (which contains the acceleration structure) and the ray distribution.
Run it with `-h` to get the list of options.

### Tools ###

This repository also includes some tools to generate ray distributions for primary rays, and to convert the output
of the frontend into a PNG image. You need libpng installed to compile them. Run them with `-h` to get the list of options.
