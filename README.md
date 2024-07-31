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
  // Write Vec3 as JSON array of 3 numbers
  inline void Write(writer &w, const Vec3 &in)
  {
    Write(w, std::array<float, 3>{in.x, in.y, in.z});
  }

  // Read Vec3 from JSON array
  template <>
  class json5::detail::Reflector<Vec3> : public RefReflector<Vec3>
  {
  public:
    using RefReflector<Vec3>::RefReflector;
    using RefReflector<Vec3>::m_obj;

    Error::Type getNonTypeError() override { return Error::ArrayExpected; }
    bool allowArray() override { return true; }
    std::unique_ptr<BaseReflector> getReflectorInArray() override
    {
      m_index++;
      switch (m_index)
      {
      case 1:
        return std::make_unique<Reflector<float>>(m_obj.x);
      case 2:
        return std::make_unique<Reflector<float>>(m_obj.y);
      case 3:
        return std::make_unique<Reflector<float>>(m_obj.z);
      default:
        break;
      }

      return std::make_unique<IgnoreReflector>();
    }

    Error::Type complete() override
    {
      if (m_index != 3)
        return Error::WrongArraySize;

      return Error::None;
    }

  protected:
    size_t m_index = 0;
  };

  // Write Triangle as JSON array of 3 numbers
  inline void Write(writer &w, const Triangle &in)
  {
    Write(w, std::array<float, 3>{in.x, in.y, in.z});
  }

  // Read Triangle from JSON array
  template <>
  class json5::detail::Reflector<Triangle> : public RefReflector<Triangle>
  {
  public:
    using RefReflector<Triangle>::RefReflector;
    using RefReflector<Triangle>::m_obj;

    Error::Type getNonTypeError() override { return Error::ArrayExpected; }
    bool allowArray() override { return true; }
    std::unique_ptr<BaseReflector> getReflectorInArray() override
    {
      m_index++;
      switch (m_index)
      {
      case 1:
        return std::make_unique<Reflector<Vec3>>(m_obj.a);
      case 2:
        return std::make_unique<Reflector<Vec3>>(m_obj.b);
      case 3:
        return std::make_unique<Reflector<Vec3>>(m_obj.c);
      default:
        break;
      }

      return std::make_unique<IgnoreReflector>();
    }

    Error::Type complete() override
    {
      if (m_index != 3)
        return Error::WrongArraySize;

      return Error::None;
    }

  protected:
    size_t m_index = 0;
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
