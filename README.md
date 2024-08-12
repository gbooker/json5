# `json5`
**`json5`** is a small header only C++ library for parsing [JSON](https://en.wikipedia.org/wiki/JSON) or [**JSON5**](https://json5.org/) data. It also comes with a simple reflection system for easy serialization and deserialization of C++ structs.

### Quick Example:
```cpp
#include <json5/json5_input.hpp>
#include <json5/json5_output.hpp>

struct Settings
{
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  bool fullscreen = false;
  std::string renderer = "";

  JSON5_MEMBERS(x, y, width, height, fullscreen, renderer)
};

Settings s;

// Fill 's' instance from file
json5::FromFile("settings.json", s);

// Save 's' to file
json5::ToFile("settings.json", s);
```

## `json5.hpp`
Provides the basic types of `json5::Document`, `json5::Value`, and `json5::IndependentValue`.  A `Value` represents a value within JSON but it relies on a `Document` for storage of non-primitive (string, array, object) data.  A `Value` may be more difficult to manipulate which is why the `IndependentValue` is provided.  An `IndependentValue` is self-contained and indepenedent of any other object.  It contains a variant which respresents all possible JSON value types.  Parsing is faster with a `Document` than with an `IndependentValue` due to its compact nature and small number of memory allocations.

## `json5_input.hpp`
Provides functions to load `json5::Document` or `json5::IndependentValue` from string, stream or file.

## `json5_output.hpp`
Provides functions to convert `json5::Document` or `json5::IndependentValue` into string, stream or file.

## `json5_builder.hpp`
Defines `Builder`s for building `Document`s and `IndependentValue`s.  Also provides the basis for building arbitrary objects via reflection.

## `json5_reflect.hpp`
Provides functionality to read/write structs/classes from/to a string, stream, or file

### Basic supported types:
- `bool`
- `int`, `float`, `double`, and other numeric types
- `std::string`
- `std::vector`, `std::array`, `std::map`, `std::unordered_map`, `std::array`
- `C array`

### Reading from JSON
Reading is accomplished via the templated class `json5::detail::Reflector`.  A custom deserializer can be made via a template specialiaztion (or partial specialization) for the class type.  Often one would want to inherit `json5::detail::RefReflector` which holds a reference to the target object.  Then override the relevant functions in `json5::detail::BaseReflector`.  This is works because `Parser` will parse the JSON, calling functions on `ReflectionBuilder` which uses a stack of `BaseReflector` objects as it parses depth first into an object.
Writing is accomplished via the templated struct `json5::detail::ReflectionWriter`.  A custom serialization should create a template specialization (or partial specialiaztion) for this struct and implement `static inline void Write(Writer& w, const T& in)` with the proper value for `T` (in need not be a const ref but could instead take by value like is done when serializing numbers but non-primitives likely should be const ref).  This was changed from a templated function becuase partial specializations are not possible with functions but are possible with structs.

## `json5_base.hpp`
Contains `Error` definitions, `ValueType` enum, and macro definitions.

## `json5_filter.hpp`

# FAQ
TBD

# Additional examples

### Serialize custom type:
```cpp
// Let's have a 3D vector struct:
struct Vec3
{
  float x, y, z;
};

// Let's have a triangle struct with 'vec3' members
struct Triangle
{
  vec3 a, b, c;
};

JSON5_CLASS(Triangle, a, b, c)

namespace json5::detail {
  // Write Vec3 to JSON array
  template <>
  class ReflectionWriter<Vec3> : public TupleReflectionWriter<float, float, float>
  {
  public:
    static inline void Write(Writer& w, const Vec3& in) { TupleReflectionWriter::Write(w, in.x, in.y, in.z); }
  };

  // Read Vec3 from JSON array
  template <>
  class Reflector<Vec3> : public TupleReflector<float, float, float>
  {
  public:
    explicit Reflector(Vec3& vec)
      : TupleReflector(vec.x, vec.y, vec.z)
    {}
  };

  // Write Triangle as JSON array of 3 numbers
  template <>
  class ReflectionWriter<Triangle> : public TupleReflectionWriter<Vec3, Vec3, Vec3>
  {
  public:
    static inline void Write(Writer& w, const Triangle& in) { TupleReflectionWriter::Write(w, in.a, in.b, in.c); }
  };

  // Read Triangle from JSON array
  template <>
  class Reflector<Triangle> : public TupleReflector<Vec3, Vec3, Vec3>
  {
  public:
    explicit Reflector(Triangle& tri)
      : TupleReflector(tri.a, tri.b, tri.c)
    {}
  };
} // namespace json5::detail
```

### Serialize enum:
```cpp
enum class MyEnum
{
  Zero,
  First,
  Second,
  Third
};

// (must be placed in global namespce, requires C++20)
JSON5_ENUM(MyEnum, Zero, First, Second, Third)
```
