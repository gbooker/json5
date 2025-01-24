#pragma once

#include <tuple>

#define JSON5_CLASS_BASE(_Name, _Base, ...)                             \
  template <>                                                           \
  struct json5::detail::Json5Access<_Name>                              \
  {                                                                     \
    using SuperCls [[maybe_unused]] = _Base;                            \
    constexpr static auto GetNames() noexcept                           \
    {                                                                   \
      return #__VA_ARGS__;                                              \
    }                                                                   \
    constexpr static auto GetTie(_Name& out) noexcept                   \
    {                                                                   \
      return std::tie(_JSON5_CONCAT(_JSON5_PREFIX_OUT, (__VA_ARGS__))); \
    }                                                                   \
    constexpr static auto GetTie(const _Name& in) noexcept              \
    {                                                                   \
      return std::tie(_JSON5_CONCAT(_JSON5_PREFIX_IN, (__VA_ARGS__)));  \
    }                                                                   \
  };

/*
  Generates class serialization helper for specified type:

  namespace foo {
    struct Bar { int x; float y; bool z; };
  }

  JSON5_CLASS(foo::Bar, x, y, z)
*/
#define JSON5_CLASS(_Name, ...) \
  JSON5_CLASS_BASE(_Name, std::false_type, __VA_ARGS__)

/*
  Generates class serialization helper for specified type with inheritance:

  namespace foo {
    struct Base { std::string name; };
    struct Bar : Base { int x; float y; bool z; };
  }

  JSON5_CLASS(foo::Base, name)
  JSON5_CLASS_INHERIT(foo::Bar, foo::Base, x, y, z)
*/
#define JSON5_CLASS_INHERIT(_Name, _Base, ...) \
  JSON5_CLASS_BASE(_Name, _Base, __VA_ARGS__)

/////////////////////////////////////////////////////////////

#define JSON5_MEMBERS_BASE(...)                              \
  static constexpr std::string_view GetJson5Names() noexcept \
  {                                                          \
    return #__VA_ARGS__;                                     \
  }                                                          \
  constexpr auto getJson5Tie() noexcept                      \
  {                                                          \
    return std::tie(__VA_ARGS__);                            \
  }                                                          \
  constexpr auto getJson5Tie() const noexcept                \
  {                                                          \
    return std::tie(__VA_ARGS__);                            \
  }

/*
  Generates members serialization helper inside class:

  namespace foo {
    struct Bar {
      int x; float y; bool z;
      JSON5_MEMBERS(x, y, z)
    };
  }
*/
#define JSON5_MEMBERS(...)        \
  JSON5_MEMBERS_BASE(__VA_ARGS__) \
  using Json5SuperClass [[maybe_unused]] = std::false_type;

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
#define JSON5_MEMBERS_INHERIT(_Base, ...) \
  JSON5_MEMBERS_BASE(__VA_ARGS__)         \
  using Json5SuperClass [[maybe_unused]] = _Base;

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
  class DocumentBuilder;
  class Parser;
  class Value;

  //---------------------------------------------------------------------------------------------------------------------
  struct Error final
  {
    enum Type
    {
      None,             // no error
      InvalidRoot,      // document root is not an object or array
      UnexpectedEnd,    // unexpected end of JSON data (end of stream, string or file)
      SyntaxError,      // general parsing error
      InvalidLiteral,   // invalid literal, only "true", "false", "null" allowed
      InvalidEscapeSeq, // invalid or unsupported string escape \ sequence
      CommaExpected,    // expected comma ','
      ColonExpected,    // expected color ':'
      NullExpected,     // expected literal "null"
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
        "null expected",
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
  struct Json5Access
  {
    using SuperCls = typename T::Json5SuperClass;
    constexpr static auto GetNames() noexcept { return T::GetJson5Names(); }
    constexpr static auto GetTie(T& out) noexcept { return out.getJson5Tie(); }
    constexpr static auto GetTie(const T& in) noexcept { return in.getJson5Tie(); }
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
