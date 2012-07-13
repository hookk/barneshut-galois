/** Galois Managed Conflict type wrapper -*- C++ -*-
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
#ifndef GALOIS_CHECKEDOBJECT_H
#define GALOIS_CHECKEDOBJECT_H

#include "Galois/Runtime/Context.h"

namespace Galois {

//! Conflict-checking wapper for any type
/*! A wrapper which performs global conflict detection on the enclosed object.
  This enables arbitrary types to be managed by the Galois runtime. */
template<typename T>
class GWrapped : public GaloisRuntime::Lockable {
  T val;

public:
  GWrapped(const T& v) :val(v) {}

  T& get(Galois::MethodFlag m = ALL) {
    GaloisRuntime::acquire(this, m);
    return val;
  }
};


struct GChecked : public GaloisRuntime::Lockable {
  void acquire(Galois::MethodFlag m = ALL) const {
    // Allow locking of const objects
    GaloisRuntime::acquire(const_cast<GChecked*>(this), m);
  }
};

}

#endif // _GALOIS_CHECKEDOBJECT_H
