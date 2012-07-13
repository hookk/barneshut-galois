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

#include "Galois/Graphs/FileGraph.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#ifdef __linux__
#include <linux/mman.h>
#endif

using namespace Galois::Graph;

//File format V1:
//version (1) {uint64_t LE}
//EdgeType size {uint64_t LE}
//numNodes {uint64_t LE}
//numEdges {uint64_t LE}
//outindexs[numNodes] {uint64_t LE} (outindex[nodeid] is index of first edge for nodeid + 1 (end interator.  node 0 has an implicit start iterator of 0.
//outedges[numEdges] {uint32_t LE}
//potential padding (32bit max) to Re-Align to 64bits
//EdgeType[numEdges] {EdgeType size}

FileGraph::FileGraph()
  : masterMapping(0), masterLength(0), masterFD(0),
    outIdx(0), outs(0), edgeData(0),
    numEdges(0), numNodes(0)
{
}

FileGraph::~FileGraph() {
  if (masterMapping)
    munmap(masterMapping, masterLength);
  if (masterFD)
    close(masterFD);
}

//FIXME: perform le -> host on data here too
void FileGraph::parse(void* m) {
  //parse file
  uint64_t* fptr = (uint64_t*)m;
  __attribute__((unused)) uint64_t version = le64toh(*fptr++);
  assert(version == 1);
  sizeEdgeTy = le64toh(*fptr++);
  numNodes = le64toh(*fptr++);
  numEdges = le64toh(*fptr++);
  outIdx = fptr;
  fptr += numNodes;
  uint32_t* fptr32 = (uint32_t*)fptr;
  outs = fptr32; 
  fptr32 += numEdges;
  if (numEdges % 2)
    fptr32 += 1;
  edgeData = (char*)fptr32;
}

void FileGraph::structureFromMem(void* mem, size_t len, bool clone) {
  masterLength = len;

  if (clone) {
    int _MAP_BASE = MAP_ANONYMOUS | MAP_PRIVATE;
#ifdef MAP_POPULATE
    _MAP_BASE  |= MAP_POPULATE;
#endif
    
    void* m = mmap(0, masterLength, PROT_READ | PROT_WRITE, _MAP_BASE, -1, 0);
    if (m == MAP_FAILED) {
      m = 0;
      perror("FileGraph::structureFromMem");
      abort();
    }
    memcpy(m, mem, len);
    parse(m);
    masterMapping = m;
  } else {
    parse(mem);
    masterMapping = mem;
  }
}

void FileGraph::structureFromFile(const std::string& filename) {
  masterFD = open(filename.c_str(), O_RDONLY);
  if (masterFD == -1) {
    perror("FileGraph::structureFromFile");
    abort();
  }

  struct stat buf;
  int f = fstat(masterFD, &buf);
  if (f == -1) {
    perror("FileGraph::structureFromFile");
    abort();
  }
  masterLength = buf.st_size;

  int _MAP_BASE = MAP_PRIVATE;
#ifdef MAP_POPULATE
  _MAP_BASE  |= MAP_POPULATE;
#endif
  
  void* m = mmap(0, masterLength, PROT_READ, _MAP_BASE, masterFD, 0);
  if (m == MAP_FAILED) {
    m = 0;
    perror("FileGraph::structureFromFile");
    abort();
  }
  parse(m);
  masterMapping = m;
}

//FIXME: perform host -> le on data
void FileGraph::structureToFile(char* file) {
  ssize_t retval;
  //ASSUME LE machine
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd = open(file, O_WRONLY | O_CREAT |O_TRUNC, mode);
  retval = write(fd, masterMapping, masterLength);
  if (retval == -1) {
    perror(__FILE__);
    abort();
  }
  close(fd);
}

void FileGraph::swap(FileGraph& other) {
  std::swap(masterMapping, other.masterMapping);
  std::swap(masterLength, other.masterLength);
  std::swap(sizeEdgeTy, other.sizeEdgeTy);
  std::swap(masterFD, other.masterFD);
  std::swap(outIdx, other.outIdx);
  std::swap(outs, other.outs);
  std::swap(edgeData, other.edgeData);
  std::swap(numEdges, other.numEdges);
  std::swap(numNodes, other.numNodes);
}

void FileGraph::clone(FileGraph& other) {
  structureFromMem(other.masterMapping, other.masterLength, true);
}

uint64_t FileGraph::getEdgeIdx(GraphNode src, GraphNode dst) const {
  for (uint32_t* ii = raw_neighbor_begin(src),
	 *ee = raw_neighbor_end(src); ii != ee; ++ii)
    if (le32toh(*ii) == dst)
      return std::distance(outs, ii);
  return ~static_cast<uint64_t>(0);
}

uint32_t* FileGraph::raw_neighbor_begin(GraphNode N) const {
  return (N == 0) ? &outs[0] : &outs[le64toh(outIdx[N-1])];
}

uint32_t* FileGraph::raw_neighbor_end(GraphNode N) const {
  return &outs[le64toh(outIdx[N])];
}

size_t FileGraph::neighborsSize(GraphNode N, Galois::MethodFlag mflag) const {
  return std::distance(raw_neighbor_begin(N), raw_neighbor_end(N));
}

FileGraph::edge_iterator FileGraph::edge_begin(GraphNode N, MethodFlag mflag) const {
  return edge_iterator(N == 0 ? 0 : le64toh(outIdx[N-1]));
}
FileGraph::edge_iterator FileGraph::edge_end(GraphNode N, MethodFlag mflag) const {
  return edge_iterator(le64toh(outIdx[N]));
}

FileGraph::GraphNode FileGraph::getEdgeDst(edge_iterator it, MethodFlag mflag) const {
  return le32toh(outs[*it]);
}

FileGraph::nodeid_iterator FileGraph::nodeid_begin() const {
  return boost::make_transform_iterator(&outs[0], Convert32());
}

FileGraph::nodeid_iterator FileGraph::nodeid_end() const {
  return boost::make_transform_iterator(&outs[numEdges], Convert32());
}

FileGraph::edgeid_iterator FileGraph::edgeid_begin() const {
  return boost::make_transform_iterator(&outIdx[0], Convert64());
}

FileGraph::edgeid_iterator FileGraph::edgeid_end() const {
  return boost::make_transform_iterator(&outIdx[numNodes], Convert64());
}

bool FileGraph::hasNeighbor(GraphNode N1, GraphNode N2, MethodFlag mflag) const {
  return getEdgeIdx(N1,N2) != ~static_cast<uint64_t>(0);
}

FileGraph::iterator FileGraph::begin() const {
  return iterator(0);
}

FileGraph::iterator FileGraph::end() const {
  return iterator(numNodes);
}

unsigned int FileGraph::size() const {
  return numNodes;
}

unsigned int FileGraph::sizeEdges() const {
  return numEdges;
}

bool FileGraph::containsNode(const GraphNode n) const {
  return n < numNodes;
}

