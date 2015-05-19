#! /usr/bin/python3
import subprocess
import os
import getopt
import sys

# Tools
bvh_io   = '/space/perard/sources/benchsiggraphasia15/build/tools/bvh_io/imbaBVHio'
gen_prim = 'tools/GenPrimary/GenPrimary'
fb2png   = 'tools/FBufToPng/FBufToPng'

models   = '/space/perard/sources/imbatracer/models'    # OBJ files directory (input)
scenes   = 'scenes'                                     # BVH files directory (output)
distribs = 'distribs'                                   # Ray distributions directory (input, output)

def generate_scenes():
    for f in os.listdir(models):
        out = scenes + "/" + f[:-3] + "bvh"
        if f.endswith(".obj") and not os.path.isfile(out):
            subprocess.call([bvh_io, models + "/" + f, out])

def generate_distribs():
    for l in open(distribs + "/" + "list.txt"):
        l = l.strip()
        if l[0] == '#' or len(l) == 0:
            continue

        params = l.split()
        name = distribs + "/" + "dist-" + ",".join(params)
        eye = (params[0], params[1], params[2])
        ctr = (params[3], params[4], params[5])
        up  = (params[6], params[7], params[8])
        fov = params[9]
        w   = params[10]
        h   = params[11]
        subprocess.call([gen_prim,
            "-e (" + eye[0] + ") (" + eye[1] + ") (" + eye[2] + ")",
            "-c (" + ctr[0] + ") (" + ctr[1] + ") (" + ctr[2] + ")",
            "-u (" + up[0]  + ") (" + up[1]  + ") (" + up[2]  + ")",
            "-f " + fov,
            "-w " + w,
            "-h " + h,
            name])

def usage():
    print("usage: benchmark.py [options]\n"
          "    -h --help : Displays this message\n"
          "    --gen-scenes : Generates the scene files\n"
          "    --gen-distribs : Generates the ray distribution files\n")

def benchmark():
    print("1 2 3")

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "h", ["help", "gen-scenes", "gen-distribs"])
    except getopt.GetoptError as err:
        print(err)
        usage()
        sys.exit(2)

    gen_scenes = False
    gen_distribs = False
    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit(0)
        elif o == "--gen-scenes":
            gen_scenes = True
        elif o == "--gen-distribs":
            gen_distribs = True

    if gen_scenes:
        print("Generating scenes...")
        generate_scenes()

    if gen_distribs:
        print("Generating distributions...")
        generate_distribs()

    print("Benchmarking...")
    benchmark()

if __name__ == "__main__":
    main()

