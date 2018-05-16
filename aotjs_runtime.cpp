#include "aotjs_runtime.h"

#ifdef DEBUG
#include <iostream>
#endif

#include <sstream>

namespace AotJS {
  Typeof typeof_jsthing = "jsthing"; // base class should not be exposed
  Typeof typeof_undefined = "undefined";
  Typeof typeof_number = "number";
  Typeof typeof_boolean = "boolean";
  Typeof typeof_string = "string";
  Typeof typeof_symbol = "symbol";
  Typeof typeof_function = "function";
  Typeof typeof_object = "object";
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

  Engine* engine_singleton = new Engine();

  bool Val::operator==(const Val &rhs) const {
    // Bit-identical always matches!
    // This catches matching pointers (same object), matching ints, etc.
    if (raw() == rhs.raw()) {
      return true;
    }

    if (isString() && rhs.isString()) {
      // Two string instances may still compare equal.
      return asString() == rhs.asString();
    }

    // Non-identical non-string objects never compare identical.
    return false;
  }

  Local Val::operator+(const Val& rhs) const {
    // todo implement this correctly
    if (isString() || rhs.isString()) {
      return Local(*toString() + *rhs.toString());
    } else {
      return Local(toDouble() + rhs.toDouble());
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

  Retained<String> Val::toString() const
  {
    if (isJSThing()) {
      return asJSThing().toString();
    } else {
      // todo: flip this?
      return retain<String>(dump());
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

  Local Val::call(Val aThis, std::vector<Val> aArgs) const
  {
    if (isFunction()) {
      return asFunction().call(aThis, aArgs);
    } else {
      #ifdef DEBUG
      std::cerr << "not a function\n";
      #endif
      std::abort();
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

  #pragma mark Object

  Object::~Object() {
    //
  }

  Typeof Object::typeof() const {
    return typeof_object;
  }

  static Val normalizePropName(Val aName) {
    if (aName.isString()) {
      return aName;
    } else if (aName.isSymbol()) {
      return aName;
    } else {
      // todo: convert to string
      // hrm, we need a ref to an engine for that?
      #ifdef DEBUG
      std::cerr << "cannot use non-string/symbol as property index";
      #endif
      std::abort();
      return Undefined();
    }
  }

  // todo: handle numeric indices
  // todo: getters
  Val Object::getProp(Val aName) {
    // fixme retain this properly with a scope
    Val name = normalizePropName(aName);
    auto index = mProps.find(name);
    if (index == mProps.end()) {
      if (mPrototype) {
        return mPrototype->getProp(name);
      } else {
        return Undefined();
      }
    } else {
      return index->first;
    }
  }

  void Object::setProp(Val aName, Val aVal) {
    // fixme retain this properly with a scope
    auto name = normalizePropName(aName);
    mProps.emplace(name, aVal);
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

  Typeof String::typeof() const {
    return typeof_string;
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

  Typeof Symbol::typeof() const {
    return typeof_symbol;
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

  #pragma mark JSThing

  JSThing::~JSThing() {
    //
  }

  Typeof JSThing::typeof() const {
    return typeof_jsthing;
  }

  Retained<String> JSThing::toString() const {
    return retain<String>("[jsthing JSThing]");
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

  #pragma mark Frame

  Frame::~Frame() {
    //
  }

  void Frame::markRefsForGC() {
    mFunc->markForGC();

    mThis.markForGC();
  }

  string Frame::dump() {
    std::ostringstream buf;
    buf << "Frame(...)";
    return buf.str();
  }

  #pragma mark Function

  Function::~Function() {
    //
  }

  Typeof Function::typeof() const {
    return typeof_function;
  }

  void Function::markRefsForGC() {
    for (auto cell : mCaptures) {
      cell->val().markForGC();
    }
  }


  Local Function::call(Val aThis, std::vector<Val> aArgs) {
    auto frame = retain<Frame>(*this, aThis, aArgs);
    return mBody(*this, *frame);
  }

  string Function::dump() {
    std::ostringstream buf;
    buf << "Function(\"" << name() << "\")";
    return buf.str();
  }

  #pragma mark Engine

  Engine::Engine(size_t aStackSize)
  : mRoot(nullptr),
    mLocalStack(),
    mObjects()
  {
    // horrible singleton cheating
    engine_singleton = this;

    // warning: if stack goes beyond this it will explode
    mLocalStack.reserve(aStackSize);

    // We must clear those pointers first or else GC might explode
    // while allocating the root object!
    mRoot = new Object();
  }

  void Engine::registerForGC(GCThing& obj) {
    // Because we don't yet control the allocation, we don't know how to walk
    // the heap looking for all objects. Instead, we need to keep a separate
    // set of all objects in order to do the final sweep.
    mObjects.insert(&obj);
  }

  Val* Engine::pushLocal(Val val)
  {
    mLocalStack.push_back(val);
    return &mLocalStack.back();
  }

  void Engine::popLocal() {
    mLocalStack.pop_back();
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

    // Mark anything on the stack of currently open scopes
    for (auto& record : mLocalStack) {
      record.markForGC();
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
}
