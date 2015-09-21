#! /usr/bin/python3
import subprocess
import os
import getopt
import sys
import time
import multiprocessing

import re

import collections

table = {}


time_type="Median" #Average, Min
print_type="megarays" #ms


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


def remove_suffix(name, suffix):
    if name.endswith(suffix):
        return name[:-len(suffix)]
    return name


def remove_prefix(name, prefix):
    if name.beginswith(suffix):
        return name[len(prefix):]
    return name


def parse_file(path):
    infile = open(path,"r")
    for line in infile:
        tp = re.findall("\#\s(Average|Median|Min):\s([0-9]+[\.[0-9]+]?)\s?(ms)",line) 
        if len(tp) > 0:
            if len(tp[0]) == 3:
                #print(tp)
                yield tp[0]
    infile.close()



def get_unique_rays(benchs):
    cmb_rays=[]
    for s, rays in config['scenes'].items():
        cmb_rays.append(rays)
    return set(cmb_rays)


def get_bench_nbr(bench_order, name):
    for i in range(len(bench_order)):
        if bench_order[i] == name:
            return i

def get_mapping(name):
    return name

def ouput_table(f):
    f.write("% Mappings\n");
    for bench in config['benches']:
        bn = os.path.basename(bench)
        f.write("\\newcommand{\\bench"+ bn + "}{" + bn + "}\n");

    for s, rays in config['scenes'].items():
        sn = remove_suffix(s, ".bvh")
        f.write("\\newcommand{\\scene"+ sn + "}{" + sn.title() + "}\n");

    f.write("\n\ctable[\n%% Options\nstar,\ndoinside=\small,\ncaption={In %s}\n]\n" % (print_type))
    f.write("% Layout\n{")

    bc = len(config['benches'])
    bc += 2   # Add first columns
    f.write("ll")
    for i in range(1, bc):
        f.write("c")
    f.write("}\n")

    bench_order=[]
    for bench in config['benches']:
        bn = os.path.basename(bench)
        bench_order.append(bn)
    
    f.write("{\n% Footnotes\n")
    footnotes={}
    for i, bn in enumerate(filter(lambda x: x in config['mappings'], bench_order)):
        letter = chr(ord('a') + i)
        footnotes[bn] = letter
        f.write("\\tnote[%c]{Speed-up w.r.t " % (letter))
        comparison = []
        for c in config['mappings'][bn]:
            comparison += ["\\bench%s{}" % c]
        f.write(", ".join(comparison) + "}\n")
    f.write("}\n")

    f.write("{\n% Table body\n\FL\n")

    f.write("& Ray Type")
    for bn in bench_order:
        f.write(" & \\bench" + bn + "{}")
        if bn in footnotes:
            f.write(" \\tmark[%c]" % footnotes[bn])
    f.write("\\ML\n")
    
    for s, rays in config['scenes'].items():
        sn = remove_suffix(s, ".bvh")

        lr = len(rays)
        cr = 0

        f.write("\\multirow{%d}{*}" % lr)
        f.write("{\pbox[t]{1.5cm} { \\scene" + sn +"{} } }")

        for r in rays:
            rn = remove_suffix(r, ".rays")        

            # Ray Type
            ray_type = config['rays'][r]['generate']
            f.write("& " + ray_type.title())

            width = config['rays'][r]['width']
            height = config['rays'][r]['width']

            nbr_of_rays = width * height

            row = {}
            for bench in config['benches']:
                bn = os.path.basename(bench)
                resdir = config['res_dir'] + "/" + bn
                name = sn + "-" + rn
                outpath = resdir + "/" + name + ".out"

                if os.path.exists(outpath):
                    for tt in parse_file(outpath):
                        if tt[0] == time_type:
                            break;

                    ms = float(tt[1])
                
                    if print_type=="megarays":
                        megarays = (nbr_of_rays * 1000 / ms) / 1000000
                        row[bn] = "%.2f" % (megarays)
                    else: 
                        row[bn] = "%.4f" % (ms)

                else:
                    row[bn] = "n/a"

            for bn in bench_order:
                f.write(" & " + row[bn])
                if bn in config['mappings']:
                    comparison = []
                    for cmp_to in config['mappings'][bn]:
                        res_a = row[bn]
                        res_b = row[cmp_to]
                        percentage = ((float(res_a) / float(res_b)) - 1) * 100
                        sign = ""
                        if percentage > 0.0:
                            sign = "+"
                        comparison.append("%s%.2f\\%%" % (sign, percentage))
                    f.write(" {(" + ", ".join(comparison) + ") }")

            # Last line must be a hline
            cr += 1
            if cr < lr :
                f.write("\\NN\n")
            else:
                f.write("\\ML\n")

    f.write("\\LL\n}\n")

def main():
   
    # Read configuration or generate config file if it doesn't exist
    if os.path.isfile("benchmark.conf"):
        f = open("benchmark.conf")
        code = compile(f.read(), "benchmark.conf", 'exec')
        exec(code, config)
    else:
        print("benchmark.conf missing")
        sys.exit()

    if not check_config():
        sys.exit()

    f = open('table.tex', 'w')
    f.write("\\documentclass{article}\n\n")                                 

    f.write("\\usepackage{multirow}\n")
    f.write("\\usepackage{float}\n")
    f.write("\\usepackage{pbox}\n")
    f.write("\\usepackage[export]{adjustbox}\n")
    f.write("\\usepackage{array}\n")

    f.write("\\usepackage{graphicx}\n")
    f.write("\\usepackage[margin=2cm]{geometry}\n\n")
    f.write("\\usepackage{ctable}\n")


    f.write("\\begin{document}\n")


    ouput_table(f)

    f.write("\\end{document}\n")
   # print_table_tex(table);
    

if __name__ == "__main__":
    main()
