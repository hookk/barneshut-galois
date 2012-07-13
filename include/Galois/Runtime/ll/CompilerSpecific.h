/** Galois configuration -*- C++ -*-
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
 * Factor out compiler-specific lowlevel stuff
 *
 * @author Donald Nguyen <ddn@cs.utexas.edu>
 */
#ifndef GALOIS_RUNTIME_LL_COMPILERSPECIFIC_H
#define GALOIS_RUNTIME_LL_COMPILERSPECIFIC_H

namespace GaloisRuntime {
namespace LL {

inline static void asmPause() {
#if defined(__i386__) || defined(__amd64__)
  //__builtin_ia32_pause();
  asm volatile ( "pause");
#endif
}

inline static void compilerBarrier() {
  asm volatile ("":::"memory");
}

#if defined(__INTEL_COMPILER)
#define GALOIS_ATTRIBUTE_NOINLINE __attribute__ ((noinline))
#define GALOIS_ATTRIBUTE_DEPRECATED __attribute__ ((deprecated))
#elif defined( __GNUC__)
#define GALOIS_ATTRIBUTE_NOINLINE __attribute__ ((noinline))
#define GALOIS_ATTRIBUTE_DEPRECATED __attribute__ ((deprecated))
#elif defined( _MSC_VER)
#define GALOIS_ATTRIBUTE_NOINLINE __declspec(noinline)
#define GALOIS_ATTRIBUTE_DEPRECATED __declspec ((deprecated))
#else
#define GALOIS_ATTRIBUTE_NOINLINE
#endif

}
}

#endif
