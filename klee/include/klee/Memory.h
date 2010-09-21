//===-- Memory.h ------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_MEMORY_H
#define KLEE_MEMORY_H

#include "Context.h"
#include "klee/Expr.h"

#include "llvm/ADT/StringExtras.h"
#include "klee/util/BitArray.h"

#include <vector>
#include <string>

namespace llvm {
  class Value;
}

namespace klee {

class BitArray;
class MemoryManager;
class Solver;

class MemoryObject {
  friend class STPBuilder;

private:
  static int counter;

public:
  unsigned id;
  uint64_t address;

  /// size in bytes
  unsigned size;
  std::string name;

  bool isLocal;
  bool isGlobal;
  bool isFixed;

  /// true if created by us.
  bool fake_object;

  /// User-specified object will not be concretized/restored
  /// when switching to/from concrete execution. That is,
  /// copy(In|Out)Concretes ignores this object.
  bool isUserSpecified;

  /// True if this object should always be accessed directly
  /// by its address (i.e., baypassing all ObjectStates).
  /// This means that the object will always contain concrete
  /// values and its conctent will be shared across all states
  /// (unless explicitly saved/restored on state switches -
  /// ObjectState will still be allocated for this purpose).
  bool isSharedConcrete;

  /// True if the object value can be ignored in local consistency
  bool isValueIgnored;

  /// "Location" for which this memory object was allocated. This
  /// should be either the allocating instruction or the global object
  /// it was allocated for (or whatever else makes sense).
  const llvm::Value *allocSite;
  
  /// A list of boolean expressions the user has requested be true of
  /// a counterexample. Mutable since we play a little fast and loose
  /// with allowing it to be added to during execution (although
  /// should sensibly be only at creation time).
  mutable std::vector< ref<Expr> > cexPreferences;

  // DO NOT IMPLEMENT
  MemoryObject(const MemoryObject &b);
  MemoryObject &operator=(const MemoryObject &b);

public:
  // XXX this is just a temp hack, should be removed
  explicit
  MemoryObject(uint64_t _address) 
    : id(counter++),
      address(_address),
      size(0),
      isFixed(true),
      allocSite(0) {
  }

  MemoryObject(uint64_t _address, unsigned _size, 
               bool _isLocal, bool _isGlobal, bool _isFixed,
               const llvm::Value *_allocSite) 
    : id(counter++),
      address(_address),
      size(_size),
      name("unnamed"),
      isLocal(_isLocal),
      isGlobal(_isGlobal),
      isFixed(_isFixed),
      fake_object(false),
      isUserSpecified(false),
      isSharedConcrete(false),
      allocSite(_allocSite) {
  }

  ~MemoryObject();

  /// Get an identifying string for this allocation.
  void getAllocInfo(std::string &result) const;

  void setName(std::string name) {
    this->name = name;
  }

  ref<ConstantExpr> getBaseExpr() const { 
    return ConstantExpr::create(address, Context::get().getPointerWidth());
  }
  ref<ConstantExpr> getSizeExpr() const { 
    return ConstantExpr::create(size, Context::get().getPointerWidth());
  }
  ref<Expr> getOffsetExpr(ref<Expr> pointer) const {
    return SubExpr::create(pointer, getBaseExpr());
  }
  ref<Expr> getBoundsCheckPointer(ref<Expr> pointer) const {
    return getBoundsCheckOffset(getOffsetExpr(pointer));
  }
  ref<Expr> getBoundsCheckPointer(ref<Expr> pointer, unsigned bytes) const {
    return getBoundsCheckOffset(getOffsetExpr(pointer), bytes);
  }

  ref<Expr> getBoundsCheckOffset(ref<Expr> offset) const {
    if (size==0) {
      return EqExpr::create(offset, 
                            ConstantExpr::alloc(0, Context::get().getPointerWidth()));
    } else {
      return UltExpr::create(offset, getSizeExpr());
    }
  }
  ref<Expr> getBoundsCheckOffset(ref<Expr> offset, unsigned bytes) const {
    if (bytes<=size) {
      return UltExpr::create(offset, 
                             ConstantExpr::alloc(size - bytes + 1, 
                                                 Context::get().getPointerWidth()));
    } else {
      return ConstantExpr::alloc(0, Expr::Bool);
    }
  }
};

class ObjectState {
private:
  // XXX(s2e) for now we keep this first to access from C code
  // (yes, we do need to access if really fast)
  BitArray *concreteMask;

  friend class AddressSpace;
  unsigned copyOnWriteOwner; // exclusively for AddressSpace

  friend class ObjectHolder;
  unsigned refCount;

  const MemoryObject *object;

  //XXX: made it public for fast access
  uint8_t *concreteStore;

  // XXX cleanup name of flushMask (its backwards or something)
  // mutable because may need flushed during read of const
  mutable BitArray *flushMask;

  ref<Expr> *knownSymbolics;

  // mutable because we may need flush during read of const
  mutable UpdateList updates;

public:
  unsigned size;

  bool readOnly;

public:
  /// Create a new object state for the given memory object with concrete
  /// contents. The initial contents are undefined, it is the callers
  /// responsibility to initialize the object contents appropriately.
  ObjectState(const MemoryObject *mo);

  /// Create a new object state for the given memory object with symbolic
  /// contents.
  ObjectState(const MemoryObject *mo, const Array *array);

  ObjectState(const ObjectState &os);
  ~ObjectState();

  inline const MemoryObject *getObject() const { return object; }

  void setReadOnly(bool ro) { readOnly = ro; }

  // make contents all concrete and zero
  void initializeToZero();
  // make contents all concrete and random
  void initializeToRandom();

  ref<Expr> read(ref<Expr> offset, Expr::Width width) const;
  ref<Expr> read(unsigned offset, Expr::Width width) const;
  ref<Expr> read8(unsigned offset) const;

  // fast-path to get concrete values
  bool readConcrete8(unsigned offset, uint8_t* v) const {
    if(object->isSharedConcrete) {
      *v = ((uint8_t*) object->address)[offset]; return true;
    } else if(isByteConcrete(offset)) {
      *v = concreteStore[offset]; return true;
    } else {
      return false;
    }
  }

  // return bytes written.
  void write(unsigned offset, ref<Expr> value);
  void write(ref<Expr> offset, ref<Expr> value);

  void write8(unsigned offset, uint8_t value);
  void write16(unsigned offset, uint16_t value);
  void write32(unsigned offset, uint32_t value);
  void write64(unsigned offset, uint64_t value);

  bool isAllConcrete() const;

  inline bool isConcrete(unsigned offset, Expr::Width width) const {
    if (!concreteMask)
        return true;

    unsigned size = Expr::getMinBytesForWidth(width);
    for(unsigned i = 0; i < size; ++i) {
      if(!isByteConcrete(offset + i))
        return false;
    }
    return true;
  }

  const uint8_t *getConcreteStore(bool allowSymbolic = false) const;
  uint8_t *getConcreteStore(bool allowSymolic = false);

private:
  const UpdateList &getUpdates() const;

  void makeConcrete();

  void makeSymbolic();

  ref<Expr> read8(ref<Expr> offset) const;
  void write8(unsigned offset, ref<Expr> value);
  void write8(ref<Expr> offset, ref<Expr> value);

  void fastRangeCheckOffset(ref<Expr> offset, unsigned *base_r, 
                            unsigned *size_r) const;
  void flushRangeForRead(unsigned rangeBase, unsigned rangeSize) const;
  void flushRangeForWrite(unsigned rangeBase, unsigned rangeSize);

  inline bool isByteConcrete(unsigned offset) const {
    return !concreteMask || concreteMask->get(offset);
  }

  inline bool isByteFlushed(unsigned offset) const {
      return flushMask && !flushMask->get(offset);
  }

  inline bool isByteKnownSymbolic(unsigned offset) const {
      return knownSymbolics && knownSymbolics[offset].get();
  }

  inline void markByteConcrete(unsigned offset) {
      if (concreteMask)
        concreteMask->set(offset);
  }

  void markByteSymbolic(unsigned offset);

  void markByteFlushed(unsigned offset);

  void markByteUnflushed(unsigned offset) {
      if (flushMask)
        flushMask->set(offset);
  }

  void setKnownSymbolic(unsigned offset, Expr *value);

  void print();
};
  
} // End klee namespace

#endif
