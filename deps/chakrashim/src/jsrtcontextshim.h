// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <vector>

namespace jsrt {
class ContextShim {
 public:
  // This has the same layout as v8::Context::Scope
  class Scope {
   private:
    Scope * previous;
    ContextShim * contextShim;
   public:
    explicit Scope(ContextShim * context);
    ~Scope();

    friend class IsolateShim;
  };

  // These global constructors will be cached
  enum GlobalType {
#define DEFTYPE(T) T,
#include "jsrtcontextcachedobj.inc"
    _TypeCount
  };

  // These prototype functions will be cached/shimmed
  enum GlobalPrototypeFunction {
#define DEFMETHOD(T, M)  T##_##M,
#include "jsrtcontextcachedobj.inc"
    _FunctionCount
  };

  static ContextShim * New(IsolateShim * isolateShim, bool exposeGC,
                           JsValueRef globalObjectTemplateInstance);
  ~ContextShim();
  void EnsureInitialized();

  IsolateShim * GetIsolateShim();
  JsContextRef GetContextRef();

  JsValueRef GetTrue();
  JsValueRef GetFalse();
  JsValueRef GetUndefined();
  JsValueRef GetNull();
  JsValueRef GetZero();
  JsValueRef GetObjectConstructor();
  JsValueRef GetBooleanObjectConstructor();
  JsValueRef GetNumberObjectConstructor();
  JsValueRef GetStringObjectConstructor();
  JsValueRef GetDateConstructor();
  JsValueRef GetRegExpConstructor();
  JsValueRef GetProxyConstructor();
  JsValueRef GetGlobalType(GlobalType index);
  JsValueRef GetGetOwnPropertyDescriptorFunction();
  JsValueRef GetStringConcatFunction();
  JsValueRef GetArrayPushFunction();
  JsValueRef GetGlobalPrototypeFunction(GlobalPrototypeFunction index);
  JsValueRef GetProxyOfGlobal();

  void * GetAlignedPointerFromEmbedderData(int index);
  void SetAlignedPointerInEmbedderData(int index, void * value);
  void RunMicrotasks();

  static ContextShim * GetCurrent();

 private:
  ContextShim(IsolateShim * isolateShim, JsContextRef context, bool exposeGC,
              JsValueRef globalObjectTemplateInstance);
  bool DoInitializeContextShim();
  bool InitializeBuiltIns();
  bool InitializeProxyOfGlobal();
  bool InitializeGlobalPrototypeFunctions();
  bool InitializeObjectPrototypeToStringShim();

  template <typename Fn>
  bool InitializeBuiltIn(JsValueRef * builtInValue, Fn getBuiltIn);
  bool InitializeBuiltIn(JsValueRef * builtInValue, const wchar_t* globalName);
  bool InitializeGlobalTypes();
  bool KeepAlive(JsValueRef value);
  JsValueRef GetCachedShimFunction(CachedPropertyIdRef id, JsValueRef* func);
  bool ExposeGc();
  bool CheckConfigGlobalObjectTemplate();
  bool ExecuteChakraShimJS();

  IsolateShim * isolateShim;
  JsContextRef context;
  bool initialized;
  bool exposeGC;
  JsValueRef keepAliveObject;
  int builtInCount;
  JsValueRef globalObjectTemplateInstance;

  JsValueRef trueRef;
  JsValueRef falseRef;
  JsValueRef undefinedRef;
  JsValueRef nullRef;
  JsValueRef zero;
  JsValueRef globalConstructor[GlobalType::_TypeCount];
  JsValueRef globalObject;
  JsValueRef proxyOfGlobal;

  JsValueRef globalPrototypeFunction[GlobalPrototypeFunction::_FunctionCount];
  JsValueRef getOwnPropertyDescriptorFunction;

  JsValueRef promiseContinuationFunction;
  std::vector<void*> embedderData;

#define DECLARE_CHAKRASHIM_FUNCTION_GETTER(F) \
public: \
  JsValueRef Get##F##Function(); \
private: \
  JsValueRef F##Function; \

  DECLARE_CHAKRASHIM_FUNCTION_GETTER(cloneObject);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getPropertyNames);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getEnumerableNamedProperties);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getEnumerableIndexedProperties);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(createEnumerationIterator);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER
  (createPropertyDescriptorsEnumerationIterator);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getNamedOwnKeys);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getIndexedOwnKeys);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getStackTrace);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getSymbolKeyFor);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getSymbolFor);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(ensureDebug);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(enqueueMicrotask);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(dequeueMicrotask);
  DECLARE_CHAKRASHIM_FUNCTION_GETTER(getPropertyAttributes);

#define DEF_IS_TYPE(F) DECLARE_CHAKRASHIM_FUNCTION_GETTER(F)
#include "jsrtcachedpropertyidref.inc"
#undef DEF_IS_TYPE
};

}  // namespace jsrt
