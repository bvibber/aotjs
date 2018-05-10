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

  class Null {
    // just a tag
  };

  ///
  /// Polymorphic JS values are handled by using 64-bit values with
  /// NaN signalling, so they may contain either a double-precision float
  /// or a typed value with up to a 48-bit pointer or integer payload.
  ///
  /// This is similar to, but not the same as, the "Pun-boxing" used in
  /// Mozilla's SpiderMonkey engine:
  /// * https://github.com/mozilla/gecko-dev/blob/master/js/public/Value.h
  ///
  /// May not be optimal for wasm32 yet.
  class Val {
    union {
      uint64_t val_raw;
      int32_t val_int32;
      double val_double;
    };

  public:

    // 13 bits reserved at top for NaN
    //   one bit for the sign, haughty on his throne
    //   eleven 1s for exponent, expanding through the 'verse
    //   last 1 for the NaN marker, whispered in the night
    // 3 bits to mark the low-level tag type
    //   alchemy clouds its mind
    // up to 48 bits for payload
    //   x86_64 needs all 48 bits for pointers, or just 47 for user mode...?
    //   aarch64 may need 48 bits too, and ... isn't signed?
    //   ints, bools, 32-bit pointers use bottom 32 bits
    //   null, undefined don't use any of the payload
    static const uint64_t sign_bit       = 0b1000000000000000'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_mask       = 0b1111111111111111'0000000000000000'0000000000000000'0000000000000000;
    // double cutoff: -Infinity
    static const uint64_t tag_max_double = 0b1111111111110000'0000000000000000'0000000000000000'0000000000000000;
    // integer types:
    static const uint64_t tag_bool       = 0b1111111111111001'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_int32      = 0b1111111111111010'0000000000000000'0000000000000000'0000000000000000;
    // pointer types:
    static const uint64_t tag_string     = 0b1111111111111011'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_symbol     = 0b1111111111111100'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_object     = 0b1111111111111101'0000000000000000'0000000000000000'0000000000000000;
    // tag-only types
    static const uint64_t tag_null       = 0b1111111111111110'0000000000000000'0000000000000000'0000000000000000;
    static const uint64_t tag_undefined  = 0b1111111111111111'0000000000000000'0000000000000000'0000000000000000;

    Val(const Val &val) : val_raw(val.raw()) {}
    Val(double val)     : val_double(val) {}
    Val(int32_t val)    : val_raw(((uint64_t)((int64_t)val) & ~tag_mask) | tag_int32) {}
    Val(bool val)       : val_raw(((uint64_t)val & ~tag_mask) | tag_bool) {}
    Val(Object *val)    : val_raw((reinterpret_cast<uint64_t>(val) & ~tag_mask) | tag_bool) {}
    Val(Undefined val)  : val_raw(tag_undefined) {}
    Val(Null val)       : val_raw(tag_null) {}

    uint64_t raw() const {
      return val_raw;
    }

    uint64_t tag() const {
      return val_raw & tag_mask;
    }

    bool isDouble() const {
      // Saw this trick in SpiderMonkey.
      // Our tagged values will be > tag_max_double in uint64_t interpretation
      // Any negative double will be < that
      // Any positive double, inverted, will be < that
      return (val_raw | sign_bit) <= tag_max_double;
    }

    bool isInt32() const {
      return tag() == tag_int32;
    }

    bool isObject() const {
      return tag() == tag_object;
    }

    bool isBool() const {
      return tag() == tag_bool;
    }

    bool isUndefined() const {
      return tag() == tag_undefined;
    }

    bool isNull() const {
      return tag() == tag_null;
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
    return reinterpret_cast<Object *>((val_raw << 16) >> 16);
  #else
    // 32 bit host -- bottom bits are ours, like an int.
    return reinterpret_cast<Object *>(val_int32);
  #endif
    }

    Undefined asUndefined() const {
      Undefined undef;
      return undef;
    }

    bool operator==(const Val &rhs) const;
  };

}

namespace std {
  template<> struct hash<::AotJS::Val> {
      size_t operator()(::AotJS::Val const& Val) const noexcept;
  };
}

namespace AotJS {
  class PropList;

  class Object {
    Type typeof;
    Object *prototype;
    unordered_map<Val,Val> props;

    friend class PropList;

  public:

    Type getTypeof() const {
      return typeof;
    }

    Val getProp(Val name);
    void setProp(Val name, Val val);
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

    unordered_map<Val,Val>::iterator begin() noexcept {
      return obj->props.begin();
    }

    unordered_map<Val,Val>::const_iterator cbegin() const noexcept {
      return obj->props.cbegin();
    }

    unordered_map<Val,Val>::iterator end() noexcept {
      return obj->props.end();
    }

    unordered_map<Val,Val>::const_iterator cend() const noexcept {
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
