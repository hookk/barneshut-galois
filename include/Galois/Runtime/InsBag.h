/** Insert Bag v2 -*- C++ -*-
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
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#ifndef GALOIS_RUNTIME_INSBAG_H
#define GALOIS_RUNTIME_INSBAG_H

#include "Galois/Runtime/mm/Mem.h" 
#include "Galois/Runtime/ll/PtrLock.h"
#include <iterator>

namespace GaloisRuntime {

template<class T>
class galois_insert_bag : private boost::noncopyable {
 
  struct header {
    header* next;
    T* dbegin; //start of interesting data
    T* dend; //end of valid data
    T* dlast; //end of storage
  };

  GaloisRuntime::PerCPU<header*> heads;

  void insHeader(header* h) {
    header*& H = heads.get();
    h->next = H;
    H = h;
  }

  header* newHeader() {
    void* m = MM::pageAlloc();
    header* H = new (m) header();
    int offset = 1;
    if (sizeof(T) < sizeof(header))
      offset += sizeof(header)/sizeof(T);
    T* a = reinterpret_cast<T*>(m);
    H->dbegin = &a[offset];
    H->dend = H->dbegin;
    H->dlast = &a[(MM::pageSize / sizeof(T))];
    H->next = 0;
    return H;
  }

  void destruct() {
    for (unsigned x = 0; x < heads.size(); ++x) {
      header* h = heads.get(x);
      if (h) {
	heads.get(x) = h->next;
	for (T* ii = h->dbegin, *ee = h->dend; ii != ee; ++ii) {
	  ii->~T();
	}
	MM::pageFree(h);
      }
    }
  }

public:
  galois_insert_bag() {}

  ~galois_insert_bag() {
    destruct();
  }

  void clear() {
    destruct();
    for (unsigned i = 0; i < heads.size(); ++i)
      heads.get(i) = 0;
  }

  typedef T        value_type;
  typedef const T& const_reference;
  typedef T&       reference;

  class iterator : public std::iterator<std::forward_iterator_tag, T> {
    GaloisRuntime::PerCPU<header*>* hd;
    unsigned int thr;
    header* p;
    T* v;

    bool init_thread() {
      p = thr < hd->size() ? hd->get(thr) : 0;
      v = p ? p->dbegin : 0;
      return p;
    }

    bool advance_local() {
      if (p) {
	++v;
	return v != p->dend;
      }
      return false;
    }

    bool advance_chunk() {
      if (p) {
	p = p->next;
	v = p ? p->dbegin : 0;
      }
      return p;
    }

    void advance_thread() {
      while (thr < hd->size()) {
	++thr;
	if (init_thread())
	  return;
      }
    }

    void advance() {
      if (advance_local()) return;
      if (advance_chunk()) return;
      advance_thread();
    }

  public:
    iterator() :hd(0), thr(0), p(0), v(0) {}
    iterator(GaloisRuntime::PerCPU<header*>* _hd, int _thr) 
      :hd(_hd), thr(_thr), p(0), v(0)
    {
      //find first valid item
      if (!init_thread())
	advance_thread();
    }

    iterator(const iterator& mit) :hd(mit.hd), thr(mit.thr), p(mit.p), v(mit.v) {}

    iterator& operator++() {
      advance();
      return *this;
    }
    iterator operator++(int) {iterator tmp(*this); operator++(); return tmp;}
    bool operator==(const iterator& rhs) const {
      return (hd == rhs.hd && thr == rhs.thr && p==rhs.p && v == rhs.v);
    }
    bool operator!=(const iterator& rhs) const {
      return !(hd == rhs.hd && thr == rhs.thr && p==rhs.p && v == rhs.v);
    }
    T& operator*() const {return *v;}
  };
  
  iterator begin() {
    return iterator(&heads, 0);
  }
  
  iterator end() {
    return iterator(&heads, heads.size());
  }
  
  typedef iterator local_iterator;

  local_iterator local_begin() {
    return iterator(&heads, LL::getTID());
  }

  local_iterator local_end() {
    return iterator(&heads, LL::getTID() + 1);
  }

  //Only this is thread safe
  reference push(const T& val) {
    header* H = heads.get();
    T* rv;
    if (!H || H->dend == H->dlast) {
      H = newHeader();
      insHeader(H);
    }
    rv = new (H->dend) T(val);
    H->dend++;
    return *rv;
  }

  //allow using std::back_inserter
  reference push_back(const T& val) {
    return push(val);
  }
};


}
#endif
