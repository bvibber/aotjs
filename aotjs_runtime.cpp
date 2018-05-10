#include "aotjs_runtime.h"

namespace aotjs {
  aotjs_type typeof_undefined = "undefined";
  aotjs_type typeof_number = "number";
  aotjs_type typeof_boolean = "boolean";
  aotjs_type typeof_string = "string";
  aotjs_type typeof_object = "object";
}

#pragma mark aotjs_ref hash helpers

namespace std {
  size_t hash<::aotjs::aotjs_ref>::operator()(::aotjs::aotjs_ref const& ref) const noexcept {
    return hash<int64_t>{}(ref.raw());
  }
}

#pragma mark aotjs_proplist

namespace aotjs {
  bool aotjs_ref::operator==(const aotjs_ref &rhs) const {
    // Bit-identical always matches!
    // This catches matching pointers (same object), matching ints, etc.
    if (raw() == rhs.raw()) {
      return true;
    }

    if (is_object() && rhs.is_object()) {
      auto obj_l = as_object();
      auto type_l = obj_l->get_typeof();

      auto obj_r = rhs.as_object();
      auto type_r = obj_r->get_typeof();

      if (type_l == typeof_string && type_r == typeof_string) {
        // Two string instances may still be equal.
        auto str_l = static_cast<aotjs_string *>(obj_l);
        auto str_r = static_cast<aotjs_string *>(obj_r);
        return (*str_l) == (*str_r);
      }
    }

    // Non-identical non-string objects never compare identical.
    return false;
  }

  #pragma mark aotjs_object

  aotjs_ref aotjs_object::get_prop(aotjs_ref name) {
    auto index = props.find(name);
    if (index == props.end()) {
      if (prototype) {
        return prototype->get_prop(name);
      } else {
        return aotjs_undefined();
      }
    } else {
      return index->first;
    }
  }

  void aotjs_object::set_prop(aotjs_ref name, aotjs_ref val) {
    props.insert_or_assign(name, val);
  }

  aotjs_proplist aotjs_object::list_props() {
    aotjs_proplist props = this;
    return props;
  }

  #pragma mark aotjs_heap

  bool aotjs_heap::gc_get_mark(aotjs_object *obj) {
    auto mark = marks.find(obj);
    if (mark->first) {
      return mark->second;
    } else {
      return false;
    }
  }

  void aotjs_heap::gc_set_mark(aotjs_object *obj, bool val) {
    marks.insert_or_assign(obj, val);
  }

  void aotjs_heap::register_object(aotjs_object *obj) {
    objects.insert(obj);
    gc_set_mark(obj, false);
  }

  void aotjs_heap::gc_mark(aotjs_object *obj) {
    if (!gc_get_mark(obj)) {
      gc_set_mark(obj, true);

      for (auto [prop_name, prop_val] : obj->list_props()) {
        // prop names are always either strings or symbols, so objects.
        gc_mark(prop_name.as_object());

        // prop values may not be, so check!
        if (prop_val.is_object()) {
          gc_mark(prop_val.as_object());
        }
      }
    }
  }

  void aotjs_heap::gc_sweep(aotjs_object *obj) {
    if (gc_get_mark(obj)) {
      // Keep the object, but reset the marker value for next time.
      gc_set_mark(obj, false);
    } else {
      // No findable references to this object. Destroy it!
      objects.erase(obj);
      marks.erase(obj);
      delete obj;
    }
  }

  void aotjs_heap::gc() {
    gc_mark(root);

    // Todo: search stack frames / held-live objects

    // Todo: don't require allocating memory to free memory
    std::vector<aotjs_object *> dead_objects;
    for (auto obj : objects) {
      if (gc_get_mark(obj)) {
        dead_objects.push_back(obj);
      }
    }
    for (auto obj : dead_objects) {
      gc_sweep(obj);
    }
  }
}
