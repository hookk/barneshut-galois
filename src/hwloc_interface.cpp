/*
Galois, a framework to exploit amorphous data-parallelism in irregular
programs.

Copyright (C) 2011, The University of Texas at Austin. All rights reserved.
UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS SOFTWARE
AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR ANY
PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF PERFORMANCE, AND ANY
WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF DEALING OR USAGE OF TRADE.
NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH RESPECT TO THE USE OF THE
SOFTWARE OR DOCUMENTATION. Under no circumstances shall University be liable
for incidental, special, indirect, direct or consequential damages or loss of
profits, interruption of business, or related expenses which may arise from use
of Software or Documentation, including but not limited to those resulting from
defects in Software and/or Documentation, or loss or inaccuracy of data of any
kind.
*/
#include "Galois/Runtime/Threads.h"
#include "Galois/Runtime/Support.h"

#include <vector>
#include <iostream>

using namespace std;

#if 0

#include "hwloc.h"

class hwloc_policy : public GaloisRuntime::ThreadPolicy {
  hwloc_topology_t topology;

  //thread id -> obj map
  vector<hwloc_obj_t> bindObj;

  int& indexLevelMapL(int level, int thr) {
    return levelMap[level * numThreads + thr];
  }

  void dump() {
    cerr << "numLevels: " << numLevels << "\n"
	 << "numThreads: " << numThreads << "\n"
	 << "numCores: " << numCores << "\n";

    cerr << "LevelSizes: ";
    for (int i = 0; i < numLevels; ++i)
      cerr << " " << levelSize[i];
    cerr << "\n";
    
    cerr << "LevelMap:";
    for (int l = 0; l < numLevels; ++l) {
      cerr << "\n";
      for (int t = 0; t < numThreads; ++t)
	cerr << " " << indexLevelMap(l,t);
    }
    cerr << "\n";

    for (int i = 0; i < numThreads; ++i) {
      char s[128];
      hwloc_obj_snprintf(s, sizeof(s), topology, bindObj[i], "#", 0);
      cerr << "[" << i << "," << s << "] ";
      //printf("%*s%s\n", 2*depth, "", string);
    }
    cerr << "\n";
  }

public:
  hwloc_policy() {
    /* Allocate and initialize topology object. */
    hwloc_topology_init(&topology);
    
    /* ... Optionally, put detection configuration here to ignore
       some objects types, define a synthetic topology, etc....  
       
       The default is to detect all the objects of the machine that
       the caller is allowed to access.  See Configure Topology
       Detection. */
    hwloc_topology_ignore_all_keep_structure(topology);
    
    /* Perform the topology detection. */
    hwloc_topology_load(topology);
    
    /* count the number of threads and cores */
    int numLevelsABS = hwloc_topology_get_depth(topology);
    numThreads = hwloc_get_nbobjs_by_depth(topology, numLevelsABS - 1);
    numLevels = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_CORE);
    numCores = hwloc_get_nbobjs_by_depth(topology, numLevels);
    
    /* allocate maping structure */
    levelMap.resize(numThreads * numLevels);

    /* count how many instances exist at each level */
    for (int depth = 0; depth < numLevels; depth++) {
      int lmax = hwloc_get_nbobjs_by_depth(topology, depth);
      levelSize.push_back(lmax);
    }

    int threadsPerCore = numThreads / numCores;
    //for each non-identiy level, group things
    for (int depth = 0; depth < numLevels; depth++) {
      int lmax = hwloc_get_nbobjs_by_depth(topology, depth);
      int coresPerThing = numCores / lmax;
      for (int i = 0; i < threadsPerCore; ++i)
       	for (int j = 0; j < numCores; ++j)
       	  indexLevelMapL(depth, i * numCores + j) = j / coresPerThing;
      // int threadsPerThing = numThreads / lmax;
      // for (int i = 0; i < numThreads; ++i)
      // 	indexLevelMapL(depth, i ) = i / threadsPerThing;
    }

    // id -> proc map
    bindObj.resize(numThreads);
    for (int i = 0; i < numThreads; ++i) {
      //std::cerr << "[" << i << "," << (i % numCores) * threadsPerCore + (i / numCores) << "] ";
      bindObj[i] = hwloc_get_obj_by_depth(topology, numLevelsABS - 1, (i % numCores) * threadsPerCore + (i / numCores));
      //bindObj[i] = hwloc_get_obj_by_depth(topology, numLevelsABS - 1, i);
    }

    //dump();
  }

  ~hwloc_policy() {
    /* Destroy topology object. */
    hwloc_topology_destroy(topology);
  }

  virtual void bindThreadToProcessor() {
    return;
    int id = GaloisRuntime::getSystemThreadPool().getMyID();
    //cerr << "Binding Thread " << id << "\n";
    if (hwloc_set_cpubind(topology, bindObj[id - 1]->cpuset, 0) < 0) {
      GaloisRuntime::reportWarning("Binding of CPU to thread failed");
    }
  }

};

hwloc_policy hwloc_SystemPolicy;
GaloisRuntime::ThreadPolicy& GaloisRuntime::getSystemThreadPolicy() {
  static hwloc_policy hwloc_SystemPolicy;
  return hwloc_SystemPolicy;
}

#endif
