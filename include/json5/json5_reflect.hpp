#pragma once

#include <array>
#include <fstream>
#include <map>
#include <unordered_map>

#include "json5_builder.hpp"
#include "json5_output.hpp"

namespace json5
{

  //
  template <typename T>
  void ToDocument(Document& doc, const T& in);

  //
  template <typename T>
  void ToStream(std::ostream& os, const T& in, const WriterParams& wp = WriterParams());

  //
  template <typename T>
  void ToString(std::string& str, const T& in, const WriterParams& wp = WriterParams());

  //
  template <typename T>
  std::string ToString(const T& in, const WriterParams& wp = WriterParams());

  //
  template <typename T>
  bool ToFile(std::string_view fileName, const T& in, const WriterParams& wp = WriterParams());

  //
  template <typename T>
  Error FromDocument(const Document& doc, T& out);

  //
  template <typename T>
  Error FromString(std::string_view str, T& out);

  //
  template <typename T>
  Error FromFile(std::string_view fileName, T& out);

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  namespace detail
  {

    template <typename T>
    struct is_optional : public std::false_type // NOLINT(readability-identifier-naming)
    {
    };

    template <typename T>
    struct is_optional<std::optional<T>> : public std::true_type
    {
    };

    template <typename T>
    constexpr static bool is_optional_v = is_optional<T>::value; // NOLINT(readability-identifier-naming)

    //---------------------------------------------------------------------------------------------------------------------
    inline std::string_view GetNameSlice(const char* names, size_t index)
    {
      size_t numCommas = index;
      while (numCommas > 0 && *names)
        if (*names++ == ',')
          --numCommas;

      while (*names && *names <= 32)
        ++names;

      size_t length = 0;
      while (names[length] > 32 && names[length] != ',')
        ++length;

      return std::string_view(names, length);
    }

    /* Forward declarations */
    template <typename T>
    void Write(Writer& w, const T& in);
    template <typename T>
    void WriteArray(Writer& w, const T* in, size_t numItems);
    template <typename T>
    void WriteMap(Writer& w, const T& in);
    template <typename T>
    void WriteEnum(Writer& w, T in);
    void WriteIndependentValue(Writer& w, const IndependentValue& in);

    //---------------------------------------------------------------------------------------------------------------------
    inline void Write(Writer& w, bool in) { w.writeBoolean(in); }
    inline void Write(Writer& w, int in) { w.writeNumber(double(in)); }
    inline void Write(Writer& w, unsigned in) { w.writeNumber(double(in)); }
    inline void Write(Writer& w, float in) { w.writeNumber(double(in)); }
    inline void Write(Writer& w, double in) { w.writeNumber(in); }

    //---------------------------------------------------------------------------------------------------------------------
    inline void Write(Writer& w, const char* in) { w.writeString(in); }
    inline void Write(Writer& w, const std::string& in) { w.writeString(in); }

    //---------------------------------------------------------------------------------------------------------------------
    inline void Write(Writer& w, const IndependentValue& in)
    {
      WriteIndependentValue(w, in);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, typename A>
    inline void Write(Writer& w, const std::vector<T, A>& in)
    {
      return WriteArray(w, in.data(), in.size());
    }

    template <typename A>
    inline void Write(Writer& w, const std::vector<bool, A>& in)
    {
      w.beginArray();
      for (size_t i = 0; i < in.size(); ++i)
      {
        w.beginArrayElement();
        Write(w, (bool)in[i]);
      }

      w.endArray();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    inline void Write(Writer& w, const T (&in)[N])
    {
      return WriteArray(w, in, N);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    inline void Write(Writer& w, const std::array<T, N>& in)
    {
      return WriteArray(w, in.data(), N);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename K, typename T, typename P, typename A>
    inline void Write(Writer& w, const std::map<K, T, P, A>& in)
    {
      WriteMap(w, in);
    }

    template <typename K, typename T, typename H, typename EQ, typename A>
    inline void Write(Writer& w, const std::unordered_map<K, T, H, EQ, A>& in)
    {
      WriteMap(w, in);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename Type>
    inline void WriteTupleValue(Writer& w, std::string_view name, const Type& in)
    {
      if constexpr (is_optional_v<Type>)
      {
        if (in)
          WriteTupleValue<typename Type::value_type>(w, name, *in);
      }
      else
      {
        w.beginObjectElement();
        w.writeObjectKey(name);

        if constexpr (std::is_enum_v<Type>)
        {
          if constexpr (EnumTable<Type>())
            WriteEnum(w, in);
          else
            Write(w, std::underlying_type_t<Type>(in));
        }
        else
        {
          Write(w, in);
        }
      }
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <size_t Index = 0, typename... Types>
    inline void WriteTuple(Writer& w, const char* names, const std::tuple<Types...>& t)
    {
      const auto& in = std::get<Index>(t);
      using Type = std::remove_const_t<std::remove_reference_t<decltype(in)>>;

      if (auto name = GetNameSlice(names, Index); !name.empty())
        WriteTupleValue<Type>(w, name, in);

      if constexpr (Index + 1 != sizeof...(Types))
        WriteTuple<Index + 1>(w, names, t);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <size_t Index = 0, typename... Types>
    inline void WriteNamedTuple(Writer& w, const std::tuple<Types...>& t)
    {
      WriteTuple(w, std::get<Index>(t), std::get<Index + 1>(t));

      if constexpr (Index + 2 != sizeof...(Types))
        WriteNamedTuple<Index + 2>(w, t);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline void Write(Writer& w, const T& in)
    {
      w.beginObject();
      WriteNamedTuple(w, ClassWrapper<T>::MakeNamedTuple(in));
      w.endObject();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline void WriteArray(Writer& w, const T* in, size_t numItems)
    {
      w.beginArray();

      for (size_t i = 0; i < numItems; ++i)
      {
        w.beginArrayElement();
        Write(w, in[i]);
      }

      w.endArray();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline void WriteMap(Writer& w, const T& in)
    {
      w.beginObject();

      for (const auto& kvp : in)
      {
        w.beginObjectElement();
        w.writeObjectKey(kvp.first);
        Write(w, kvp.second);
      }

      w.endObject();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline void WriteEnum(Writer& w, T in)
    {
      size_t index = 0;
      const auto* names = EnumTable<T>::Names;
      const auto* values = EnumTable<T>::Values;

      while (true)
      {
        auto name = GetNameSlice(names, index);

        // Underlying value fallback
        if (name.empty())
        {
          Write(w, std::underlying_type_t<T>(in));
          return;
        }

        if (in == values[index])
        {
          w.writeString(name);
          return;
        }

        ++index;
      }
    }

    //---------------------------------------------------------------------------------------------------------------------
    inline void WriteIndependentValue(Writer& w, const IndependentValue& in)
    {
      std::visit([&w](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          w.writeNull();
        else
          Write(w, value);
      },
        in.value);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class BaseReflector
    {
    public:
      virtual Error::Type getNonTypeError() = 0;
      virtual Error::Type setValue(double number) { return getNonTypeError(); }
      virtual Error::Type setValue(bool boolean) { return getNonTypeError(); }
      virtual Error::Type setValue(std::nullptr_t) { return getNonTypeError(); }

      virtual bool allowString() { return false; }
      virtual void setValue(std::string str) { throw std::logic_error("Attempt to set string on non string value"); }

      virtual bool allowObject() { return false; }
      virtual std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) { throw std::logic_error("Attempt to get object item on non object value"); }
      virtual bool allowArray() { return false; }
      virtual std::unique_ptr<BaseReflector> getReflectorInArray() { throw std::logic_error("Attempt to get array item on non array value"); }

      virtual Error::Type complete() { return Error::None; }
    };

    class IgnoreReflector : public BaseReflector
    {
    public:
      Error::Type getNonTypeError() override { return Error::None; }
      Error::Type setValue(double number) override { return Error::None; }
      Error::Type setValue(bool boolean) override { return Error::None; }
      Error::Type setValue(std::nullptr_t) override { return Error::None; }

      bool allowString() override { return true; }
      void setValue(std::string) override {}

      bool allowObject() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override { return std::make_unique<IgnoreReflector>(); }
      bool allowArray() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorInArray() override { return std::make_unique<IgnoreReflector>(); }
    };

    template <typename T>
    class RefReflector : public BaseReflector
    {
    public:
      explicit RefReflector(T& obj)
        : m_obj(obj)
      {}

    protected:
      T& m_obj;
    };

    template <typename T, class Enabled = void>
    class Reflector : public RefReflector<T>
    {
    public:
      using RefReflector<T>::RefReflector;
      using RefReflector<T>::m_obj;

      Error::Type getNonTypeError() override { return Error::ObjectExpected; }

      bool allowObject() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override
      {
        auto tuple = detail::ClassWrapper<T>::MakeNamedTuple(m_obj);
        return ReadNamedTuple(key, tuple);
      }

      //---------------------------------------------------------------------------------------------------------------------
      template <size_t Index = 0, typename... Types>
      inline std::unique_ptr<BaseReflector> ReadTuple(std::string_view name, const char* names, std::tuple<Types...>& t)
      {
        auto candidate = detail::GetNameSlice(names, Index);
        if (candidate == name)
          return std::make_unique<Reflector<std::remove_reference_t<std::tuple_element_t<Index, std::tuple<Types...>>>>>(std::get<Index>(t));
        else if constexpr (Index + 1 != sizeof...(Types))
          return ReadTuple<Index + 1>(name, names, t);

        return {};
      }

      //---------------------------------------------------------------------------------------------------------------------
      template <size_t Index = 0, typename... Types>
      inline std::unique_ptr<BaseReflector> ReadNamedTuple(std::string_view name, std::tuple<Types...>& t)
      {
        if (std::unique_ptr<BaseReflector> reflector = ReadTuple(name, std::get<Index>(t), std::get<Index + 1>(t)))
          return reflector;

        if constexpr (Index + 2 != sizeof...(Types))
          return ReadNamedTuple<Index + 2>(name, t);

        return std::make_unique<IgnoreReflector>();
      }
    };

    template <typename T>
    class Reflector<std::optional<T>> : public RefReflector<std::optional<T>>
    {
    public:
      using OptType = std::optional<T>;
      using RefReflector<OptType>::m_obj;

      static T& MakeInnerRef(OptType& opt)
      {
        opt = std::optional<T>(std::in_place, T());
        return *opt;
      }

      explicit Reflector(OptType& obj)
        : RefReflector<OptType>(obj)
        , m_inner(MakeInnerRef(obj))
      {}

      Error::Type getNonTypeError() override { return m_inner.getNonTypeError(); }
      Error::Type setValue(double number) override { return m_inner.setValue(number); }
      Error::Type setValue(bool boolean) override { return m_inner.setValue(boolean); }
      Error::Type setValue(std::nullptr_t) override
      {
        if (m_inner.setValue(nullptr) != Error::None)
          m_obj = std::nullopt;

        return Error::None;
      }

      bool allowString() override { return m_inner.allowString(); }
      void setValue(std::string str) override { m_inner.setValue(std::move(str)); }

      bool allowObject() override { return m_inner.allowObject(); }
      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override { return m_inner.getReflectorForKey(std::move(key)); }
      bool allowArray() override { return m_inner.allowArray(); }
      std::unique_ptr<BaseReflector> getReflectorInArray() override { return m_inner.getReflectorInArray(); }
      Error::Type complete() override { return m_inner.complete(); }

    protected:
      Reflector<T> m_inner;
    };

    template <typename T>
    class Reflector<T, std::enable_if_t<std::is_enum_v<T>>> : public RefReflector<T>
    {
    public:
      using RefReflector<T>::RefReflector;
      using RefReflector<T>::m_obj;
      using RefReflector<T>::setValue;

      Error::Type getNonTypeError() override { return Error::NumberExpected; }
      Error::Type setValue(double number) override
      {
        m_obj = (T)number;
        return Error::None;
      }

      bool allowString() override { return true; }
      void setValue(std::string str) override
      {
        if constexpr (EnumTable<T>())
        {
          size_t index = 0;
          const auto* names = EnumTable<T>::Names;
          const auto* values = EnumTable<T>::Values;

          while (true)
          {
            auto name = GetNameSlice(names, index);
            if (name.empty())
              break;

            if (name == str)
            {
              m_obj = values[index];
              return;
            }

            ++index;
          }

          m_err = Error::InvalidEnum;
        }
      }

      Error::Type complete() override { return m_err; }

    protected:
      Error::Type m_err = Error::None;
    };

    template <typename T>
    class Reflector<T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>> : public RefReflector<T>
    {
    public:
      using RefReflector<T>::RefReflector;
      using RefReflector<T>::m_obj;
      using RefReflector<T>::setValue;

      Error::Type getNonTypeError() override { return Error::NumberExpected; }
      Error::Type setValue(double number) override
      {
        m_obj = number;
        return Error::None;
      }
    };

    template <>
    class Reflector<bool> : public RefReflector<bool>
    {
    public:
      using RefReflector<bool>::RefReflector;
      using RefReflector<bool>::m_obj;
      using RefReflector<bool>::setValue;

      Error::Type getNonTypeError() override { return Error::BooleanExpected; }
      Error::Type setValue(bool boolean) override
      {
        m_obj = boolean;
        return Error::None;
      }
    };

    template <>
    class Reflector<std::string> : public RefReflector<std::string>
    {
    public:
      using RefReflector<std::string>::RefReflector;
      using RefReflector<std::string>::m_obj;
      using RefReflector<std::string>::setValue;

      Error::Type getNonTypeError() override { return Error::StringExpected; }
      bool allowString() override { return true; }
      void setValue(std::string str) override { m_obj = std::move(str); }
    };

    template <typename T>
    class Reflector<std::vector<T>> : public RefReflector<std::vector<T>>
    {
    public:
      using RefReflector<std::vector<T>>::m_obj;

      explicit Reflector(std::vector<T>& obj)
        : RefReflector<std::vector<T>>::RefReflector(obj)
      {
        m_obj.clear();
      }

      Error::Type getNonTypeError() override { return Error::ArrayExpected; }
      bool allowArray() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorInArray() override
      {
        m_obj.push_back({});
        return std::make_unique<Reflector<T>>(m_obj.back());
      }
    };

    template <typename ContainerType, typename T, size_t N>
    class ArrayReflector : public RefReflector<ContainerType>
    {
    public:
      using RefReflector<ContainerType>::RefReflector;
      using RefReflector<ContainerType>::m_obj;

      Error::Type getNonTypeError() override { return Error::ArrayExpected; }
      bool allowArray() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorInArray() override
      {
        if (m_nextIndex == N)
        {
          return std::unique_ptr<IgnoreReflector>();
          m_wrongSize = true;
        }

        m_nextIndex++;
        return std::make_unique<Reflector<T>>(m_obj[m_nextIndex - 1]);
      }

      Error::Type complete() override
      {
        if (m_wrongSize || m_nextIndex != N)
          return Error::WrongArraySize;

        return Error::None;
      }

    protected:
      size_t m_nextIndex = 0;
      bool m_wrongSize = false;
    };

    template <typename T, size_t N>
    class Reflector<T[N]> : public ArrayReflector<T[N], T, N>
    {
    public:
      using ArrayReflector<T[N], T, N>::ArrayReflector;
    };

    template <typename T, size_t N>
    class Reflector<std::array<T, N>> : public ArrayReflector<std::array<T, N>, T, N>
    {
    public:
      using ArrayReflector<std::array<T, N>, T, N>::ArrayReflector;
    };

    template <typename ContainerType, typename K, typename T>
    class MapReflector : public RefReflector<ContainerType>
    {
    public:
      using RefReflector<ContainerType>::m_obj;

      explicit MapReflector(ContainerType& obj)
        : RefReflector<ContainerType>::RefReflector(obj)
      {
        m_obj.clear();
      }

      Error::Type getNonTypeError() override { return Error::ObjectExpected; }
      bool allowObject() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override
      {
        return std::make_unique<Reflector<T>>(m_obj[std::move(key)]);
      }
    };

    template <typename K, typename T, typename P, typename A>
    class Reflector<std::map<K, T, P, A>> : public MapReflector<std::map<K, T, P, A>, K, T>
    {
    public:
      using MapReflector<std::map<K, T, P, A>, K, T>::MapReflector;
    };

    template <typename K, typename T, typename P, typename A>
    class Reflector<std::unordered_map<K, T, P, A>> : public MapReflector<std::unordered_map<K, T, P, A>, K, T>
    {
    public:
      using MapReflector<std::unordered_map<K, T, P, A>, K, T>::MapReflector;
    };

    template <>
    class Reflector<IndependentValue> : public RefReflector<IndependentValue>
    {
    public:
      using RefReflector<IndependentValue>::RefReflector;
      using RefReflector<IndependentValue>::m_obj;

      Error::Type getNonTypeError() override { return Error::ObjectExpected; }
      Error::Type setValue(double number) override
      {
        m_obj.value = number;
        return Error::None;
      }

      Error::Type setValue(bool boolean) override
      {
        m_obj.value = boolean;
        return Error::None;
      }

      Error::Type setValue(std::nullptr_t) override
      {
        m_obj.value = std::monostate();
        return Error::None;
      }

      bool allowString() override { return true; }
      void setValue(std::string str) override { m_obj.value = std::move(str); }

      bool allowObject() override
      {
        m_obj.value = IndependentValue::Map();
        return true;
      }

      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override
      {
        if (auto* map = std::get_if<IndependentValue::Map>(&m_obj.value))
          return std::make_unique<Reflector<IndependentValue>>((*map)[std::move(key)]);

        throw std::logic_error("Attempt to get object item on non object value");
      }

      bool allowArray() override
      {
        m_obj.value = IndependentValue::Array();
        return true;
      }

      std::unique_ptr<BaseReflector> getReflectorInArray() override
      {
        if (auto* array = std::get_if<IndependentValue::Array>(&m_obj.value))
        {
          array->push_back({});
          return std::make_unique<Reflector<IndependentValue>>(array->back());
        }

        throw std::logic_error("Attempt to get array item on non array value");
      }
    };

  } // namespace detail

  class ReflectionBuilder : public Builder
  {
  public:
    template <typename T>
    explicit ReflectionBuilder(T& obj)
    {
      m_stack.push_back(std::make_unique<detail::Reflector<T>>(obj));
    }

    Error::Type setValue(double number) override { return m_stack.back()->setValue(number); }
    Error::Type setValue(bool boolean) override { return m_stack.back()->setValue(boolean); }
    Error::Type setValue(std::nullptr_t) override { return m_stack.back()->setValue(nullptr); }

    Error::Type setString() override
    {
      m_str.clear();
      if (m_processingKey)
        return Error::None;

      if (!m_stack.back()->allowString())
        return m_stack.back()->getNonTypeError();

      return Error::None;
    }

    void stringBufferAdd(char ch) override { m_str.push_back(ch); }
    void stringBufferAdd(std::string_view str) override { m_str = string(str); }
    void stringBufferAddUtf8(uint32_t ch) override { StringBufferAddUtf8(m_str, ch); }
    void stringBufferEnd() override
    {
      if (m_processingKey)
        return;

      m_stack.back()->setValue(std::move(m_str));
    }

    Error::Type pushObject() override
    {
      if (!m_stack.back()->allowObject())
        return m_stack.back()->getNonTypeError();

      m_processingKey = true;
      return Error::None;
    }

    Error::Type pushArray() override
    {
      if (!m_stack.back()->allowArray())
        return m_stack.back()->getNonTypeError();

      return Error::None;
    }

    Error::Type pop() override { return m_stack.back()->complete(); }

    void addKey() override
    {
      m_processingKey = false;
      m_stack.push_back(m_stack.back()->getReflectorForKey(std::move(m_str)));
      m_str.clear();
    }

    void addKeyedValue() override
    {
      m_processingKey = true;
      m_stack.pop_back();
    }

    void beginArrayValue() override { m_stack.push_back(m_stack.back()->getReflectorInArray()); }
    void addArrayValue() override { m_stack.pop_back(); }

    bool isValidRoot() override { return true; }

  protected:
    bool m_processingKey = false;
    std::string m_str;
    std::vector<std::unique_ptr<detail::BaseReflector>> m_stack;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  class DocumentWriter : public Writer
  {
  public:
    explicit DocumentWriter(Document& doc)
      : m_builder(doc)
    {}

    void writeNull() override { m_builder.setValue(nullptr); }
    void writeBoolean(bool boolean) override { m_builder.setValue(boolean); }
    void writeNumber(double number) override { m_builder.setValue(number); }

    void writeString(const char* str) override
    {
      m_builder.setString();
      m_builder.stringBufferAdd(str);
    }
    void writeString(std::string_view str) override
    {
      m_builder.setString();
      m_builder.stringBufferAdd(str);
    }

    void push()
    {
      m_firstElementStack.push_back(m_firstElement);
      m_firstElement = true;
    }

    void pop()
    {
      m_firstElement = m_firstElementStack.back();
      m_firstElementStack.pop_back();
    }

    void beginArray() override
    {
      push();
      m_builder.pushArray();
    }

    void beginArrayElement() override
    {
      if (!m_firstElement)
        m_builder.addArrayValue();
      else
        m_firstElement = false;
    }

    void endArray() override
    {
      if (!m_firstElement)
        m_builder.addArrayValue();

      pop();
      m_builder.pop();
    }

    void writeEmptyArray() override
    {
      m_builder.pushArray();
      m_builder.pop();
    }

    void beginObject() override
    {
      push();
      m_builder.pushObject();
    }

    void writeObjectKey(const char* str) override
    {
      m_builder.setString();
      m_builder.stringBufferAdd(str);
      m_builder.addKey();
    }

    void writeObjectKey(std::string_view str) override
    {
      m_builder.setString();
      m_builder.stringBufferAdd(str);
      m_builder.addKey();
    }

    void beginObjectElement() override
    {
      if (!m_firstElement)
        m_builder.addKeyedValue();
      else
        m_firstElement = false;
    }

    void endObject() override
    {
      if (!m_firstElement)
        m_builder.addKeyedValue();

      pop();
      m_builder.pop();
    }

    void writeEmptyObject() override
    {
      m_builder.pushObject();
      m_builder.pop();
    }

    void complete() override {}

  protected:
    bool m_firstElement = false;
    vector<bool> m_firstElementStack;
    DocumentBuilder m_builder;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline void ToDocument(Document& doc, const T& in)
  {
    DocumentWriter writer(doc);
    detail::Write(writer, in);
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline void ToStream(std::ostream& os, const T& in, const WriterParams& wp)
  {
    Json5Writer writer(os, wp);
    detail::Write(writer, in);
    writer.complete();
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline void ToString(std::string& str, const T& in, const WriterParams& wp)
  {
    std::ostringstream os;
    ToStream(os, in, wp);
    str = os.str();
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline std::string ToString(const T& in, const WriterParams& wp)
  {
    std::string result;
    ToString(result, in, wp);
    return result;
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline bool ToFile(std::string_view fileName, const T& in, const WriterParams& wp)
  {
    std::ofstream ofs(std::string(fileName).c_str());
    ToStream(ofs, in, wp);
    return true;
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  class DocumentParser final
  {
  public:
    DocumentParser(Builder& builder, const Document& doc)
      : m_builder(builder)
      , m_doc(doc)
    {}

    Error parse()
    {
      return parseValue(m_doc);
    }

    Error parseValue(const Value& v)
    {
      if (v.isNumber())
        return {m_builder.setValue(v.get<double>())};
      if (v.isBoolean())
        return {m_builder.setValue(v.getBool())};
      if (v.isNull())
        return {m_builder.setValue(nullptr)};

      if (v.isString())
      {
        if (Error::Type err = m_builder.setString())
          return {err};

        m_builder.stringBufferAdd(v.getCStr());
        m_builder.stringBufferEnd();
        return {Error::None};
      }

      if (v.isObject())
      {
        if (Error::Type err = m_builder.pushObject())
          return {err};

        for (auto kvp : ObjectView(v))
        {
          if (Error::Type err = m_builder.setString())
            return {err};

          m_builder.stringBufferAdd(kvp.first);
          m_builder.stringBufferEnd();
          m_builder.addKey();

          if (Error err = parseValue(kvp.second))
            return err;

          m_builder.addKeyedValue();
        }

        return {m_builder.pop()};
      }

      if (v.isArray())
      {
        if (Error::Type err = m_builder.pushArray())
          return {err};

        for (auto inner : ArrayView(v))
        {
          if (Error err = parseValue(inner))
            return err;

          m_builder.addArrayValue();
        }

        return {m_builder.pop()};
      }

      return {Error::SyntaxError};
    }

  protected:
    Builder& m_builder;
    const Document& m_doc;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline Error FromDocument(const Document& doc, T& out)
  {
    ReflectionBuilder builder(out);
    DocumentParser p(builder, doc);
    return p.parse();
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline Error FromString(std::string_view str, T& out)
  {
    ReflectionBuilder builder(out);
    return FromString(str, (Builder&)builder);
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline Error FromFile(std::string_view fileName, T& out)
  {
    ReflectionBuilder builder(out);
    return FromFile(fileName, (Builder&)builder);
  }

} // namespace json5
