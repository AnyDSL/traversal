# README #

This is the main repository for the impala traversal code.

### Building ###

You can run CMake to generate the different traversal variants by changing the following variables :

* BACKEND : defines the backend to generate code for (supported values: cpu, avx, nvvm, cuda)
* VECTORIZE : defines manual vector size to 1, 4, 8, or 16 (only supported in the cpu backend)
* ACCEL : defines the acceleration structure (bvh is the only supported value for now)

### Running ###

The frontend benchmarks the traversal code and generates an output which contains the intersection parameters.
It takes as input the scene file (which contains the acceleration structure) and the ray distribution.
Run it with -h to get the list of options.

### Tools ###

This repository also includes some tools to generate ray distributions for primary rays, and to convert the output
of the frontend into a PNG image. They are written in Haskell, and require GHC and additional packages. Run `cabal
configure/build` in the tools directory.
