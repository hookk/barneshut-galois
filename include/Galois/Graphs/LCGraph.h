/** Local Computation graphs -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2011, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @section Description
 *
 * There are two main classes, ::FileGraph and ::LC_FileGraph. The former
 * represents the pure structure of a graph (i.e., whether an edge exists between
 * two nodes) and cannot be modified. The latter allows values to be stored on
 * nodes and edges, but the structure of the graph cannot be modified.
 *
 * An example of use:
 * 
 * \code
 * typedef Galois::Graph::LC_FileGraph<int,int> Graph;
 * 
 * // Create graph
 * Graph g;
 * g.structureFromFile(inputfile);
 *
 * // Traverse graph
 * for (Graph::iterator i = g.begin(), iend = g.end();
 *      i != iend;
 *      ++i) {
 *   Graph::GraphNode src = *i;
 *   for (Graph::neighbor_iterator j = g.neighbor_begin(src),
 *                                 jend = g.neighbor_end(src);
 *        j != jend;
 *        ++j) {
 *     Graph::GraphNode dst = *j;
 *     int edgeData = g.getEdgeData(src, dst);
 *     int nodeData = g.getData(dst);
 *   }
 * }
 * \endcode
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */

#include "Galois/Graphs/FileGraph.h"
#include "Galois/Runtime/MethodFlags.h"
#include "Galois/Runtime/mm/Mem.h"

#include <iterator>
#include <new>

namespace Galois {
namespace Graph {

//! Small wrapper to have value void specialization
template<typename ETy>
struct EdgeDataWrapper {
  typedef ETy& reference;
  ETy* data;
  uint64_t numEdges;
  
  reference get(ptrdiff_t x) const { return data[x]; }
  EdgeDataWrapper(): data(0) { }
  ~EdgeDataWrapper() {
    if (data)
      GaloisRuntime::MM::largeInterleavedFree(data, sizeof(ETy) * numEdges);
  }
  void readIn(FileGraph& g) {
    numEdges = g.sizeEdges();
    data = reinterpret_cast<ETy*>(GaloisRuntime::MM::largeInterleavedAlloc(sizeof(ETy) * numEdges));
    std::copy(g.edgedata_begin<ETy>(), g.edgedata_end<ETy>(), &data[0]);
  }
};

template<>
struct EdgeDataWrapper<void> {
  typedef bool reference;
  reference get(ptrdiff_t x) const { return false; }
  void readIn(FileGraph& g) { }
};

//! Local computation graph (i.e., graph structure does not change)
template<typename NodeTy, typename EdgeTy>
class LC_CSR_Graph {
protected:
  struct NodeInfo : public GaloisRuntime::Lockable {
    NodeTy data;
  };

  NodeInfo* NodeData;
  uint64_t* EdgeIndData;
  uint32_t* EdgeDst;
  EdgeDataWrapper<EdgeTy> EdgeData;

  uint64_t numNodes;
  uint64_t numEdges;

  uint64_t raw_neighbor_begin(uint32_t N) const {
    return (N == 0) ? 0 : EdgeIndData[N-1];
  }

  uint64_t raw_neighbor_end(uint32_t N) const {
    return EdgeIndData[N];
  }

  uint64_t getEdgeIdx(uint32_t src, uint32_t dst) {
    for (uint64_t ii = raw_neighbor_begin(src),
	   ee = raw_neighbor_end(src); ii != ee; ++ii)
      if (EdgeDst[ii] == dst)
	return ii;
    return ~static_cast<uint64_t>(0);
  }

public:
  typedef uint32_t GraphNode;
  typedef typename EdgeDataWrapper<EdgeTy>::reference edge_data_reference;
  typedef boost::counting_iterator<uint64_t> edge_iterator;
  typedef boost::counting_iterator<uint32_t> iterator;

  LC_CSR_Graph(): NodeData(0), EdgeIndData(0), EdgeDst(0) { }

  ~LC_CSR_Graph() {
    // TODO(ddn): call destructors of user data
    if (EdgeDst)
      GaloisRuntime::MM::largeInterleavedFree(EdgeDst, sizeof(uint32_t) * numEdges);
    if (EdgeIndData)
      GaloisRuntime::MM::largeInterleavedFree(EdgeIndData, sizeof(uint64_t) * numNodes);
    if (NodeData)
      GaloisRuntime::MM::largeInterleavedFree(NodeData, sizeof(NodeInfo) * numNodes);
  }

  NodeTy& getData(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    NodeInfo& NI = NodeData[N];
    GaloisRuntime::acquire(&NI, mflag);
    return NI.data;
  }

  bool hasNeighbor(GraphNode src, GraphNode dst, MethodFlag mflag = ALL) {
    return getEdgeIdx(src, dst) != ~static_cast<uint64_t>(0);
  }

  edge_data_reference getEdgeData(GraphNode src, GraphNode dst, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    GaloisRuntime::acquire(&NodeData[src], mflag);
    return EdgeData.get(getEdgeIdx(src, dst));
  }

  edge_data_reference getEdgeData(edge_iterator ni, MethodFlag mflag = NONE) {
    GaloisRuntime::checkWrite(mflag);
    return EdgeData.get(*ni);
  }

  GraphNode getEdgeDst(edge_iterator ni) {
    return EdgeDst[*ni];
  }

  uint64_t size() const {
    return numNodes;
  }

  uint64_t sizeEdges() const {
    return numEdges;
  }

  iterator begin() const {
    return iterator(0);
  }

  iterator end() const {
    return iterator(numNodes);
  }

  edge_iterator edge_begin(GraphNode N, MethodFlag mflag = ALL) {
    NodeInfo& NI = NodeData[N];
    GaloisRuntime::acquire(&NI, mflag);
    return edge_iterator(raw_neighbor_begin(N));
  }

  edge_iterator edge_end(GraphNode N, MethodFlag mflag = ALL) {
    NodeInfo& NI = NodeData[N];
    GaloisRuntime::acquire(&NI, mflag);
    return edge_iterator(raw_neighbor_end(N));
  }

  void structureFromFile(const std::string& fname) {
    FileGraph graph;
    graph.structureFromFile(fname);
    numNodes = graph.size();
    numEdges = graph.sizeEdges();
    NodeData = reinterpret_cast<NodeInfo*>(GaloisRuntime::MM::largeInterleavedAlloc(sizeof(NodeInfo) * numNodes));
    EdgeIndData = reinterpret_cast<uint64_t*>(GaloisRuntime::MM::largeInterleavedAlloc(sizeof(uint64_t) * numNodes));
    EdgeDst = reinterpret_cast<uint32_t*>(GaloisRuntime::MM::largeInterleavedAlloc(sizeof(uint32_t) * numEdges));
    EdgeData.readIn(graph);
    std::copy(graph.edgeid_begin(), graph.edgeid_end(), &EdgeIndData[0]);
    std::copy(graph.nodeid_begin(), graph.nodeid_end(), &EdgeDst[0]);

    for (unsigned x = 0; x < numNodes; ++x)
      new (&NodeData[x]) NodeTy; // inplace new
  }
};

/**
 * Wrapper class to have a valid type on void edges
 */
template<typename NITy, typename EdgeTy>
struct EdgeInfoWrapper {
  typedef EdgeTy& reference;
  EdgeTy data;
  NITy* dst;

  void allocateEdgeData(FileGraph& g, FileGraph::neighbor_iterator& ni) {
    new (&data) EdgeTy(g.getEdgeData<EdgeTy>(ni));
  }

  reference getData() {
    return data;
  }
};

template<typename NITy>
struct EdgeInfoWrapper<NITy,void> {
  typedef void reference;
  NITy* dst;
  void allocateEdgeData(FileGraph& g, FileGraph::neighbor_iterator& ni) { }
  reference getData() { }
};

//! Local computation graph (i.e., graph structure does not change)
template<typename NodeTy, typename EdgeTy>
class LC_CSRInline_Graph {
protected:
  struct NodeInfo;
  typedef EdgeInfoWrapper<NodeInfo, EdgeTy> EdgeInfo;
  
  struct NodeInfo : public GaloisRuntime::Lockable {
    NodeTy data;
    EdgeInfo* edgebegin;
    EdgeInfo* edgeend;
  };

  NodeInfo* NodeData;
  EdgeInfo* EdgeData;
  uint64_t numNodes;
  uint64_t numEdges;
  NodeInfo* endNode;

  uint64_t getEdgeIdx(uint64_t src, uint64_t dst) {
    NodeInfo& NI = NodeData[src];
    for (uint64_t x = NI.edgebegin; x < NI.edgeend; ++x)
      if (EdgeData[x].dst == dst)
	return x;
    return ~static_cast<uint64_t>(0);
  }

public:
  typedef NodeInfo* GraphNode;
  typedef EdgeInfo* edge_iterator;
  typedef typename EdgeInfo::reference edge_data_reference;

  class iterator : std::iterator<std::random_access_iterator_tag, GraphNode> {
    NodeInfo* at;

  public:
    iterator(NodeInfo* a) :at(a) {}
    iterator(const iterator& m) :at(m.at) {}
    iterator& operator++() { ++at; return *this; }
    iterator operator++(int) { iterator tmp(*this); ++at; return tmp; }
    iterator& operator--() { --at; return *this; }
    iterator operator--(int) { iterator tmp(*this); --at; return tmp; }
    bool operator==(const iterator& rhs) { return at == rhs.at; }
    bool operator!=(const iterator& rhs) { return at != rhs.at; }
    GraphNode operator*() { return at; }
  };

  LC_CSRInline_Graph(): NodeData(0), EdgeData(0) { }

  ~LC_CSRInline_Graph() {
    // TODO(ddn): call destructors of user data
    if (EdgeData)
      GaloisRuntime::MM::largeInterleavedFree(EdgeData, sizeof(*EdgeData) * numEdges);
    if (NodeData)
      GaloisRuntime::MM::largeInterleavedFree(NodeData, sizeof(*NodeData) * numEdges);
  }

  NodeTy& getData(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    GaloisRuntime::acquire(N, mflag);
    return N->data;
  }
  
  edge_data_reference getEdgeData(GraphNode src, GraphNode dst, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    GaloisRuntime::acquire(src, mflag);
    return EdgeData[getEdgeIdx(src,dst)].getData();
  }

  edge_data_reference getEdgeData(edge_iterator ni, MethodFlag mflag = NONE) const {
    GaloisRuntime::checkWrite(mflag);
    return ni->getData();
   }

  GraphNode getEdgeDst(edge_iterator ni) const {
    return ni->dst;
  }

  uint64_t size() const {
    return numNodes;
  }

  uint64_t sizeEdges() const {
    return numEdges;
  }

  iterator begin() const {
    return iterator(&NodeData[0]);
  }

  iterator end() const {
    return iterator(endNode);
  }

  edge_iterator edge_begin(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::acquire(N, mflag);
    return N->edgebegin;
  }

  edge_iterator edge_end(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::acquire(N, mflag);
    return N->edgeend;
  }

  void structureFromFile(const std::string& fname) {
    FileGraph graph;
    graph.structureFromFile(fname);
    numNodes = graph.size();
    numEdges = graph.sizeEdges();
    NodeData = reinterpret_cast<NodeInfo*>(GaloisRuntime::MM::largeInterleavedAlloc(sizeof(*NodeData) * numNodes));
    EdgeData = reinterpret_cast<EdgeInfo*>(GaloisRuntime::MM::largeInterleavedAlloc(sizeof(*EdgeData) * numEdges));
    std::vector<NodeInfo*> node_ids;
    node_ids.resize(numNodes);
    for (FileGraph::iterator ii = graph.begin(),
	   ee = graph.end(); ii != ee; ++ii) {
      NodeInfo* curNode = &NodeData[*ii];
      new (&curNode->data) NodeTy; //inplace new
      node_ids[*ii] = curNode;
    }
    endNode = &NodeData[numNodes];

    //layout the edges
    EdgeInfo* curEdge = &EdgeData[0];
    for (FileGraph::iterator ii = graph.begin(),
	   ee = graph.end(); ii != ee; ++ii) {
      node_ids[*ii]->edgebegin = curEdge;
      for (FileGraph::neighbor_iterator ni = graph.neighbor_begin(*ii),
	     ne = graph.neighbor_end(*ii); ni != ne; ++ni) {
        curEdge->allocateEdgeData(graph, ni);
	curEdge->dst = node_ids[*ni];
	++curEdge;
      }
      node_ids[*ii]->edgeend = curEdge;
    }
  }
};

//! Local computation graph (i.e., graph structure does not change)
template<typename NodeTy, typename EdgeTy>
class LC_Linear_Graph {
protected:
  struct NodeInfo;
  typedef EdgeInfoWrapper<NodeInfo,EdgeTy> EdgeInfo;

  struct NodeInfo : public GaloisRuntime::Lockable {
    NodeTy data;
    int numEdges;

    EdgeInfo* edgeBegin() {
      NodeInfo* n = this;
      ++n; //start of edges
      return reinterpret_cast<EdgeInfo*>(n);
    }

    EdgeInfo* edgeEnd() {
      EdgeInfo* ei = edgeBegin();
      ei += numEdges;
      return ei;
    }

    NodeInfo* next() {
      NodeInfo* ni = this;
      EdgeInfo* ei = edgeEnd();
      while (reinterpret_cast<char*>(ni) < reinterpret_cast<char*>(ei))
	++ni;
      return ni;
    }
  };

  void* Data;
  uint64_t numNodes;
  uint64_t numEdges;
  NodeInfo** nodes;

  EdgeInfo* getEdgeIdx(NodeInfo* src, NodeInfo* dst) {
    EdgeInfo* eb = src->edgeBegin();
    EdgeInfo* ee = src->edgeEnd();
    for (; eb != ee; ++eb)
      if (eb->dst == dst)
	return eb;
    return 0;
  }

public:
  typedef NodeInfo* GraphNode;
  typedef EdgeInfo* edge_iterator;
  typedef typename EdgeInfo::reference edge_data_reference;
  typedef NodeInfo** iterator;

  LC_Linear_Graph(): Data(0), nodes(0) { }

  ~LC_Linear_Graph() { 
    // TODO(ddn): call destructors of user data
    if (nodes)
      GaloisRuntime::MM::largeInterleavedFree(nodes, sizeof(NodeInfo*) * numNodes);
    if (Data)
      GaloisRuntime::MM::largeInterleavedFree(Data, sizeof(NodeInfo) * numNodes * 2 + sizeof(EdgeInfo) * numEdges);
  }

  NodeTy& getData(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    GaloisRuntime::acquire(N, mflag);
    return N->data;
  }
  
  edge_data_reference getEdgeData(GraphNode src, GraphNode dst, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    GaloisRuntime::acquire(src, mflag);
    return getEdgeIdx(src,dst)->getData();
  }

  edge_data_reference getEdgeData(edge_iterator ni, MethodFlag mflag = NONE) const {
    GaloisRuntime::checkWrite(mflag);
    return ni->getData();
  }

  GraphNode getEdgeDst(edge_iterator ni) const {
    return ni->dst;
  }

  uint64_t size() const {
    return numNodes;
  }

  uint64_t sizeEdges() const {
    return numEdges;
  }

  iterator begin() const {
    return nodes;
  }

  iterator end() const {
    return &nodes[numNodes];
  }

  edge_iterator edge_begin(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::acquire(N, mflag);
    return N->edgeBegin();
  }

  edge_iterator edge_end(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::acquire(N, mflag);
    return N->edgeEnd();
  }

  void structureFromFile(const std::string& fname) {
    FileGraph graph;
    graph.structureFromFile(fname);
    numNodes = graph.size();
    numEdges = graph.sizeEdges();
    Data = GaloisRuntime::MM::largeInterleavedAlloc(sizeof(NodeInfo) * numNodes * 2 +
					 sizeof(EdgeInfo) * numEdges);
    nodes = reinterpret_cast<NodeInfo**>(GaloisRuntime::MM::largeInterleavedAlloc(sizeof(NodeInfo*) * numNodes));
    NodeInfo* curNode = reinterpret_cast<NodeInfo*>(Data);
    for (FileGraph::iterator ii = graph.begin(),
	   ee = graph.end(); ii != ee; ++ii) {
	new (&curNode->data) NodeTy; //inplace new
      curNode->numEdges = graph.neighborsSize(*ii);
      nodes[*ii] = curNode;
      curNode = curNode->next();
    }

    //layout the edges
    for (FileGraph::iterator ii = graph.begin(), ee = graph.end(); ii != ee; ++ii) {
      EdgeInfo* edge = nodes[*ii]->edgeBegin();
      for (FileGraph::neighbor_iterator ni = graph.neighbor_begin(*ii),
	     ne = graph.neighbor_end(*ii); ni != ne; ++ni) {
        edge->allocateEdgeData(graph, ni);
	edge->dst = nodes[*ni];
	++edge;
      }
    }
  }
};

//! Local computation graph (i.e., graph structure does not change)
template<typename NodeTy, typename EdgeTy>
class LC_Linear2_Graph {
protected:
  struct NodeInfo;
  typedef EdgeInfoWrapper<NodeInfo,EdgeTy> EdgeInfo;

  struct NodeInfo : public GaloisRuntime::Lockable {
    NodeTy data;
    int numEdges;

    EdgeInfo* edgeBegin() {
      NodeInfo* n = this;
      ++n; //start of edges
      return reinterpret_cast<EdgeInfo*>(n);
    }

    EdgeInfo* edgeEnd() {
      EdgeInfo* ei = edgeBegin();
      ei += numEdges;
      return ei;
    }

    NodeInfo* next() {
      NodeInfo* ni = this;
      EdgeInfo* ei = edgeEnd();
      while (reinterpret_cast<char*>(ni) < reinterpret_cast<char*>(ei))
	++ni;
      return ni;
    }
  };

  struct Header {
    NodeInfo* begin;
    NodeInfo* end;
    size_t size;
  };

  GaloisRuntime::PerCPU<Header*> headers;
  NodeInfo** nodes;
  uint64_t numNodes;
  uint64_t numEdges;

  EdgeInfo* getEdgeIdx(NodeInfo* src, NodeInfo* dst) {
    EdgeInfo* eb = src->edgeBegin();
    EdgeInfo* ee = src->edgeEnd();
    for (; eb != ee; ++eb)
      if (eb->dst == dst)
	return eb;
    return 0;
  }

  struct DistributeInfo {
    uint64_t numNodes;
    uint64_t numEdges;
    FileGraph::iterator begin;
    FileGraph::iterator end;
  };

  //! Divide graph into equal sized chunks
  void distribute(FileGraph& graph, GaloisRuntime::PerCPU<DistributeInfo>& dinfo) {
    size_t total = sizeof(NodeInfo) * numNodes + sizeof(EdgeInfo) * numEdges;
    unsigned int num = Galois::getActiveThreads();
    size_t blockSize = total / num;
    size_t curSize = 0;
    FileGraph::iterator ii = graph.begin();
    FileGraph::iterator ei = graph.end();
    FileGraph::iterator last = ii;
    uint64_t nnodes = 0;
    uint64_t nedges = 0;
    uint64_t runningNodes = 0;
    uint64_t runningEdges = 0;

    unsigned int tid;
    for (tid = 0; tid + 1 < num; ++tid) {
      for (; ii != ei; ++ii) {
        if (curSize >= (tid + 1) * blockSize) {
          DistributeInfo& d = dinfo.get(tid);
          d.numNodes = nnodes;
          d.numEdges = nedges;
          d.begin = last;
          d.end = ii;

          runningNodes += nnodes;
          runningEdges += nedges;
          nnodes = nedges = 0;
          last = ii;
          break;
        }
        size_t nneighbors = graph.neighborsSize(*ii);
        nedges += nneighbors;
        nnodes += 1;
        curSize += sizeof(NodeInfo) + sizeof(EdgeInfo) * nneighbors;
      }
    }

    DistributeInfo& d = dinfo.get(tid);
    d.numNodes = numNodes - runningNodes;
    d.numEdges = numEdges - runningEdges;
    d.begin = last;
    d.end = ei;
  }

  struct AllocateNodes {
    GaloisRuntime::PerCPU<DistributeInfo>& dinfo;
    GaloisRuntime::PerCPU<Header*>& headers;
    NodeInfo** nodes;
    FileGraph& graph;

    AllocateNodes(
        GaloisRuntime::PerCPU<DistributeInfo>& d,
        GaloisRuntime::PerCPU<Header*>& h, NodeInfo** n, FileGraph& g):
      dinfo(d), headers(h), nodes(n), graph(g) { }

    void operator()(unsigned int tid, unsigned int num) {
      DistributeInfo& d = dinfo.get(tid);

      // extra 2 factors are for alignment purposes
      size_t size =
          sizeof(Header) * 2 +
          sizeof(NodeInfo) * d.numNodes * 2 +
          sizeof(EdgeInfo) * d.numEdges;

      void *raw = GaloisRuntime::MM::largeAlloc(size);
      memset(raw, 0, size);

      Header*& h = headers.get();
      h = reinterpret_cast<Header*>(raw);
      h->size = size;
      h->begin = h->end = reinterpret_cast<NodeInfo*>(h + 1);

      if (!d.numNodes)
        return;

      for (FileGraph::iterator ii = d.begin, ee = d.end; ii != ee; ++ii) {
        new (&h->end->data) NodeTy; //inplace new
        h->end->numEdges = graph.neighborsSize(*ii);
        nodes[*ii] = h->end;
        h->end = h->end->next();
      }
    }
  };

  struct AllocateEdges {
    GaloisRuntime::PerCPU<DistributeInfo>& dinfo;
    NodeInfo** nodes;
    FileGraph& graph;

    AllocateEdges(GaloisRuntime::PerCPU<DistributeInfo>& d, NodeInfo** n, FileGraph& g):
      dinfo(d), nodes(n), graph(g) { }

    //! layout the edges
    void operator()(unsigned int tid, unsigned int num) {
      DistributeInfo& d = dinfo.get(tid);
      if (!d.numNodes)
        return;

      for (FileGraph::iterator ii = d.begin, ee = d.end; ii != ee; ++ii) {
        EdgeInfo* edge = nodes[*ii]->edgeBegin();
        for (FileGraph::neighbor_iterator ni = graph.neighbor_begin(*ii),
               ne = graph.neighbor_end(*ii); ni != ne; ++ni) {
          edge->allocateEdgeData(graph, ni);
          edge->dst = nodes[*ni];
          ++edge;
        }
      }
    }
  };

public:
  typedef NodeInfo* GraphNode;
  typedef EdgeInfo* edge_iterator;
  typedef typename EdgeInfo::reference edge_data_reference;
  typedef NodeInfo** iterator;

  class local_iterator : public std::iterator<std::forward_iterator_tag, GraphNode> {
    const GaloisRuntime::PerCPU<Header*>* headers;
    unsigned int tid;
    Header* p;
    GraphNode v;

    bool init_thread() {
      p = tid < headers->size() ? headers->get(tid) : 0;
      v = p ? p->begin : 0;
      return p;
    }

    bool advance_local() {
      if (p) {
	v = v->next();
	return v != p->end;
      }
      return false;
    }

    void advance_thread() {
      while (tid < headers->size()) {
	++tid;
	if (init_thread())
	  return;
      }
    }

    void advance() {
      if (advance_local()) return;
      advance_thread();
    }

  public:
    local_iterator(): headers(0), tid(0), p(0), v(0) { }
    local_iterator(const GaloisRuntime::PerCPU<Header*>* _headers, int _tid):
      headers(_headers), tid(_tid), p(0), v(0)
    {
      //find first valid item
      if (!init_thread())
	advance_thread();
    }

    local_iterator(const iterator& it): headers(it.headers), tid(it.tid), p(it.p), v(it.v) { }
    local_iterator& operator++() { advance(); return *this; }
    local_iterator operator++(int) { local_iterator tmp(*this); operator++(); return tmp; }
    bool operator==(const local_iterator& rhs) const {
      return (headers == rhs.headers && tid == rhs.tid && p == rhs.p && v == rhs.v);
    }
    bool operator!=(const local_iterator& rhs) const {
      return !(headers == rhs.headers && tid == rhs.tid && p == rhs.p && v == rhs.v);
    }
    GraphNode operator*() const { return v; }
  };

  LC_Linear2_Graph(): nodes(0) { }

  ~LC_Linear2_Graph() {
    // TODO(ddn): call destructors of user data
    if (nodes)
      GaloisRuntime::MM::largeInterleavedFree(nodes, sizeof(NodeInfo*) * numNodes);
    for (unsigned i = 0; i < headers.size(); ++i) {
      Header* h = headers.get(i);
      if (h)
        GaloisRuntime::MM::largeFree(h, h->size);
    }
  }

  NodeTy& getData(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    GaloisRuntime::acquire(N, mflag);
    return N->data;
  }
  
  edge_data_reference getEdgeData(GraphNode src, GraphNode dst, MethodFlag mflag = ALL) {
    GaloisRuntime::checkWrite(mflag);
    GaloisRuntime::acquire(src, mflag);
    return getEdgeIdx(src,dst)->getData();
  }

  edge_data_reference getEdgeData(edge_iterator ni, MethodFlag mflag = NONE) const {
    GaloisRuntime::checkWrite(mflag);
    return ni->getData();
  }

  GraphNode getEdgeDst(edge_iterator ni) const {
    return ni->dst;
  }

  uint64_t size() const {
    return numNodes;
  }

  uint64_t sizeEdges() const {
    return numEdges;
  }

  iterator begin() const {
    return nodes;
  }

  iterator end() const {
    return &nodes[numNodes];
  }

  local_iterator local_begin() const {
    return local_iterator(&headers, GaloisRuntime::LL::getTID());
  }

  local_iterator local_end() const {
    return local_iterator(&headers, GaloisRuntime::LL::getTID() + 1);
  }

  edge_iterator edge_begin(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::acquire(N, mflag);
    return N->edgeBegin();
  }

  edge_iterator edge_end(GraphNode N, MethodFlag mflag = ALL) {
    GaloisRuntime::acquire(N, mflag);
    return N->edgeEnd();
  }

  void structureFromFile(const std::string& fname) {
    FileGraph graph;
    graph.structureFromFile(fname);
    numNodes = graph.size();
    numEdges = graph.sizeEdges();

    GaloisRuntime::PerCPU<DistributeInfo> dinfo;
    distribute(graph, dinfo);

    size_t size = sizeof(NodeInfo*) * numNodes;
    nodes = reinterpret_cast<NodeInfo**>(GaloisRuntime::MM::largeInterleavedAlloc(size));

    Galois::on_each(AllocateNodes(dinfo, headers, nodes, graph));
    Galois::on_each(AllocateEdges(dinfo, nodes, graph));
  }
};


}
}
