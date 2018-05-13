#include "aotjs_runtime.h"

#ifdef DEBUG
#include <iostream>
#endif

#include <sstream>

namespace AotJS {
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
    return hash<uint64_t>{}(ref.raw());
  }
}

namespace AotJS {
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
    } else if (isJSThing()) {
      buf << asJSThing().dump();
    }
    return buf.str();
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

  #pragma mark JSThing

  JSThing::~JSThing() {
    //
  }

  Typeof JSThing::typeof() const {
    return "invalid-jsthing";
  }

  string JSThing::dump() {
    return "JSThing";
  }

  #pragma mark PropIndex
  PropIndex::~PropIndex() {
    //
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

      // prop names are always either strings or symbols,
      // so they are pointers to GCThings.
      prop_name.asJSThing().markForGC();

      // prop values may not be, so check!
      if (prop_val.isJSThing()) {
        prop_val.asJSThing().markForGC();
      }
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

  #pragma mark Scope

  Scope::~Scope() {
    //
  }

  void Scope::markRefsForGC() {
    if (mParent) {
      mParent->markRefsForGC();
    }

    for (auto local : mLocals) {
      if (local.isJSThing()) {
        local.asJSThing().markRefsForGC();
      }
    }
  }

  string Scope::dump() {
    std::ostringstream buf;
    buf << "Scope(...)";
    return buf.str();
  }

  #pragma mark Frame

  Frame::~Frame() {
    //
  }

  void Frame::markRefsForGC() {
    if (mParent) {
      mParent->markRefsForGC();
    }

    mFunc->markRefsForGC();

    if (mThis.isJSThing()) {
      mThis.asJSThing().markRefsForGC();
    }
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
    if (mScope) {
      mScope->markRefsForGC();
    }
  }

  string Function::dump() {
    std::ostringstream buf;
    buf << "Function(\"" << name() << "\")";
    return buf.str();
  }

  #pragma mark Engine

  Engine::Engine()
  : mRoot(nullptr),
    mScopeStack(),
    mObjects(),
    mFrame(nullptr)
  {
    // We must clear those pointers first or else GC might explode
    // while allocating the root object!
    mRoot = new Object(*this);
  }

  void Engine::registerForGC(GCThing& obj) {
    // Because we don't yet control the allocation, we don't know how to walk
    // the heap looking for all objects. Instead, we need to keep a separate
    // set of all objects in order to do the final sweep.
    mObjects.insert(&obj);
  }

  Frame& Engine::pushFrame(
    Function& aFunc,
    Val aThis,
    std::vector<Val> aArgs)
  {
    auto frame = new Frame(*this, *mFrame, aFunc, aThis, aArgs);
    mFrame = frame;
    return *frame;
  }

  void Engine::popFrame() {
    if (mFrame) {
      mFrame = &mFrame->parent();
    } else {
      // should not happen!
      std::abort();
    }
  }

  Scope& Engine::pushScope(Scope& aParent, size_t aSize)
  {
    mScopeStack.push_back(new Scope(*this, aParent, aSize));
    return *mScopeStack.back();
  }

  Scope& Engine::pushScope(size_t aSize) {
    mScopeStack.push_back(new Scope(*this, aSize));
    return *mScopeStack.back();
  }

  void Engine::popScope() {
    mScopeStack.pop_back();
  }

  Val Engine::call(Val aFunc, Val aThis, std::vector<Val> aArgs) {
    if (aFunc.isFunction()) {
      auto func = aFunc.asFunction();
      auto frame = pushFrame(func, aThis, aArgs);
      Val retval = func.body()(*this, func, frame);
      // we know popFrame can't trigger GC
      popFrame();
      return retval;
    } else {
      // todo do something
      #ifdef DEBUG
      std::cerr << "cannot call a non-function\n";
      #endif
      std::abort();
    }
  }

  void Engine::gc() {
    #ifdef DEBUG
    std::cerr << "starting gc mark/sweep:\n";
    #endif

    // 1) Mark!
    // Mark anything reachable from the global root object.
    if (mRoot) {
      mRoot->markForGC();
    }

    // Mark anything on the stack of currently open scopes
    for (auto scope : mScopeStack) {
      scope->markForGC();
    }

    // Todo: merge args/frame stack with scope stack?
    if (mFrame) {
      mFrame->markForGC();
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
