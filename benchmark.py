#! /usr/bin/python3
import subprocess
import os
import getopt
import sys
from multiprocessing import Process

# Benchmark configuration is in benchmarks.conf
config = {}

default_config = """ # Configuration file for the traversal benchmark

# Tools
ref_intr = ''                             # Reference intersection program
bvh_io = ''                               # BVH file generator
gen_prim = 'tools/GenPrimary/GenPrimary'  # Primary rays generator
gen_shadow = 'tools/GenShadow/GenShadow'  # Shadow rays generator
fbuf2png = 'tools/FBufToPng/FBufToPng'    # FBUF to PNG converter

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

    if not var_file('ref_intr') or not var_file('bvh_io') or not var_file('gen_prim') or not var_file('gen_shadow') or not var_file('fbuf2png'):
        return False

    if not var_path('obj_dir') or not var_path('bvh_dir') or not var_path('rays_dir') or not var_path('res_dir'):
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

def generate_scenes():
    for s in config['scenes']:
        print("* Scene: " + s)
        obj_file = config['obj_dir'] + "/" + s[:-3] + "obj"
        devnull = open(os.devnull, "w")
        subprocess.call([config['bvh_io'], obj_file, config['bvh_dir'] + "/" + s], stdout=devnull, stderr=devnull)

def generate_distribs():
    devnull = open(os.devnull, "w")
    # Generate primary ray distributions
    for name, viewport in config['rays'].items():
        if 'generate' not in viewport:
            continue
        if viewport['generate'] == 'primary':
            print("* Primary rays: " + name)
            eye = viewport['eye']
            ctr = viewport['center']
            up  = viewport['up']
            subprocess.call([config['gen_prim'],
                "-e", "(" + str(eye[0]) + ") (" + str(eye[1]) + ") (" + str(eye[2]) + ")",
                "-c", "(" + str(ctr[0]) + ") (" + str(ctr[1]) + ") (" + str(ctr[2]) + ")",
                "-u", "(" + str(up[0])  + ") (" + str(up[1])  + ") (" + str(up[2])  + ")",
                "-f", str(viewport['fov']),
                "-w", str(viewport['width']),
                "-h", str(viewport['height']),
                config['rays_dir'] + "/" + name])
    # Generate shadow ray distributions
    for name, viewport in config['rays'].items():
        if 'generate' not in viewport:
            continue
        if viewport['generate'] == 'shadow':
            print("* Shadow rays: " + name)
            fbuf = config['rays_dir'] + "/" + name + ".fbuf"
            primary = config['rays'][viewport['primary']]
            light = viewport['light']
            # Intersect the scene to generate the primary ray .fbuf file
            subprocess.call([config['ref_intr'],
                "-a", config['bvh_dir'] + "/" + viewport['scene'],
                "-r", config['rays_dir'] + "/" + viewport['primary'],
                "-tmin", str(primary['tmin']),
                "-tmax", str(primary['tmax']),
                "-n", "1",
                "-d", "0",
                "-o", fbuf], stdout=devnull, stderr=devnull)
            # Generate the shadow ray distribution from the result
            subprocess.call([config['gen_shadow'],
                "-p", config['rays_dir'] + "/" + viewport['primary'],
                "-d", fbuf,
                "-l", "(" + str(light[0]) + ") (" + str(light[1]) + ") (" + str(light[2]) + ")",
                config['rays_dir'] + "/" + name])

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
                errname = resdir + "/" + name + ".out"
                outname = resdir + "/" + name + ".err"
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
                    to_convert.append((resname, width, height))

        # Convert .fbuf files to .png
        for f, width, height in to_convert:
            p = Process(target=convert_fbuf, args=(f, f[:-5] + ".png", width, height))
            p.start()

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

