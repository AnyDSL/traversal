#! /usr/bin/python3
import subprocess
import os
import getopt
import sys
import time
from multiprocessing import Process

# Benchmark configuration is in benchmarks.conf
config = {}

default_config = """ # Configuration file for the traversal benchmark

# Tools
ref_intr = ''                             # Reference intersection program
bvh_io = ''                               # BVH file generator
gen_prim = 'tools/GenPrimary/GenPrimary'  # Primary rays generator
gen_shadow = 'tools/GenShadow/GenShadow'  # Shadow rays generator
gen_random = 'tools/GenRandom/GenRandom'  # Random rays generator
fbuf2png = 'tools/FBufToPng/FBufToPng'    # FBUF to PNG converter

# Generator tools options
num_cores = 1           # Number of cores used when generating BVH and ray files

# Directories
obj_dir  = 'models'     # OBJ models directory
bvh_dir  = 'scenes'     # BVH file directory
rays_dir = 'distribs'   # Ray distribution directory
res_dir  = 'results'    # Benchmark results directory

# Benchmark programs
benches = []

# Benchmark parameters
runs = 1                # Number of iterations
dryruns = 0             # Number of warmup iterations

# Dataset
rays = {}               # Dictionary of ray distributions associated with their parameters
scenes = {}             # Dictionary of BVH files associated with their distributions
"""

def check_config():
    # Checks that variables in the configuration file are valid
    def check_var(var):
        if var not in config:
            print("Variable '" + var + "' missing from configuration file.")
            return False
        return True
    
    def var_file(var):
        if not check_var(var):
            return False
        if not os.path.isfile(config[var]):
            print("Configuration file variable '" + var + "' should be a valid file.")
            return False
        return True

    def var_path(var):
        if not check_var(var):
            return False
        if not os.path.isdir(config[var]):
            print("Configuration file variable '" + var + "' should be a valid directory.")
            return False
        return True

    if not check_var('runs') or not check_var('dryruns'):
        return False

    for f in ['ref_intr', 'bvh_io', 'gen_prim', 'gen_shadow', 'gen_random', 'fbuf2png']:
        if not var_file(f):
            return False

    if not var_path('obj_dir') or not var_path('bvh_dir') or not var_path('rays_dir') or not var_path('res_dir'):
        return False

    if not check_var('num_cores'):
        return False

    if not check_var('benches'):
        return False

    for prg in config['benches']:
        if not os.path.isfile(prg):
            print("Benchmark program '" + prg + "' is not a file.")
            return False

    if not check_var('runs') or not check_var('dryruns') or not check_var('rays') or not check_var('scenes'):
        return False

    return True

def spawn_silent(msg, params):
    def call_silent(msg, params):
        if msg != "":
            print(msg)
        devnull = open(os.devnull, "w")
        subprocess.call(params)#, stdout=devnull, stderr=devnull)
        devnull.close()

    p = Process(target=call_silent, args=(msg, params))
    return p

def run_parallel(p):
    n = config['num_cores']
    for i in range(0, len(p), n):
        for q in p[i:i + n]:
            q.start()
        for q in p[i:i + n]:
            q.join()

def generate_scenes():
    p = []
    for s in config['scenes']:
        obj_file = config['obj_dir'] + "/" + s[:-3] + "obj"
        p.append(spawn_silent("* Scene: " + s, [config['bvh_io'], obj_file, config['bvh_dir'] + "/" + s]))
    run_parallel(p)

def generate_distribs():
    devnull = open(os.devnull, "w")

    p = []
    # Generate primary ray distributions
    for name, viewport in config['rays'].items():
        if viewport['generate'] == 'primary':
            eye = viewport['eye']
            ctr = viewport['center']
            up  = viewport['up']
            p.append(spawn_silent("* Primary rays: " + name, [config['gen_prim'],
                "-e", "(" + str(eye[0]) + ") (" + str(eye[1]) + ") (" + str(eye[2]) + ")",
                "-c", "(" + str(ctr[0]) + ") (" + str(ctr[1]) + ") (" + str(ctr[2]) + ")",
                "-u", "(" + str(up[0])  + ") (" + str(up[1])  + ") (" + str(up[2])  + ")",
                "-f", str(viewport['fov']),
                "-w", str(viewport['width']),
                "-h", str(viewport['height']),
                config['rays_dir'] + "/" + name]))
        elif viewport['generate'] == 'random':
            p.append(spawn_silent("* Random rays: " + name, [config['gen_random'],
                "-b", config['bvh_dir'] + "/" + viewport['scene'],
                "-s", str(int(time.time())),
                "-w", str(viewport['width']),
                "-h", str(viewport['height']),
                config['rays_dir'] + "/" + name]))
    run_parallel(p)

    p = []
    q = []
    # Generate shadow ray distributions
    for name, viewport in config['rays'].items():
        if viewport['generate'] == 'shadow':
            fbuf = config['rays_dir'] + "/" + name + ".fbuf"
            primary = config['rays'][viewport['primary']]
            light = viewport['light']
            # Intersect the scene to generate the primary ray .fbuf file
            p.append(spawn_silent("* Finding intersections: " + name, [config['ref_intr'],
                "-a", config['bvh_dir'] + "/" + viewport['scene'],
                "-r", config['rays_dir'] + "/" + viewport['primary'],
                "-tmin", str(primary['tmin']),
                "-tmax", str(primary['tmax']),
                "-n", "1",
                "-d", "0",
                "-o", fbuf]))
            # Generate the shadow ray distribution from the result
            q.append(spawn_silent("* Shadow rays: " + name, [config['gen_shadow'],
                "-p", config['rays_dir'] + "/" + viewport['primary'],
                "-d", fbuf,
                "-l", "(" + str(light[0]) + ") (" + str(light[1]) + ") (" + str(light[2]) + ")",
                config['rays_dir'] + "/" + name]))

    run_parallel(p)
    run_parallel(q)

def usage():
    print("usage: benchmark.py [options]\n"
          "    -h --help : Displays this message\n"
          "    --gen-scenes : Generates the scene files\n"
          "    --gen-distribs : Generates the ray distribution files\n")

def remove_if_empty(name):
    if os.stat(name).st_size == 0:
        os.remove(name)

def remove_suffix(name, suffix):
    if name.endswith(suffix):
        return name[:-len(suffix)]
    return name

def convert_fbuf(fbuf, png, w, h):
    devnull = open(os.devnull, "w")
    subprocess.call([config['fbuf2png'], fbuf, png, "-w", str(w), "-h", str(h)], stdout=devnull, stderr=devnull)

def benchmark():
    # Run the benchmarks sequentially
    for bench in config['benches']:
        print("* Program: " + bench)
        resdir = config['res_dir'] + "/" + os.path.basename(bench)
        if not os.path.exists(resdir):
            os.makedirs(resdir)

        # Call the benchmark program
        to_convert = []
        for s, rays in config['scenes'].items():
            for r in rays:
                name = remove_suffix(s, ".bvh") + "-" + remove_suffix(r, ".rays")
                print("   scene: " + s + ", distrib: " + r)
                errname = resdir + "/" + name + ".err"
                outname = resdir + "/" + name + ".out"
                resname = resdir + "/" + name + ".fbuf"
                tmin = config['rays'][r]['tmin']
                tmax = config['rays'][r]['tmax']
                width = config['rays'][r]['width']
                height = config['rays'][r]['height']
                subprocess.call([bench,
                    "-a", config['bvh_dir'] + "/" + s,
                    "-r", config['rays_dir'] + "/" + r,
                    "-tmin", str(tmin),
                    "-tmax", str(tmax),
                    "-n", str(config['runs']),
                    "-d", str(config['dryruns']),
                    "-o", resname], stdout=open(outname, "w"), stderr=open(errname, "w"))

                remove_if_empty(errname)
                remove_if_empty(outname)
                if os.path.isfile(resname):
                    to_convert.append(spawn_silent("", [config['fbuf2png'], resname, resname[:-5] + ".png", "-w", str(width), "-h", str(height)]))

        # Convert .fbuf files to .png
        run_parallel(to_convert)

def main():
    global config

    # Read configuration or generate config file if it doesn't exist
    if os.path.isfile("benchmark.conf"):
        f = open("benchmark.conf")
        code = compile(f.read(), "benchmark.conf", 'exec')
        exec(code, config)
    else:
        print("Running for the first time, configuration file benchmark.conf will be generated.")
        f = open("benchmark.conf", "w")
        f.write(default_config)
        f.close()
        sys.exit()

    if not check_config():
        sys.exit()

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

