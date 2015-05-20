#! /usr/bin/python3
import subprocess
import os
import getopt
import sys

# Benchmark configuration is in benchmarks.conf
config = {}

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
    

    if not var_file('bvh_io') or not var_file('gen_prim') or not var_file('fbuf2png'):
        return False

    if not var_path('models') or not var_path('scenes') or not var_path('distribs') or not var_path('results'):
        return False

    if not check_var('benches'):
        return False

    for prg in config['benches']:
        if not os.path.isfile(prg):
            print("Benchmark program '" + prg + "' is not a file.")
            return False

    return True

def generate_config():
    # Generates a default configuration file
    f = open("benchmarks.conf", "w")
    f.write("# Configuration file for the traversal benchmark\n"
            "\n"
            "# Tools\n"
            "bvh_io = ''                               # BVH file generator\n"
            "gen_prim = 'tools/GenPrimary/GenPrimary'  # Primary rays generator\n"
            "fbuf2png = 'tools/FBufToPng/FBufToPng'    # FBUF to PNG converter\n"
            "\n"
            "# Directories\n"
            "models = 'models'     # OBJ models directory\n"
            "scenes = 'scenes'     # BVH file directory\n"
            "distribs = 'distribs' # Ray distribution directory\n"
            "results = 'results'   # Benchmark results directory\n"
            "\n"
            "# Benchmark programs\n"
            "benches = []\n"
            "\n"
            "# Benchmark parameters\n"
            "runs = 1\n"
            "dryruns = 0\n")
    f.close()

def generate_scenes():
    # Generates the BVH files with bv_io based on models/*.obj
    for f in os.listdir(config['models']):
        out = config['scenes'] + "/" + f[:-3] + "bvh"
        if f.endswith(".obj") and not os.path.isfile(out):
            subprocess.call([config['bvh_io'], config['models'] + "/" + f, out])

def generate_distribs():
    # Generates the ray distributions based on distribs/list.txt
    dist_names = {}
    for l in open(config['distribs'] + "/" + "list.txt"):
        l = l.strip()
        if l[0] == '#' or len(l) == 0:
            continue

        params = l.split()
        name = config['distribs'] + "/" + "dist-" + ",".join(params)
        name = params[0]
        if name in dist_names:
            dist_names[name] += 1
        else:
            dist_names[name] = 1
        name += "%02d" % dist_names[name] + ".rays"

        eye = (params[1], params[2], params[3])
        ctr = (params[4], params[5], params[6])
        up  = (params[7], params[8], params[9])
        fov = params[10]
        w   = params[11]
        h   = params[12]
        subprocess.call([config['gen_prim'],
            "-e", "(" + eye[0] + ") (" + eye[1] + ") (" + eye[2] + ")",
            "-c", "(" + ctr[0] + ") (" + ctr[1] + ") (" + ctr[2] + ")",
            "-u", "(" + up[0]  + ") (" + up[1]  + ") (" + up[2]  + ")",
            "-f", fov,
            "-w", w,
            "-h", h,
            config['distribs'] + "/" + name])

def usage():
    print("usage: benchmark.py [options]\n"
          "    -h --help : Displays this message\n"
          "    --gen-scenes : Generates the scene files\n"
          "    --gen-distribs : Generates the ray distribution files\n")

def benchmark():
    # Get the list of scenes
    scenes = []
    for f in os.listdir(config['scenes']):
        if f.endswith(".bvh"):
            scenes.append(f[:-4])

    # Get the list of ray distributions for each scene
    distribs = {}
    for s in scenes:
        distribs[s] = []

    for f in os.listdir(config['distribs']):
        if f.endswith(".rays"):
            # Add the distribution to each scene that is mentioned in its name
            for s in scenes:
                if f.find(s) >= 0:
                    distribs[s].append(f[:-5])
                        

    # Run the benchmarks sequentially
    for bench in config['benches']:
        print("* Program : " + bench)
        resdir = config['results'] + "/" + os.path.basename(bench)
        if not os.path.exists(resdir):
            os.makedirs(resdir)

        for s in scenes:
            for d in distribs[s]:
                name = s + "-" + d
                print("   scene: " + s + ", distrib: " + d)
                out = open(resdir + "/" + name + ".out", "w")
                err = open(resdir + "/" + name + ".err", "w")
                subprocess.call([bench,
                    "-a", config['scenes'] + "/" + s + ".bvh",
                    "-r", config['distribs'] + "/" + d + ".rays",
                    #"-tmin", tmin,
                    #"-tmax", tmax,
                    "-n", str(config['runs']),
                    "-d", str(config['dryruns']),
                    resdir + "/" + name + ".fbuf"], stdout=out, stderr=err)
                out.close()
                err.close()

def main():
    global config

    # Read configuration or generate config file if it doesn't exist
    if os.path.isfile("benchmarks.conf"):
        f = open("benchmarks.conf")
        code = compile(f.read(), "benchmarks.conf", 'exec')
        exec(code, config)
    else:
        generate_config()
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

