#include <cinttypes>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// 13 bits reserved at top for NaN
// 1 bit to mark whether it's a tag
// 2 bits to mark subtype
// 48 bits at the bottom for pointers (for native x86_64)
// 16 bits apare + 32 bits at the bottom for ints
#define TAG_MASK      0xffff000000000000LL
#define TAG_DOUBLE    0xfff8000000000000LL
#define TAG_INT32     0xfffc000000000000LL
#define TAG_OBJECT    0xfffd000000000000LL
#define TAG_BOOL      0xfffe000000000000LL
#define TAG_UNDEFINED 0xffff000000000000LL

typedef const char *aotjs_type;

extern aotjs_type typeof_undefined;
extern aotjs_type typeof_number;
extern aotjs_type typeof_boolean;
extern aotjs_type typeof_string;
extern aotjs_type typeof_object;

class aotjs_object;

class aotjs_undefined {
  // just a tag
};

class aotjs_ref {
  union {
    double val_double;
    int64_t val_int64;
    int32_t val_int32;
  };

public:

  aotjs_ref(double val) : val_double(val) {}
  aotjs_ref(int32_t val) : val_int64(((int64_t)val & ~TAG_MASK) | TAG_INT32) {}
  aotjs_ref(bool val) : val_int64(((int64_t)val & ~TAG_MASK) | TAG_BOOL) {}
  aotjs_ref(aotjs_object *val) : val_int64((reinterpret_cast<int64_t>(val) & ~TAG_MASK) | TAG_BOOL) {}
  aotjs_ref(aotjs_undefined val) : val_int64(TAG_UNDEFINED) {}

  int64_t raw() const {
    return val_int64;
  }

  int64_t tag() const {
    return val_int64 & TAG_MASK;
  }

  bool is_double() const {
    return tag() == TAG_DOUBLE;
  }

  bool is_int32() const {
    return tag() == TAG_INT32;
  }

  bool is_object() const {
    return tag() == TAG_OBJECT;
  }

  bool is_bool() const {
    return tag() == TAG_BOOL;
  }

  bool is_undefined() const {
    return tag() == TAG_UNDEFINED;
  }

  double as_double() const {
    // Interpret all bits as double-precision float
    return val_double;
  }

  int32_t as_int32() const {
    // Bottom 32 bits are ours for ints.
    return val_int32;
  }

  bool as_bool() const {
    // Bottom 1 bit is all we need!
    // But treat it like an int32.
    return (bool)val_int32;
  }

  aotjs_object *as_object() const {
#if (PTRDIFF_MAX) > 2147483647
  // 64-bit host -- drop the top 16 bits of NaN and tag.
  // Assumes address space has only 48 significant bits
  // but may be signed, as on x86_64.
  return reinterpret_cast<aotjs_object *>((val_int64 << 16) >> 16);
#else
  // 32 bit host -- bottom bits are ours, like an int.
  return reinterpret_cast<aotjs_object *>(val_int32);
#endif
  }

  aotjs_undefined as_undefined() const {
    aotjs_undefined undef;
    return undef;
  }

  bool operator==(const aotjs_ref &rhs) const;
};

namespace std {
  template<> struct hash<aotjs_ref> {
      size_t operator()(aotjs_ref const& ref) const noexcept;
  };
}

class aotjs_proplist;

class aotjs_object {
  aotjs_type typeof;
  aotjs_object *prototype;
  std::unordered_map<aotjs_ref,aotjs_ref> props;

  friend class aotjs_proplist;

public:

  aotjs_type get_typeof() const {
    return typeof;
  }

  aotjs_ref get_prop(aotjs_ref name);
  void set_prop(aotjs_ref name, aotjs_ref val);
  aotjs_proplist list_props();
};

class aotjs_proplist {
  aotjs_object *obj;

  aotjs_proplist(aotjs_object *obj)
  : obj(obj) {
    //
  }

friend class aotjs_object;

public:

  std::unordered_map<aotjs_ref,aotjs_ref>::iterator begin() noexcept {
    return obj->props.begin();
  }

  std::unordered_map<aotjs_ref,aotjs_ref>::const_iterator cbegin() const noexcept {
    return obj->props.cbegin();
  }

  std::unordered_map<aotjs_ref,aotjs_ref>::iterator end() noexcept {
    return obj->props.end();
  }

  std::unordered_map<aotjs_ref,aotjs_ref>::const_iterator cend() const noexcept {
    return obj->props.cend();
  }
};

class aotjs_string : public aotjs_object {
  std::wstring data;

public:
  aotjs_string(std::wstring const &src)
  : data(src) {
    //
  }

  operator std::wstring() const {
    return data;
  }

  bool operator==(const aotjs_string &rhs) const {
    return data == rhs.data;
  }
};

class aotjs_heap {
  aotjs_object *root;
  std::unordered_set<aotjs_object *> objects;
  std::unordered_map<aotjs_object *, bool> marks;

  bool gc_get_mark(aotjs_object *obj);
  void gc_set_mark(aotjs_object *obj, bool val);
  void gc_mark(aotjs_object *obj);
  void gc_sweep(aotjs_object *obj);

public:
  aotjs_heap(aotjs_object *aRoot)
  : root(aRoot) {
    register_object(root);
  }

  void register_object(aotjs_object *obj);
  void gc();
};
