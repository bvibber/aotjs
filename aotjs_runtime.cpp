#include "aotjs_runtime.h"

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

    if (isObject() && rhs.isObject()) {
      auto obj_l = asObject();
      auto type_l = obj_l->getTypeof();

      auto obj_r = rhs.asObject();
      auto type_r = obj_r->getTypeof();

      if (type_l == typeof_string && type_r == typeof_string) {
        // Two string instances may still be equal.
        auto str_l = static_cast<String *>(obj_l);
        auto str_r = static_cast<String *>(obj_r);
        return (*str_l) == (*str_r);
      }
    }

    // Non-identical non-string objects never compare identical.
    return false;
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

  PropList Object::listProps() {
    PropList props = this;
    return props;
  }

  #pragma mark Heap

  bool Heap::getMark(Object *obj) {
    auto mark = marks.find(obj);
    if (mark->first) {
      return mark->second;
    } else {
      return false;
    }
  }

  void Heap::setMark(Object *obj, bool val) {
    marks.emplace(obj, val);
  }

  void Heap::registerObject(Object *obj) {
    objects.insert(obj);
    setMark(obj, false);
  }

  void Heap::mark(Object *obj) {
    if (!getMark(obj)) {
      setMark(obj, true);

      for (auto iter : obj->listProps()) {
        auto prop_name(iter.first);
        auto prop_val(iter.second);
        // prop names are always either strings or symbols, so objects.
        mark(prop_name.asObject());

        // prop values may not be, so check!
        if (prop_val.isObject()) {
          mark(prop_val.asObject());
        }
      }
    }
  }

  void Heap::sweep(Object *obj) {
    if (getMark(obj)) {
      // Keep the object, but reset the marker value for next time.
      setMark(obj, false);
    } else {
      // No findable references to this object. Destroy it!
      objects.erase(obj);
      marks.erase(obj);
      delete obj;
    }
  }

  void Heap::gc() {
    mark(root);

    // Todo: search stack frames / held-live objects

    // Todo: don't require allocating memory to free memory
    std::vector<Object *> dead_objects;
    for (auto obj : objects) {
      if (getMark(obj)) {
        dead_objects.push_back(obj);
      }
    }
    for (auto obj : dead_objects) {
      sweep(obj);
    }
  }
}
