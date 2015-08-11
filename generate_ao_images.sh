#!/bin/bash

nbr_of_samples=$1

./build/src/frontend -a ./benchmarks/bvhs/powerplant.bvh -r ./benchmarks/distribs/powerplant.rays -n 1 -tmax 1e9 -d 0 -ao ao-powerplant.fbuf -aotmax 10000.0 -aotmin 1.0 -aos ${nbr_of_samples}

./build/src/fbuf2png ao-powerplant.fbuf ao-powerplant.png

./build/src/frontend -a ./benchmarks/bvhs/sibenik.bvh -r ./benchmarks/distribs/sibenik.rays -n 1 -d 0 -ao ao-sibenik.fbuf -aotmax 5.f -aotmin 0.01 -aos ${nbr_of_samples}

./build/src/fbuf2png ao-sibenik.fbuf ao-sibenik.png

./build/src/frontend -a ./benchmarks/bvhs/sanmiguel.bvh -r ./benchmarks/distribs/sanmiguel.rays -n 1 -d 0 -ao ao-sanmiguel.fbuf -aotmax 5.f -aotmin 0.01 -aos ${nbr_of_samples}

./build/src/fbuf2png ao-sanmiguel.fbuf ao-sanmiguel.png

./build/src/frontend -a ./benchmarks/bvhs/sponza.bvh -r ./benchmarks/distribs/sponza.rays -n 1 -d 0 -ao ao-sponza.fbuf -aotmax 500.f -aotmin 0.01 -aos ${nbr_of_samples}

./build/src/fbuf2png ao-sponza.fbuf ao-sponza.png

./build/src/frontend -a ./benchmarks/bvhs/conference.bvh -r ./benchmarks/distribs/conference.rays -n 1 -d 0 -ao ao-conference.fbuf -aotmax 280.f -aotmin 0.01 -aos ${nbr_of_samples}

./build/src/fbuf2png ao-conference.fbuf ao-conference.png

