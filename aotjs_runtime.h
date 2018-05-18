#ifndef AOTJS_RUNTIME
#define AOTJS_RUNTIME

#ifdef DEBUG
#include <iostream>
#endif

#include <cinttypes>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace AotJS {
  using ::std::string;
  using ::std::vector;
  using ::std::hash;
  using ::std::unordered_set;
  using ::std::unordered_map;

  typedef const char *TypeOf;

  extern TypeOf typeOfGCThing;
  extern TypeOf typeOfInternal;
  extern TypeOf typeOfJSThing;

  extern TypeOf typeOfBoxDouble;
  extern TypeOf typeOfBoxInt32;

  extern TypeOf typeOfUndefined;
  extern TypeOf typeOfNull;
  extern TypeOf typeOfNumber;
  extern TypeOf typeOfBoolean;
  extern TypeOf typeOfString;
  extern TypeOf typeOfSymbol;
  extern TypeOf typeOfFunction;
  extern TypeOf typeOfObject;

  class Val;
}

namespace std {
  template<> struct hash<::AotJS::Val> {
    size_t operator()(::AotJS::Val const& ref) const noexcept;
  };
}

namespace AotJS {
  typedef Val* Binding;
  template <typename T> class Box;

  class Engine;

  class Local;
  template <class T> class Retained;

  class Scope;
  class RetVal;
  template <class T> class Ret;

  typedef std::initializer_list<Local> RawArgList;
  class ArgList;

  class GCThing;
  class Cell;
  class Frame;

  typedef std::initializer_list<Cell*> RawCaptureList;

  class PropIndex;
  class String;
  class Symbol;

  class Object;
  class Function;

  class Undefined {
    // just a tag
  };

  class Null {
    // just a tag
  };

  class Deleted {
    // just a tag
  };

  typedef RetVal (*FunctionBody)(Function& func, Local this_, ArgList args);

  ///
  /// Represents an entire JS world.
  ///
  /// Garbage collection is run when gc() is called manually,
  /// or just let everything deallocate when the engine is
  /// destroyed.
  ///
  class Engine {
    // Flag to disable GC until we've finished initializing.
    // We need to be able to create a few sigil objects before
    // it's possible to cleanly run the GC system.
    bool mReadyForGC;

    // Sigil values with special boxed values
    Box<Undefined>* mUndefined;
    Box<Null>* mNull;
    Box<Deleted>* mDeleted;
    Box<bool>* mFalse;
    Box<bool>* mTrue;

    // Global root object.
    Object* mRoot;

    // a stack of Val cells, to which we keep pointers in vars and 'real' stack.
    // Or, in theory we could scan the real stack but may have false values
    // and could run into problems with values optimized into locals which
    // aren't on the emscripten actual stack in wasm!
    // todo: don't need vector really here?
    Val* mStackBegin;
    Val* mStackTop;
    Val* mStackEnd;

    // Set of all live objects.
    // Todo: replace this with an allocator we know how to walk!
    unordered_set<GCThing *> mObjects;

    void registerForGC(GCThing& aObj);
    friend class GCThing;

    friend class Local;
    friend class Scope;
    friend class ScopeRetVal;
    friend class RetVal;
    template <class T> friend class ScopeRet;
    template <class T> friend class Ret;
    friend class ArgList;
    Val* stackTop();
    Val* pushLocal(Val ref);
    void popLocal(Val* mRecord);

  public:
    static const size_t defaultStackSize = 256 * 1024;

    // Todo allow specializing the root object at instantiation time?
    Engine(size_t aStackSize);

    Engine()
    : Engine(defaultStackSize)
    {
      //
    }

    ~Engine();

    // ick!
    void setRoot(Object& aRoot) {
      mRoot = &aRoot;
    }

    Object* root() const {
      return mRoot;
    }

    // Sigil values
    Box<Undefined>* undefinedRef() const {
      return mUndefined;
    }

    Box<Null>* nullRef() const {
      return mNull;
    }

    Box<Deleted>* deletedRef() const {
      return mDeleted;
    }

    Box<bool>* falseRef() const {
      return mFalse;
    }

    Box<bool>* trueRef() const {
      return mTrue;
    }

    void gc();
    string dump();
  };

  // Singleton
  extern Engine engine_singleton;

  // Singleton access
  Engine& engine();

  ///
  /// Base class for an item that can be garbage-collected and may
  /// reference other GC-able items.
  ///
  /// Not necessarily exposed to JS.
  ///
  class GCThing {
    ///
    /// GC mark state -- normally false, except during GC marking
    /// when it records true if the object is reachable.
    ///
    bool mMarked;

  public:
    GCThing()
    :
      mMarked(false)
    {
      #ifdef FORCE_GC
      // Force garbage collection to happen on every allocation.
      // Should shake out some bugs.
      engine().gc();
      #endif

      // We need a set of *all* allocated objects to do sweep.
      // todo: put this in the allocator?
      engine().registerForGC(*this);
    }

    virtual ~GCThing();

    // To be called only by Engine...

    bool isMarkedForGC() const {
      return mMarked;
    }

    void markForGC() {
      if (!isMarkedForGC()) {
        mMarked = true;
        #ifdef DEBUG
        std::cerr << "marking object live " << dump() << "\n";
        #endif
        markRefsForGC();
      }
    }

    void clearForGC() {
      mMarked = false;
    }

    virtual void markRefsForGC();

    // Really public!
    virtual string dump();

    virtual TypeOf typeOf() const;

    virtual Ret<String> toString() const;
  };

  // Internal classes that should not be exposed to JS
  class Internal : public GCThing {
  public:
    Internal()
    : GCThing()
    {
      //
    }

    ~Internal() override;
    TypeOf typeOf() const override;
  };

  ///
  /// Child classes are JS-exposed objects, but not necessarily Objects.
  ///
  class JSThing : public GCThing {
  public:
    JSThing()
    : GCThing()
    {
      //
    }

    ~JSThing() override;
    TypeOf typeOf() const override;

    Ret<String> toString() const override;
  };

  template <typename T>
  class Box : public GCThing {
    T mVal;

  public:
    Box(T aVal) : mVal(aVal) {}

    T val() const {
      return mVal;
    }

    TypeOf typeOf() const override;
  };

  ///
  /// Polymorphic JS values are handled by using pointer-sized values with
  /// either a pointer to a GCThing or a tagged 31-bit integer with a tag bit
  /// in the lowest bit.
  ///
  /// Unlike NaN-boxing this means double-precision floats and some int32s
  /// must be boxed into GCThing subclasses and allocated on the heap.
  /// Other values like undefined, null, and boolean use special sigil
  /// objects that don't have to be allocated on each use.
  ///
  /// However it is closer to the available reference types in the Wasm
  /// garbage collection proposal: https://github.com/WebAssembly/gc/pull/34
  /// which includes an int31ref tagged type which can be freely mixed with
  /// references.
  ///
  /// And I don't expect high-performance float math to be a big use case
  /// for this plugin model, so we'll live with the boxing.
  class Val {
    union {
      size_t mRaw;
      GCThing* mPtr;
    };

    static bool isValidInt31(int32_t aInt) {
      return ((aInt << 1) >> 1) == aInt;
    }

    static GCThing* tagInt31(int32_t aInt) {
       return reinterpret_cast<GCThing*>(static_cast<size_t>((aInt << 1) | 1));
    }

    static GCThing* tagOrBoxInt32(int32_t aInt) {
      if (isValidInt31(aInt)) {
        return tagInt31(aInt);
      } else {
        return new Box<int32_t>(aInt);
      }
    }

    static int32_t derefInt31(const GCThing* aPtr) {
      return static_cast<int32_t>(reinterpret_cast<size_t>(aPtr)) >> 1;
    }

  public:

    #ifdef DEBUG
    Val(GCThing* aRef) {
      if (!aRef){
         // don't use null pointers!
        std::abort();
      }
      mPtr = aRef;
    }
    #else
    Val(GCThing* aRef)        : mPtr(aRef) {}
    #endif

    Val(const GCThing* aRef)  : Val(const_cast<GCThing*>(aRef)) {} // don't use null pointers!
    Val(double aDouble)       : Val(new Box<double>(aDouble)) {}
    Val(int32_t aInt)         : Val(tagOrBoxInt32(aInt)) {};
    Val(bool aBool)           : Val(aBool ? engine().trueRef() : engine().falseRef()) {}
    Val(Null aNull)           : Val(engine().nullRef()) {}
    Val(Undefined aUndefined) : Val(engine().undefinedRef()) {}
    Val(Deleted aDeleted)     : Val(engine().deletedRef()) {}

    Val(const Val &aVal) : mRaw(aVal.raw()) {}
    Val(Val &aVal)       : mRaw(aVal.raw()) {}
    Val()                : Val(Undefined()) {}

    Val &operator=(const Val &aVal) {
      mRaw = aVal.mRaw;
      return *this;
    }

    size_t raw() const {
      return mRaw;
    }

    bool tag() const {
      return mRaw & 1;
    }

    bool isInt31() const {
      return tag();
    }

    bool isGCThing() const {
      return !tag();
    }

    bool isTypeOf(TypeOf aExpected) const {
      return isGCThing() && (mPtr->typeOf() == aExpected);
    }

    bool isDouble() const {
      return isTypeOf(typeOfBoxDouble);
    }

    bool isInt32() const {
      return isTypeOf(typeOfBoxInt32);
    }

    bool isBool() const {
      return (mPtr == engine().trueRef()) || (mPtr == engine().falseRef());
    }

    bool isUndefined() const {
      return (mPtr == engine().undefinedRef());
    }

    bool isNull() const {
      return (mPtr == engine().nullRef());
    }

    bool isInternal() const {
      return isGCThing() && isTypeOf(typeOfInternal);
    }

    bool isJSThing() const {
      return isGCThing() && !isTypeOf(typeOfInternal);
    }

    bool isObject() const {
      return isTypeOf(typeOfObject);
    }

    bool isString() const {
      return isTypeOf(typeOfString);
    }

    bool isSymbol() const {
      return isTypeOf(typeOfSymbol);
    }

    bool isFunction() const {
      return isTypeOf(typeOfFunction);
    }

    double asDouble() const {
        return reinterpret_cast<Box<double>*>(mPtr)->val();
    }

    int32_t asInt31() const {
      return derefInt31(mPtr);
    }

    int32_t asInt32() const {
      return reinterpret_cast<Box<int32_t>*>(mPtr)->val();
    }

    bool asBool() const {
      return reinterpret_cast<Box<bool>*>(mPtr)->val();
    }

    Null asNull() const {
      return Null();
    }

    Undefined asUndefined() const {
      return Undefined();
    }

    Deleted asDeleted() const {
      return Deleted();
    }

    // Un-checked conversions returning a raw thingy

    GCThing& asGCThing() const {
      return *mPtr;
    }

    Internal& asInternal() const {
      return *static_cast<Internal *>(mPtr);
    }

    JSThing& asJSThing() const {
      return *static_cast<JSThing*>(mPtr);
    }

    Object& asObject() const {
      // fixme it doesn't understand the type rels
      // clean up these methods later
      return *reinterpret_cast<Object *>(mPtr);
    }

    String& asString() const {
      return *reinterpret_cast<String *>(mPtr);
    }

    Symbol& asSymbol() const {
      return *reinterpret_cast<Symbol *>(mPtr);
    }

    Function& asFunction() const {
      return *reinterpret_cast<Function *>(mPtr);
    }

    // Unchecked conversions returning a *T, castable to Retained<T>
    // or Ret<T>.
    template <class T>
    T& as() const {
      return *static_cast<T*>(mPtr);
    }

    // Checked conversions
    bool toBool() const;
    int32_t toInt32() const;
    double toDouble() const;
    Ret<String> toString() const;

    bool operator==(const Val& rhs) const;

    RetVal operator+(const Val& rhs) const;

    string dump() const;

    void markForGC() const {
      if (isGCThing()) {
        asGCThing().markForGC();
      }
    }

    RetVal call(Local aThis, RawArgList aArgs) const;

  };

  class SmartVal {
  protected:
    // The smart pointer contains a single pointer word to the value we want
    // to work with.
    Val* mRecord;

    SmartVal(Val *aRecord)
    : mRecord(aRecord)
    {
    }

  public:
    // Deref ops
    Val& operator*() const {
      return *mRecord;
    }

    const Val* operator->() const {
      return mRecord;
    }

    // Conversion ops

    operator Val*() const {
      return mRecord;
    }

    operator const Val*() const {
      return mRecord;
    }
  };

  ///
  /// GC-safe wrapper cell for a Val allocated on the stack.
  /// Do NOT store a Local in the heap or return it as a value; it will explode!
  ///
  /// The engine keeps a second stack with the actual Val cells,
  /// so while we're in the scope they stay alive during GC. When we go out
  /// of scope we pop back off the stack in correct order.
  ///
  class Local : public SmartVal {
  public:

    ///
    /// The constructor pushes a value onto the engine-managed stack.
    /// It will be cleaned up when the current Scope goes out of ... scope.
    ///
    Local(Val aVal)
    : SmartVal(engine().pushLocal(aVal))
    {
      //
    }

    /// C++ doesn't want to do multiple implicit conversions, so let's add
    /// some explicit constructors.
    Local(bool aVal)      : Local(Val(aVal)) {}
    Local(int32_t aVal)   : Local(Val(aVal)) {}
    Local(double aVal)    : Local(Val(aVal)) {}
    Local(Undefined aVal) : Local(Val(aVal)) {}
    Local(Null aVal)      : Local(Val(aVal)) {}
    Local(GCThing* aVal)  : Local(Val(aVal)) {}

    Local(const Local& aLocal)
    : Local(aLocal.mRecord)
    {
      // override the copy constructor
      // to make sure we push as expected
    }

    Local(Local&& aMoved)
    : Local(aMoved.mRecord)
    {
      // overide move constructor
    }

    Local()
    : Local(Undefined())
    {
      //
    }
  };

  template <class T>
  class Retained {
    Local mLocal;

  public:

    template <typename... Args>
    Retained(Args&&... aArgs)
    : mLocal(new T(std::forward<Args>(aArgs)...))
    {
      // Initialize a shiny new object
    }

    T* asPointer() const {
      if (mLocal->isGCThing()) {
        // fixme this should be static_cast but it won't let me
        // since it doesn't think they're related by inheritence
        return reinterpret_cast<T*>(&(mLocal->asGCThing()));
      } else {
        // todo: handle
        std::abort();
      }
    }

    // Deref ops

    T& operator*() const {
      return *asPointer();
    }

    T* operator->() const {
      return asPointer();
    }

    // Conversion ops

    operator T*() const {
      return asPointer();
    }

    operator const T*() const {
      return asPointer();
    }
  };

  ///
  /// Return values need special handling to allocate them in the parent scope,
  /// or everything gets very confusing.
  ///
  /// Never allocate a RetVal yourself or you may be surprised.
  /// Never, ever allocate one on the heap.
  ///
  /// Any function that returns a RetVal must allocate a Scope at its start,
  /// which allocates space before you allocate your locals. Instead of manually
  /// returning, call `return scope.escape(val);`... Then it'll get
  /// cleaned up in the parent function by the parent Scope's destructor.
  ///
  class RetVal : public SmartVal {
    // Let Scope create new RetVals.
    friend class Scope;
    friend class ScopeRetVal;

    template <class T> friend class Ret;

    // The constructors *should not* push on the stack.
    // Instead, take the current scope's space.
    RetVal(Val* aRecord)
    : SmartVal(aRecord)
    {
      //
    }
  };

  ///
  /// Type-specific smart pointer wrapper class for GC'd return values
  ///
  /// See notes for RetVal.
  ///
  template <class T>
  class Ret {
    RetVal mRetVal;

    // fails on GCC 8.1: error: default template arguments may not be used in template friend declarations
    //template <class T2=T> friend class ScopeRet;
    template <class T2> friend class ScopeRet;

    Ret(Val* aRecord)
    : mRetVal(aRecord)
    {
      // Initialize with a pointer
    }

    T* asPointer() const {
      if (mRetVal->isGCThing()) {
        // fixme this should be static_cast but it won't let me
        // since it doesn't think they're related by inheritence
        return reinterpret_cast<T*>(&(mRetVal->asGCThing()));
      } else {
        // todo: handle
        std::abort();
      }
    }

  public:

    // Deref ops

    T& operator*() const {
      return *asPointer();
    }

    T* operator->() const {
      return asPointer();
    }

    // Conversion ops

    operator T*() const {
      return asPointer();
    }

    operator const T*() const {
      return asPointer();
    }

    operator Val() const {
      return *mRetVal;
    }
  };

  ///
  /// Any function that does not return a JS value should declare a
  /// Scope instance before allocating any local variables of the
  /// Local or Retained<T> types.
  ///
  /// When the Scope goes out of scope (usually at end of function),
  /// it will remove all later-allocated Local variables from the
  /// stack, allowing object references to be garbage collected.
  ///
  /// If your function returns a value, use ScopeRetVal or
  /// ScopeRet<T> instead.
  ///
  class Scope {
    Val* mFirstRecord;

  public:
    Scope()
    : mFirstRecord(engine().stackTop())
    {
      //
    }

    ~Scope() {
      // Return the stack to its initial state.
      engine().popLocal(mFirstRecord);
    }
  };

  ///
  /// Functions returning a JS value that could be a garbage-collected
  /// reference should return the RetVal or Ret<T> types, and allocate
  /// a ScopeRetVal immediately at the start before allocating any
  /// Local or Retained<T> variables.
  ///
  /// This allocates space on the _parent_ Scope for a return value,
  /// which should be intermediated through the escape() function.
  ///
  class ScopeRetVal {
    Val* mRecord;
    Scope mScope;

  public:
    ScopeRetVal()
    : mRecord(engine().pushLocal(Undefined())),
      mScope()
    {
      // We saved space for a return value on the parent scope...
    }

    RetVal escape(Val aVal) {
      *mRecord = aVal;
      return RetVal(mRecord);
    }
  };

  ///
  template <class T>
  class ScopeRet {
    Val* mRecord;
    Scope mScope;

  public:
    ScopeRet()
    : mRecord(engine().pushLocal(Undefined())),
      mScope()
    {
      // We saved space for a return value on the parent scope...
    }

    Ret<T> escape(const T* aVal) {
      *mRecord = aVal;
      return Ret<T>(mRecord);
    }
  };

  class ArgList {
    Val* mStackTop;
    Val* mBegin;
    size_t mSize;

    friend class Function;

    // Never create an ArgList yourselve, use an initializer list (RawArgList)
    ArgList(Function& func, RawArgList args);

  public:

    ~ArgList() {
      // Like a Scope, we pop back to the beginning.
      engine().popLocal(mStackTop);
    }

    ///
    /// Return the original argument list length
    ///
    size_t size() const {
      return mSize;
    }

    ///
    /// Return a Binding for the given argument index.
    /// Valid up to the expected arity of the function.
    ///
    Binding operator[](size_t index) const {
      return &mBegin[index];
    }
  };

  class PropIndex : public JSThing {
  public:
    PropIndex()
    : JSThing() {}

    ~PropIndex() override;
  };

  class String : public PropIndex {
    string data;

  public:
    String(string const &aStr)
    : PropIndex(),
      data(aStr)
    {
      //
    }

    ~String() override;

    TypeOf typeOf() const override;

    string dump() override;

    size_t length() const {
      return data.size();
    }

    operator string() const {
      return data;
    }

    Ret<String> toString() const override {
      ScopeRet<String> scope;
      return scope.escape(this);
    }

    bool operator==(const String &rhs) const {
      return data == rhs.data;
    }

    Ret<String> operator+(const String &rhs) const {
      ScopeRet<String> scope;
      return scope.escape(new String(data + rhs.data));
    }
  };

  class Symbol : public PropIndex {
    string name;

  public:
    Symbol(string const &aName)
    : PropIndex(),
      name(aName)
    {
      //
    }

    ~Symbol() override;

    TypeOf typeOf() const override;

    string dump() override;

    Ret<String> toString() const override {
      ScopeRet<String> scope;
      return scope.escape(new String("Symbol(" + getName() + ")"));
    }

    const string &getName() const {
      return name;
    }
  };

  ///
  /// Represents a regular JavaScript object, with properties and
  /// a prototype chain. String and Symbol are subclasses.
  ///
  /// Todo: throw exceptions on bad prop lookups
  /// Todo: implement getters, setters, enumeration, etc
  ///
  class Object : public JSThing {
    Object *mPrototype;
    unordered_map<Val,Val> mProps;

  public:
    Object()
    : JSThing(),
      mPrototype(nullptr)
    {
      //
    }

    Object(Object& aPrototype)
    : JSThing(),
      mPrototype(&aPrototype)
    {
      // Note this doesn't call the constructor,
      // which would be done by outside code.
    }

    ~Object() override;

    void markRefsForGC() override;
    string dump() override;
    TypeOf typeOf() const override;

    Ret<String> toString() const override {
      ScopeRet<String> scope;
      // todo get the constructor name
      return scope.escape(new String("[object Object]"));
    }

    RetVal getProp(Val name);
    void setProp(Val name, Val val);
  };

  ///
  /// Represents a closure-captured JS variable, allocated on the heap in
  /// this wrapper object.
  ///
  class Cell : public Internal {
    Val mVal;

  public:
    Cell()
    : mVal(Undefined())
    {
      //
    }

    Cell(Val aVal)
    : mVal(aVal)
    {
      //
    }

    ~Cell() override;

    Binding binding() {
      return &mVal;
    }

    Val& val() {
      return mVal;
    }

    void markRefsForGC() override;
    string dump() override;
  };

  ///
  /// Represents a runtime function object.
  /// References the
  ///
  class Function : public Object {
    std::string mName;
    size_t mArity;
    std::vector<Cell*> mCaptures;
    FunctionBody mBody;

  public:
    // For function with no captures
    Function(
      std::string aName,
      size_t aArity,
      FunctionBody aBody)
    : Object(), // todo: have a function prototype object!
      mName(aName),
      mArity(aArity),
      mCaptures(),
      mBody(aBody)
    {
      //
    }

    // For function with captures
    Function(
      std::string aName,
      size_t aArity,
      RawCaptureList aCaptures,
      FunctionBody aBody)
    : Object(), // todo: have a function prototype object!
      mName(aName),
      mArity(aArity),
      mCaptures(aCaptures),
      mBody(aBody)
    {
      //
    }

    ~Function() override;

    TypeOf typeOf() const override;

    std::string name() const {
      return mName;
    }

    ///
    /// Number of defined arguments
    ///
    size_t arity() const {
      return mArity;
    }

    RetVal call(Local aThis, RawArgList aArgs);

    ///
    /// Return one of the captured variable pointers.
    ///
    Cell& capture(size_t aIndex) {
      return *mCaptures[aIndex];
    }

    void markRefsForGC() override;
    string dump() override;

    Ret<String> toString() const override {
      ScopeRet<String> scope;
      return scope.escape(new String("[Function: " + name() + "]"));
    }

  };

}

#endif
