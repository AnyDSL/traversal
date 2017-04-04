#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#if defined(TRAVERSAL_CPU)
    #include "traversal_cpu.h"
#elif defined(TRAVERSAL_GPU)
    #include"traversal_gpu.h"
#else
    #error "Traversal platform not defined"
#endif

#endif