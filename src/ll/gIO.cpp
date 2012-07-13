/** galois IO rutines -*- C++ -*-
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
 * IO support for galois.  We use this to handle output redirection,
 * and common formating issues.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */

#include "Galois/Runtime/ll/gio.h"
#include "Galois/Runtime/ll/SimpleLock.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

static GaloisRuntime::LL::SimpleLock<true> IOLock;

void GaloisRuntime::LL::gPrint(const char* format, ...) {
  IOLock.lock();
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  va_end(ap);
  IOLock.unlock();
}

void GaloisRuntime::LL::gInfo( const char* format, ...) {
  IOLock.lock();
  va_list ap;
  va_start(ap, format);
  printf("INFO: ");
  vprintf(format, ap);
  printf("\n");
  va_end(ap);
  IOLock.unlock();
}

void GaloisRuntime::LL::gWarn( const char* format, ...) {
  IOLock.lock();
  va_list ap;
  va_start(ap, format);
  printf("WARNING: ");
  vprintf(format, ap);
  printf("\n");
  va_end(ap);
  IOLock.unlock();
}

void GaloisRuntime::LL::gError(bool doabort, const char* format, ...) {
  IOLock.lock();
  va_list ap;
  va_start(ap, format);
  printf("ERROR: ");
  vprintf(format, ap);
  printf("\n");
  va_end(ap);
  IOLock.unlock();
  if (doabort)
    abort();
}

void GaloisRuntime::LL::gFlush() {
  fflush(stdout);
}
