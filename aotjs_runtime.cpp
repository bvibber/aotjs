#include "aotjs_runtime.h"

#include <sstream>

namespace AotJS {
  Type typeof_undefined = "undefined";
  Type typeof_number = "number";
  Type typeof_boolean = "boolean";
  Type typeof_string = "string";
  Type typeof_object = "object";
}

#pragma mark Val hash helpers

namespace std {
  size_t hash<::AotJS::Val>::operator()(::AotJS::Val const& ref) const noexcept {
    return hash<int64_t>{}(ref.raw());
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
    } else if (isGCThing()) {
      buf << asGCThing()->dump();
    }
    return buf.str();
  }

  #pragma mark GCThing

  GCThing::GCThing(Heap *aHeap) :
    heap(aHeap),
    marked(0)
  {
    //
  }

  GCThing::~GCThing() {
    //
  }

  bool GCThing::isMarkedForGC() {
    return marked;
  }

  void GCThing::markForGC() {
    if (!marked) {
      marked = true;
      markRefsForGC();
    }
  }

  void GCThing::clearForGC() {
    marked = false;
  }

  void GCThing::markRefsForGC() {
    // no-op default
  }

  string GCThing::dump() {
    return "GCThing";
  }

  #pragma mark Object

  Val Object::getProp(Val name) {
    auto index = props.find(name);
    if (index == props.end()) {
      if (prototype) {
        return prototype->getProp(name);
      } else {
        return Undefined();
      }
    } else {
      return index->first;
    }
  }

  void Object::setProp(Val name, Val val) {
    props.emplace(name, val);
  }

  void Object::markRefsForGC() {
    for (auto iter : props) {
      auto prop_name(iter.first);
      auto prop_val(iter.second);

      // prop names are always either strings or symbols,
      // so they are pointers to GCThings.
      prop_name.asGCThing()->markForGC();

      // prop values may not be, so check!
      if (prop_val.isGCThing()) {
        prop_val.asGCThing()->markForGC();
      }
    }
  }

  string Object::dump() {
    std::ostringstream buf;
    buf << "{";

    bool first = true;
    for (auto iter : props) {
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
    buf << "}";
    return buf.str();
  }

  string String::dump() {
    // todo: emit JSON or something
    std::ostringstream buf;
    buf << "\"";
    buf << data;
    buf << "\"";
    return buf.str();
  }

  string Symbol::dump() {
    std::ostringstream buf;
    buf << "Symbol(\"";
    buf << name;
    buf << "\")";
    return buf.str();
  }

  #pragma mark Heap

  void Heap::registerForGC(GCThing *obj) {
    objects.insert(obj);
  }

  Object *Heap::newObject(Object *prototype) {
    auto obj = new Object(this, prototype);
    registerForGC(obj);
    return obj;
  }

  String *Heap::newString(const string &aStr) {
    auto str = new String(this, aStr);
    registerForGC(str);
    return str;
  }

  Symbol *Heap::newSymbol(const string &aName) {
    auto sym = new Symbol(this, aName);
    registerForGC(sym);
    return sym;
  }

  void Heap::gc() {
    root->markForGC();

    // Todo: search stack frames / held-live objects

    // Todo: don't require allocating memory to free memory!
    std::vector<GCThing *> dead_objects;
    for (auto obj : objects) {
      if (!obj->isMarkedForGC()) {
        dead_objects.push_back(obj);
      }
    }
    for (auto obj : dead_objects) {
      if (obj->isMarkedForGC()) {
        // Keep the object, but reset the marker value for next time.
        obj->clearForGC();
      } else {
        // No findable references to this object. Destroy it!
        objects.erase(obj);
        delete obj;
      }
    }
  }

  string Heap::dump() {
    std::ostringstream buf;
    buf << "Heap([";

    bool first = true;
    for (auto obj : objects) {
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
