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


time_type="Median"

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





def ouput_table(f):

    f.write("\\begin{table}\n")

    #Table Header
    #uni_benches = get_unique_benches()
    bc = len(config['benches'])
    bc+=3   #Add first columns
    f.write("\\begin{tabular}{")
    for i in range(bc):
        f.write("|l")
    f.write("|")
    f.write("} \n") 
    f.write("\\hline\n")

    #Table 'Headline'
    f.write(" & Ray Type"),
    f.write(" & Image"),
    for bench in config['benches']:
        bn = os.path.basename(bench)
        f.write(" & " + bn),
    f.write("\\\\\n")
    
    f.write("\\hline\n")


    for s, rays in config['scenes'].items():
        sn = remove_suffix(s, ".bvh")
        #print(sn)
        lr = len(rays)
        f.write("\\multirow{%d}{*}" % lr)
        f.write("{" + sn +"}")

        lr = len(rays)
        cr=0    

        for r in rays:
            rn = remove_suffix(r, ".rays")        
                    
            #f.write("& " + rn)
            #Ray Type
            ray_type = config['rays'][r]['generate']
            f.write("& " + ray_type)

            cb=0
            for bench in config['benches']:
                bn = os.path.basename(bench)
                resdir = config['res_dir'] + "/" + bn
                name = sn + "-" + rn
                
                outpath = resdir + "/" + name + ".out"
                if cb==0:
                    pngpath = resdir + "/" + name + ".png"
                    f.write(" & ");
                    if(ray_type != "random"):
                        #f.write("{\\begin{figure}[ht]\n")
                        #f.write("\\centering\n")
                        f.write("{\\includegraphics[valign=c, width=0.08\\textwidth]{" + pngpath + "} } ")                        
                        #f.write("{\\end{figure} }\n")
                cb+=1

                if os.path.exists(outpath):
                    for tt in parse_file(outpath):
                        if tt[0] == time_type:
                            break;
                    f.write(" & %s " % (tt[1]))
                else:
                    f.write(" & n/a")
            f.write("\\\\ \n")

            #Last line must be a hline
            cr+=1
            if cr<lr :
                f.write("\cline{%d-%d}\n" % (2, bc))
            else: 
                f.write("\hline\n")


            #print("\t" + rn)
    #print("\hline")

    f.write("\\end{tabular}\n")
    f.write("\\end{table}\n")

            
def main():
   
    # Read configuration or generate config file if it doesn't exist
    if os.path.isfile("benchmark.conf"):
        f = open("benchmark.conf")
        code = compile(f.read(), "benchmark.conf", 'exec')
        exec(code, config)
    else:
        print("benchmark.conf missing")
        sys.exit()

   # if not check_config():
   #        sys.exit()
    f = open('table.tex', 'w')
    f.write("\\documentclass{article}\n\n")

    f.write("\\usepackage{multirow}\n")
    f.write("\\usepackage{float}\n")
    f.write("\\usepackage[export]{adjustbox}\n")

    f.write("\\usepackage{graphicx}\n\n")

    f.write("\\begin{document}\n")


    ouput_table(f)

    f.write("\\end{document}\n")
   # print_table_tex(table);
    

if __name__ == "__main__":
    main()
