#pragma once

#include <tuple>

/*
  Generates class serialization helper for specified type:

  namespace foo {
    struct Bar { int x; float y; bool z; };
  }

  JSON5_CLASS(foo::Bar, x, y, z)
*/
#define JSON5_CLASS(_Name, ...)                                                            \
  template <>                                                                              \
  struct json5::detail::ClassWrapper<_Name>                                                \
  {                                                                                        \
    static constexpr const char* names = #__VA_ARGS__;                                     \
    inline static auto MakeNamedTuple(_Name& out) noexcept                                 \
    {                                                                                      \
      return std::tuple(names, std::tie(_JSON5_CONCAT(_JSON5_PREFIX_OUT, (__VA_ARGS__)))); \
    }                                                                                      \
    inline static auto MakeNamedTuple(const _Name& in) noexcept                            \
    {                                                                                      \
      return std::tuple(names, std::tie(_JSON5_CONCAT(_JSON5_PREFIX_IN, (__VA_ARGS__))));  \
    }                                                                                      \
  };

/*
  Generates class serialization helper for specified type with inheritance:

  namespace foo {
    struct Base { std::string name; };
    struct Bar : Base { int x; float y; bool z; };
  }

  JSON5_CLASS(foo::Base, name)
  JSON5_CLASS_INHERIT(foo::Bar, foo::Base, x, y, z)
*/
#define JSON5_CLASS_INHERIT(_Name, _Base, ...)                                         \
  template <>                                                                          \
  struct json5::detail::ClassWrapper<_Name>                                            \
  {                                                                                    \
    static constexpr const char* names = #__VA_ARGS__;                                 \
    inline static auto MakeNamedTuple(_Name& out) noexcept                             \
    {                                                                                  \
      return std::tuple_cat(                                                           \
        json5::detail::ClassWrapper<_Base>::MakeNamedTuple(out),                       \
        std::tuple(names, std::tie(_JSON5_CONCAT(_JSON5_PREFIX_OUT, (__VA_ARGS__))))); \
    }                                                                                  \
    inline static auto MakeNamedTuple(const _Name& in) noexcept                        \
    {                                                                                  \
      return std::tuple_cat(                                                           \
        json5::detail::ClassWrapper<_Base>::MakeNamedTuple(in),                        \
        std::tuple(names, std::tie(_JSON5_CONCAT(_JSON5_PREFIX_IN, (__VA_ARGS__)))));  \
    }                                                                                  \
  };

/*
  Generates members serialization helper inside class:

  namespace foo {
    struct Bar {
      int x; float y; bool z;
      JSON5_MEMBERS(x, y, z)
    };
  }
*/
#define JSON5_MEMBERS(...)                                               \
  inline auto makeNamedTuple() noexcept                                  \
  {                                                                      \
    return std::tuple((const char*)#__VA_ARGS__, std::tie(__VA_ARGS__)); \
  }                                                                      \
  inline auto makeNamedTuple() const noexcept                            \
  {                                                                      \
    return std::tuple((const char*)#__VA_ARGS__, std::tie(__VA_ARGS__)); \
  }

/*
  Generates members serialzation helper inside class with inheritance:

  namespace foo {
    struct Base {
      std::string name;
      JSON5_MEMBERS(name)
    };

    struct Bar : Base {
      int x; float y; bool z;
      JSON5_MEMBERS_INHERIT(Base, x, y, z)
    };
  }
*/
#define JSON5_MEMBERS_INHERIT(_Base, ...)                            \
  inline auto makeNamedTuple() noexcept                              \
  {                                                                  \
    return std::tuple_cat(                                           \
      json5::detail::ClassWrapper<_Base>::MakeNamedTuple(*this),     \
      std::tuple((const char*)#__VA_ARGS__, std::tie(__VA_ARGS__))); \
  }                                                                  \
  inline auto makeNamedTuple() const noexcept                        \
  {                                                                  \
    return std::tuple_cat(                                           \
      json5::detail::ClassWrapper<_Base>::MakeNamedTuple(*this),     \
      std::tuple((const char*)#__VA_ARGS__, std::tie(__VA_ARGS__))); \
  }

/*
  Generates enum wrapper:

  enum class MyEnum {
    One, Two, Three
  };

  JSON5_ENUM(MyEnum, One, Two, Three)
*/
#define JSON5_ENUM(_Name, ...)                             \
  template <>                                              \
  struct json5::detail::EnumTable<_Name> : std::true_type  \
  {                                                        \
    using enum _Name;                                      \
    static constexpr const char* Names = #__VA_ARGS__;     \
    static constexpr const _Name Values[] = {__VA_ARGS__}; \
  };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace json5
{

  /* Forward declarations */
  class Builder;
  class Document;
  class Parser;
  class Value;

  //---------------------------------------------------------------------------------------------------------------------
  struct Error final
  {
    enum
    {
      None,             // no error
      InvalidRoot,      // document root is not an object or array
      UnexpectedEnd,    // unexpected end of JSON data (end of stream, string or file)
      SyntaxError,      // general parsing error
      InvalidLiteral,   // invalid literal, only "true", "false", "null" allowed
      InvalidEscapeSeq, // invalid or unsupported string escape \ sequence
      CommaExpected,    // expected comma ','
      ColonExpected,    // expected color ':'
      BooleanExpected,  // expected boolean literal "true" or "false"
      NumberExpected,   // expected number
      StringExpected,   // expected string "..."
      ObjectExpected,   // expected object { ... }
      ArrayExpected,    // expected array [ ... ]
      WrongArraySize,   // invalid number of array elements
      InvalidEnum,      // invalid enum value or string (conversion failed)
      CouldNotOpen,     // stream is not open
    };

    static constexpr const char* TypeString[] =
      {
        "none",
        "invalid root",
        "unexpected end",
        "syntax error",
        "invalid literal",
        "invalid escape sequence",
        "comma expected",
        "colon expected",
        "boolean expected",
        "number expected",
        "string expected",
        "object expected",
        "array expected",
        "wrong array size",
        "invalid enum",
        "could not open stream",
    };

    int type = None;
    int line = 0;
    int column = 0;

    operator int() const noexcept { return type; } // NOLINT(google-explicit-constructor)
  };

  //---------------------------------------------------------------------------------------------------------------------
  struct WriterParams
  {
    // One level of indentation
    const char* indentation = "  ";

    // End of line string
    const char* eol = "\n";

    // Write all on single line, omit extra spaces
    bool compact = false;

    // Write regular JSON (don't use any JSON5 features)
    bool jsonCompatible = false;

    // Escape unicode characters in strings
    bool escapeUnicode = false;

    // Custom user data pointer
    void* userData = nullptr;
  };

  //---------------------------------------------------------------------------------------------------------------------
  enum class ValueType
  {
    Null = 0,
    Boolean,
    Number,
    Array,
    String,
    Object
  };

} // namespace json5

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace json5::detail
{

  using StringOffset = unsigned;

  template <typename T>
  struct ClassWrapper
  {
    inline static auto MakeNamedTuple(T& in) noexcept { return in.makeNamedTuple(); }
    inline static auto MakeNamedTuple(const T& in) noexcept { return in.makeNamedTuple(); }
  };

  template <typename T>
  struct EnumTable : std::false_type
  {
  };

  class CharSource
  {
  public:
    virtual ~CharSource() = default;

    virtual int next() = 0;
    virtual int peek() = 0;
    virtual bool eof() const = 0;

    Error makeError(int type) const noexcept { return Error {type, m_line, m_column}; }

  protected:
    int m_line = 1;
    int m_column = 1;
  };

} // namespace json5::detail

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Here be dragons... */

#define _JSON5_EXPAND(...) __VA_ARGS__
#define _JSON5_JOIN(X, Y) _JSON5_JOIN2(X, Y)
#define _JSON5_JOIN2(X, Y) X##Y
#define _JSON5_COUNT(...) _JSON5_EXPAND(_JSON5_COUNT2(__VA_ARGS__, \
  16, 15, 14, 13, 12, 11, 10, 9,                                   \
  8, 7, 6, 5, 4, 3, 2, 1, ))

#define _JSON5_COUNT2(_,                 \
  _16, _15, _14, _13, _12, _11, _10, _9, \
  _8, _7, _6, _5, _4, _3, _2, _X, ...) _X

#define _JSON5_FIRST(...) _JSON5_EXPAND(_JSON5_FIRST2(__VA_ARGS__, ))
#define _JSON5_FIRST2(X, ...) X
#define _JSON5_TAIL(...) _JSON5_EXPAND(_JSON5_TAIL2(__VA_ARGS__))
#define _JSON5_TAIL2(X, ...) (__VA_ARGS__)

#define _JSON5_CONCAT(_Prefix, _Args) _JSON5_JOIN(_JSON5_CONCAT_, _JSON5_COUNT _Args)(_Prefix, _Args)
#define _JSON5_CONCAT_1(_Prefix, _Args) _Prefix _Args
#define _JSON5_CONCAT_2(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_1(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_3(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_2(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_4(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_3(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_5(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_4(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_6(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_5(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_7(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_6(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_8(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_7(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_9(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_8(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_10(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_9(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_11(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_10(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_12(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_11(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_13(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_12(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_14(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_13(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_15(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_14(_Prefix, _JSON5_TAIL _Args)
#define _JSON5_CONCAT_16(_Prefix, _Args) _Prefix(_JSON5_FIRST _Args), _JSON5_CONCAT_15(_Prefix, _JSON5_TAIL _Args)

#define _JSON5_PREFIX_IN(_X) in._X
#define _JSON5_PREFIX_OUT(_X) out._X
