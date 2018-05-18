#include "aotjs_runtime.h"

#include <cmath>

#ifdef DEBUG
#include <iostream>
#endif

#include <sstream>

namespace AotJS {
  TypeOf typeOfGCThing = "gcthing"; // base class should not be exposed
  TypeOf typeOfInternal = "internal"; // internal class should not be exposed
  TypeOf typeOfJSThing = "jsthing"; // base class should not be exposed

  TypeOf typeOfBoxDouble = "boxdouble";
  TypeOf typeOfBoxInt32 = "boxint32";
  TypeOf typeOfDeleted = "deleted";

  TypeOf typeOfUndefined = "undefined";
  TypeOf typeOfNull = "null";
  TypeOf typeOfNumber = "number";
  TypeOf typeOfBoolean = "boolean";
  TypeOf typeOfString = "string";
  TypeOf typeOfSymbol = "symbol";
  TypeOf typeOfFunction = "function";
  TypeOf typeOfObject = "object";
}

#pragma mark Val hash helpers

namespace std {
  size_t hash<::AotJS::Val>::operator()(::AotJS::Val const& ref) const noexcept {
    if (ref.isString()) {
      return hash<string>{}(ref.asString());
    } else {
      return hash<uint64_t>{}(ref.raw());
    }
  }
}

namespace AotJS {


  bool Val::operator==(const Val &rhs) const {
    // Bit-identical always matches!
    // This catches matching pointers (same object), matching int31s, etc.
    if (raw() == rhs.raw()) {
      return true;
    }

    if (isString() && rhs.isString()) {
      // Two string instances may still compare equal.
      return asString() == rhs.asString();
    }

    // fixme handle box types better
    if (isInt32() && rhs.isInt32()) {
      return asInt32() == rhs.asInt32();
    }
    if (isDouble() && rhs.isDouble()) {
      return asDouble() == rhs.asDouble();
    }

    // Non-identical non-string objects never compare identical.
    return false;
  }

  RetVal Val::operator+(const Val& rhs) const {
    ScopeRetVal scope;
    // todo implement this correctly
    if (isString() || rhs.isString()) {
      return scope.escape(*toString() + *rhs.toString());
    } else {
      return scope.escape(toDouble() + rhs.toDouble());
    }
  }

  // Checked conversions
  bool Val::toBool() const
  {
    if (isBool()) {
      return asBool();
    } else if (isInt32()) {
      return static_cast<bool>(asInt32());
    } else if (isDouble()) {
      return static_cast<bool>(asDouble());
    } else if (isUndefined()) {
      return false;
    } else if (isNull()) {
      return false;
    } else if (isString()) {
      return asString().length() > 0;
    } else {
      return true;
    }
  }

  int32_t Val::toInt32() const
  {
    if (isInt32()) {
      return asInt32();
    } else if (isBool()) {
      return static_cast<int32_t>(asBool());
    } else if (isDouble()) {
      return static_cast<int32_t>(asDouble());
    } else if (isUndefined()) {
      return 0;
    } else if (isNull()) {
      return 0;
    } else {
      // todo: handle objects
      std::abort();
    }
  }

  double Val::toDouble() const
  {
    if (isDouble()) {
      return asDouble();
    } else if (isBool()) {
      return static_cast<double>(asBool());
    } else if (isInt32()) {
      return static_cast<double>(asInt32());
    } else if (isUndefined()) {
      return NAN;
    } else if (isNull()) {
      return 0;
    } else {
      // todo: handle objects...
      std::abort();
    }
  }

  Ret<String> Val::toString() const
  {
    ScopeRet<String> scope;
    if (isJSThing()) {
      return scope.escape(asJSThing().toString());
    } else {
      // todo: flip this?
      return scope.escape(new String(dump()));
    }
  }

  string Val::dump() const {
    std::ostringstream buf;
    if (isDouble()) {
      buf << asDouble();
    } else if (isInt32()) {
      buf << asInt32();
    } else if (isBool()) {
      if (asBool()) {
        buf << "true";
      } else {
        buf << "false";
      }
    } else if (isNull()) {
      buf << "null";
    } else if (isUndefined()) {
      buf << "undefined";
    } else if (isGCThing()) {
      buf << asGCThing().dump();
    }
    return buf.str();
  }

  RetVal Val::call(Local aThis, RawArgList aArgs) const
  {
    ScopeRetVal scope;
    if (isFunction()) {
      return scope.escape(*asFunction().call(aThis, aArgs));
    } else {
      #ifdef DEBUG
      std::cerr << "not a function\n";
      #endif
      std::abort();
    }
  }

  #pragma mark ArgList

  ArgList::ArgList(Function& func, RawArgList args)
  : mStackTop(engine().stackTop()),
    mBegin(nullptr),
    mSize(0)
  {
    // Copy all the temporaries passed in to a lower scope
    size_t size = 0;
    for (auto& local : args) {
      Val* binding = engine().pushLocal(*local);
      if (size++ == 0) {
        mBegin = binding;
      }
    }

    // Save the original argument list size for `arguments.length`
    mSize = size;

    // Assign space for undefined for anything left.
    // This means functions can access vars without a bounds check.
    // todo: apply es6 default params
    while (size < func.arity()) {
      Val* binding = engine().pushLocal(Undefined());
      if (size++ == 0) {
        mBegin = binding;
      }
    }
  }

  #pragma mark GCThing


  GCThing::~GCThing() {
    //
  }

  void GCThing::markRefsForGC() {
    // no-op default
  }

  string GCThing::dump() {
    return "GCThing";
  }

  TypeOf GCThing::typeOf() const {
    return typeOfGCThing;
  }

  #pragma mark Box

  template <>
  TypeOf Box<double>::typeOf() const {
    return typeOfBoxDouble;
  }

  template <>
  TypeOf Box<int32_t>::typeOf() const {
    return typeOfBoxInt32;
  }

  template <>
  TypeOf Box<Undefined>::typeOf() const {
    return typeOfUndefined;
  }

  template <>
  TypeOf Box<Null>::typeOf() const {
    return typeOfNull;
  }

  template <>
  TypeOf Box<bool>::typeOf() const {
    return typeOfBoolean;
  }

  template <>
  TypeOf Box<Deleted>::typeOf() const {
    return typeOfDeleted;
  }

  #pragma mark Object

  Object::~Object() {
    //
  }

  TypeOf Object::typeOf() const {
    return typeOfObject;
  }

  static RetVal normalizePropName(Val aName) {
    ScopeRetVal scope;
    if (aName.isString()) {
      return scope.escape(aName);
    } else if (aName.isSymbol()) {
      return scope.escape(aName);
    } else {
      // todo: convert to string
      #ifdef DEBUG
      std::cerr << "cannot use non-string/symbol as property index";
      #endif
      std::abort();
      return scope.escape(Undefined());
    }
  }

  // todo: handle numeric indices
  // todo: getters
  RetVal Object::getProp(Val aName) {
    ScopeRetVal scope;
    Local name = *normalizePropName(aName);
    auto index = mProps.find(*name);
    if (index == mProps.end()) {
      if (mPrototype) {
        return scope.escape(*mPrototype->getProp(*name));
      } else {
        return scope.escape(Undefined());
      }
    } else {
      return scope.escape(index->first);
    }
  }

  void Object::setProp(Val aName, Val aVal) {
    Local name = *normalizePropName(aName);
    mProps.emplace(*name, aVal);
  }

  void Object::markRefsForGC() {
    for (auto iter : mProps) {
      auto prop_name(iter.first);
      auto prop_val(iter.second);

      prop_name.markForGC();
      prop_val.markForGC();
    }
  }

  string Object::dump() {
    std::ostringstream buf;
    buf << "Object({";

    bool first = true;
    for (auto iter : mProps) {
      auto name = iter.first;
      auto val = iter.second;

      if (first) {
        first = false;
      } else {
        buf << ",";
      }
      buf << name.dump();
      buf << ":";
      buf << val.dump();
    }
    buf << "})";
    return buf.str();
  }

  #pragma mark PropIndex
  PropIndex::~PropIndex() {
    //
  }

  #pragma mark String

  String::~String() {
    //
  }

  TypeOf String::typeOf() const {
    return typeOfString;
  }

  string String::dump() {
    // todo: emit JSON or something
    std::ostringstream buf;
    buf << "\"";
    buf << data;
    buf << "\"";
    return buf.str();
  }

  #pragma mark Symbol

  Symbol::~Symbol() {
    //
  }

  TypeOf Symbol::typeOf() const {
    return typeOfSymbol;
  }

  string Symbol::dump() {
    std::ostringstream buf;
    buf << "Symbol(\"";
    buf << name;
    buf << "\")";
    return buf.str();
  }

  #pragma mark Internal

  Internal::~Internal() {
    //
  }

  TypeOf Internal::typeOf() const {
    return typeOfInternal;
  }

  #pragma mark JSThing

  JSThing::~JSThing() {
    //
  }

  TypeOf JSThing::typeOf() const {
    return typeOfJSThing;
  }

  Ret<String> JSThing::toString() const {
    ScopeRet<String> scope;
    return scope.escape(new String("[jsthing JSThing]"));
  }

  #pragma mark Cell

  Cell::~Cell() {
    //
  }

  void Cell::markRefsForGC() {
    mVal.markForGC();
  }

  string Cell::dump() {
    std::ostringstream buf;
    buf << "Cell(" << mVal.dump() << ")";
    return buf.str();
  }

  #pragma mark Function

  Function::~Function() {
    //
  }

  TypeOf Function::typeOf() const {
    return typeOfFunction;
  }

  void Function::markRefsForGC() {
    for (auto cell : mCaptures) {
      cell->val().markForGC();
    }
  }


  RetVal Function::call(Local aThis, RawArgList aArgs) {
    ScopeRetVal scope;
    return scope.escape(*mBody(*this, aThis, ArgList(*this, aArgs)));
  }

  string Function::dump() {
    std::ostringstream buf;
    buf << "Function(\"" << name() << "\")";
    return buf.str();
  }

  #pragma mark Engine

  // warning: if stack goes beyond this it will explode
  Engine::Engine(size_t aStackSize)
  : mUndefined(nullptr),
    mNull(nullptr),
    mDeleted(nullptr),
    mFalse(nullptr),
    mTrue(nullptr),
    mRoot(nullptr),
    mStackBegin(new Val[aStackSize]),
    mStackTop(mStackBegin),
    mStackEnd(mStackBegin + aStackSize),
    mObjects()
  {
    // We must clear those pointers first or else GC might explode
    // while allocating the root & sigil objects!
    mUndefined = new Box<Undefined>(Undefined());
    mNull = new Box<Null>(Null());
    mDeleted = new Box<Deleted>(Deleted());
    mFalse = new Box<bool>(false);
    mTrue = new Box<bool>(true);

    // The global object.
    mRoot = new Object();
  }

  Engine::~Engine() {
    delete[] mStackBegin;
  }

  void Engine::registerForGC(GCThing& obj) {
    // Because we don't yet control the allocation, we don't know how to walk
    // the heap looking for all objects. Instead, we need to keep a separate
    // set of all objects in order to do the final sweep.
    mObjects.insert(&obj);
  }

  Val* Engine::stackTop()
  {
    return mStackTop;
  }

  Val* Engine::pushLocal(Val val)
  {
    if (mStackTop++ > mStackEnd) {
        #ifdef DEBUG
        std::cerr << "stack overflow!\n";
        #endif
        std::abort();
    }
    *mStackTop = val;
    return stackTop();
  }

  void Engine::popLocal(Val* expectedTop) {
    #ifdef DEBUG
    if (expectedTop < mStackBegin || expectedTop > mStackTop) {
      std::cerr << "bad stack pointer: "
        << stackTop()
        << ", expected "
        << expectedTop
        << "\n";
      std::abort();
    }
    #endif
    mStackTop = expectedTop;
  }

  void Engine::gc() {
    #ifdef DEBUG
    std::cerr << "starting gc mark/sweep: " << dump() <<"\n";
    #endif

    // 1) Mark!
    // Mark anything reachable from the global root object.
    if (mRoot) {
      mRoot->markForGC();
    }

    // Mark our global sigil objects.
    if (mUndefined) {
      mUndefined->markForGC();
    }
    if (mNull) {
      mNull->markForGC();
    }
    if (mDeleted) {
      mDeleted->markForGC();
    }
    if (mFalse) {
      mFalse->markForGC();
    }
    if (mTrue) {
      mTrue->markForGC();
    }

    // Mark anything on the stack of currently open scopes
    for (Val* record = mStackBegin; record < mStackTop; record++) {
      record->markForGC();
    }

    // 2) Sweep!
    // We alloc a vector here ebecause we can't change the set while iterating.
    // Todo: don't require allocating memory to free memory!
    std::vector<GCThing *> deadObjects;
    for (auto obj : mObjects) {
      if (obj->isMarkedForGC()) {
        // Keep the object, but reset the marker value for next time.
        obj->clearForGC();
      } else {
        #ifdef DEBUG
        std::cerr << "removing dead object: " << obj->dump() << "\n";
        #endif
        deadObjects.push_back(obj);
      }
    }

    for (auto obj : deadObjects) {
      // No findable references to this object. Destroy it!
      mObjects.erase(obj);
      delete obj;
    }
  }

  string Engine::dump() {
    std::ostringstream buf;
    buf << "Engine([";

    bool first = true;
    for (auto obj : mObjects) {
      if (first) {
        first = false;
      } else {
        buf << ",";
      }
      buf << obj->dump();
    }
    buf << "])";
    return buf.str();
  }

  Engine engine_singleton;

  Engine& engine() {
    return engine_singleton;
  }
}
