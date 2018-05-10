#include <cinttypes>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace AotJS {
  using ::std::wstring;
  using ::std::vector;
  using ::std::hash;
  using ::std::unordered_set;
  using ::std::unordered_map;

  typedef const char *Type;

  extern Type typeof_undefined;
  extern Type typeof_number;
  extern Type typeof_boolean;
  extern Type typeof_string;
  extern Type typeof_object;

  class Object;

  class Undefined {
    // just a tag
  };

  class Ref {
    union {
      double val_double;
      int64_t val_int64;
      int32_t val_int32;
    };

  public:

    // 13 bits reserved at top for NaN
    // 1 bit to mark whether it's a tag
    // 2 bits to mark subtype
    // 48 bits at the bottom for pointers (for native x86_64)
    // 16 bits apare + 32 bits at the bottom for ints
    static const int64_t tag_mask      = 0xffff000000000000LL;
    static const int64_t tag_double    = 0xfff8000000000000LL;
    static const int64_t tag_int32     = 0xfffc000000000000LL;
    static const int64_t tag_object    = 0xfffd000000000000LL;
    static const int64_t tag_bool      = 0xfffe000000000000LL;
    static const int64_t tag_undefined = 0xffff000000000000LL;

    Ref(double val) : val_double(val) {}
    Ref(int32_t val) : val_int64(((int64_t)val & ~tag_mask) | tag_int32) {}
    Ref(bool val) : val_int64(((int64_t)val & ~tag_mask) | tag_bool) {}
    Ref(Object *val) : val_int64((reinterpret_cast<int64_t>(val) & ~tag_mask) | tag_bool) {}
    Ref(Undefined val) : val_int64(tag_undefined) {}

    int64_t raw() const {
      return val_int64;
    }

    int64_t tag() const {
      return val_int64 & tag_mask;
    }

    bool is_double() const {
      return tag() == tag_double;
    }

    bool is_int32() const {
      return tag() == tag_int32;
    }

    bool isObject() const {
      return tag() == tag_object;
    }

    bool is_bool() const {
      return tag() == tag_bool;
    }

    bool is_undefined() const {
      return tag() == tag_undefined;
    }

    double asDouble() const {
      // Interpret all bits as double-precision float
      return val_double;
    }

    int32_t asInt32() const {
      // Bottom 32 bits are ours for ints.
      return val_int32;
    }

    bool asBool() const {
      // Bottom 1 bit is all we need!
      // But treat it like an int32.
      return (bool)val_int32;
    }

    Object *asObject() const {
  #if (PTRDIFF_MAX) > 2147483647
    // 64-bit host -- drop the top 16 bits of NaN and tag.
    // Assumes address space has only 48 significant bits
    // but may be signed, as on x86_64.
    return reinterpret_cast<Object *>((val_int64 << 16) >> 16);
  #else
    // 32 bit host -- bottom bits are ours, like an int.
    return reinterpret_cast<Object *>(val_int32);
  #endif
    }

    Undefined asUndefined() const {
      Undefined undef;
      return undef;
    }

    bool operator==(const Ref &rhs) const;
  };

}

namespace std {
  template<> struct hash<::AotJS::Ref> {
      size_t operator()(::AotJS::Ref const& ref) const noexcept;
  };
}

namespace AotJS {
  class PropList;

  class Object {
    Type typeof;
    Object *prototype;
    unordered_map<Ref,Ref> props;

    friend class PropList;

  public:

    Type getTypeof() const {
      return typeof;
    }

    Ref getProp(Ref name);
    void setProp(Ref name, Ref val);
    PropList listProps();
  };

  class PropList {
    Object *obj;

    PropList(Object *obj)
    : obj(obj) {
      //
    }

  friend class Object;

  public:

    unordered_map<Ref,Ref>::iterator begin() noexcept {
      return obj->props.begin();
    }

    unordered_map<Ref,Ref>::const_iterator cbegin() const noexcept {
      return obj->props.cbegin();
    }

    unordered_map<Ref,Ref>::iterator end() noexcept {
      return obj->props.end();
    }

    unordered_map<Ref,Ref>::const_iterator cend() const noexcept {
      return obj->props.cend();
    }
  };

  class String : public Object {
    wstring data;

  public:
    String(wstring const &src)
    : data(src) {
      //
    }

    operator wstring() const {
      return data;
    }

    bool operator==(const String &rhs) const {
      return data == rhs.data;
    }
  };

  class Heap {
    Object *root;
    unordered_set<Object *> objects;
    unordered_map<Object *, bool> marks;

    bool getMark(Object *obj);
    void setMark(Object *obj, bool val);
    void mark(Object *obj);
    void sweep(Object *obj);

  public:
    Heap(Object *aRoot)
    : root(aRoot) {
      registerObject(root);
    }

    void registerObject(Object *obj);
    void gc();
  };
}
