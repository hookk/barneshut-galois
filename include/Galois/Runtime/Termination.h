// Dikstra style termination detection -*- C++ -*-
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

#ifndef GALOIS_RUNTIME_TERMINATION_H
#define GALOIS_RUNTIME_TERMINATION_H

#include "Galois/Runtime/PerThreadStorage.h"

namespace GaloisRuntime {

//Dikstra dual-ring termination algorithm
class TerminationDetection {
public:
  class TokenHolder {
    friend class TerminationDetection;
    volatile long tokenIsBlack;
    volatile long hasToken;
    volatile long processIsBlack;
  public:
    TokenHolder() :tokenIsBlack(false), hasToken(false), processIsBlack(true) {}
    inline void workHappened() {
      processIsBlack = true;
    }
  };
private:
  PerThreadStorage<TokenHolder> data;
  volatile bool globalTerm;
  bool lastWasWhite;

public:
  TerminationDetection();

  inline void workHappened() {
    data.getLocal()->workHappened();
  }

  TokenHolder* getLocalTokenHolder() {
    return data.getLocal();
  }

  void initializeThread() {
    if (LL::getTID() == 0) {
      data.getLocal()->hasToken = true;
      data.getLocal()->tokenIsBlack = true;
    }
  }

  void localTermination();

  // Returns
  bool globalTermination() {
    return globalTerm;
  }

  void reset();

};

}
#endif
