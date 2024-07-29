#pragma once

#include <array>
#include <fstream>
#include <map>
#include <unordered_map>

#include "json5_builder.hpp"

namespace json5
{

  //
  template <typename T>
  void ToDocument(Document& doc, const T& in, const WriterParams& wp = WriterParams());

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

    // Pre-declare so compiler knows it exists before it is attempted to be used
    template <typename T>
    Error Read(const json5::Value& in, T& out);

    class Writer final : public Builder
    {
    public:
      Writer(Document& doc, const WriterParams& wp)
        : Builder(doc)
        , m_params(wp)
      {}

      const WriterParams& params() const noexcept { return m_params; }

    private:
      WriterParams m_params;
    };

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
    json5::Value Write(Writer& w, const T& in);
    template <typename T>
    json5::Value WriteArray(Writer& w, const T* in, size_t numItems);
    template <typename T>
    json5::Value WriteMap(Writer& w, const T& in);
    template <typename T>
    json5::Value WriteEnum(Writer& w, T in);

    //---------------------------------------------------------------------------------------------------------------------
    inline json5::Value Write(Writer& w, bool in) { return json5::Value(in); }
    inline json5::Value Write(Writer& w, int in) { return json5::Value(double(in)); }
    inline json5::Value Write(Writer& w, unsigned in) { return json5::Value(double(in)); }
    inline json5::Value Write(Writer& w, float in) { return json5::Value(double(in)); }
    inline json5::Value Write(Writer& w, double in) { return json5::Value(in); }
    inline json5::Value Write(Writer& w, const char* in) { return w.newString(in); }
    inline json5::Value Write(Writer& w, const std::string& in) { return w.newString(in); }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, typename A>
    inline json5::Value Write(Writer& w, const std::vector<T, A>& in)
    {
      return WriteArray(w, in.data(), in.size());
    }

    template <typename A>
    inline json5::Value Write(Writer& w, const std::vector<bool, A>& in)
    {
      w.pushArray();

      for (size_t i = 0; i < in.size(); ++i)
        w += Write(w, (bool)in[i]);

      return w.pop();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    inline json5::Value Write(Writer& w, const T (&in)[N])
    {
      return WriteArray(w, in, N);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    inline json5::Value Write(Writer& w, const std::array<T, N>& in)
    {
      return WriteArray(w, in.data(), N);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename K, typename T, typename P, typename A>
    inline json5::Value Write(Writer& w, const std::map<K, T, P, A>& in)
    {
      return WriteMap(w, in);
    }

    template <typename K, typename T, typename H, typename EQ, typename A>
    inline json5::Value Write(Writer& w, const std::unordered_map<K, T, H, EQ, A>& in)
    {
      return WriteMap(w, in);
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
      else if constexpr (std::is_enum_v<Type>)
      {
        if constexpr (EnumTable<Type>())
          w[name] = WriteEnum(w, in);
        else
          w[name] = Write(w, std::underlying_type_t<Type>(in));
      }
      else
        w[name] = Write(w, in);
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
    inline json5::Value Write(Writer& w, const T& in)
    {
      w.pushObject();
      WriteNamedTuple(w, ClassWrapper<T>::MakeNamedTuple(in));
      return w.pop();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline json5::Value WriteArray(Writer& w, const T* in, size_t numItems)
    {
      w.pushArray();

      for (size_t i = 0; i < numItems; ++i)
        w += Write(w, in[i]);

      return w.pop();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline json5::Value WriteMap(Writer& w, const T& in)
    {
      w.pushObject();

      for (const auto& kvp : in)
        w[kvp.first] = Write(w, kvp.second);

      return w.pop();
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline json5::Value WriteEnum(Writer& w, T in)
    {
      size_t index = 0;
      const auto* names = EnumTable<T>::Names;
      const auto* values = EnumTable<T>::Values;

      while (true)
      {
        auto name = GetNameSlice(names, index);

        // Underlying value fallback
        if (name.empty())
          return Write(w, std::underlying_type_t<T>(in));

        if (in == values[index])
          return w.newString(name);

        ++index;
      }

      return json5::Value();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /* Forward declarations */
    template <typename T>
    Error Read(const json5::Value& in, T& out);
    template <typename T>
    Error ReadArray(const json5::Value& in, T* out, size_t numItems);
    template <typename T>
    Error ReadMap(const json5::Value& in, T& out);
    template <typename T>
    Error ReadEnum(const json5::Value& in, T& out);

    //---------------------------------------------------------------------------------------------------------------------
    inline Error Read(const json5::Value& in, bool& out)
    {
      if (!in.isBoolean())
        return {Error::NumberExpected};

      out = in.getBool();
      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline Error ReadNumber(const json5::Value& in, T& out)
    {
      return in.tryGet(out) ? Error() : Error {Error::NumberExpected};
    }

    //---------------------------------------------------------------------------------------------------------------------
    inline Error Read(const json5::Value& in, int& out) { return ReadNumber(in, out); }
    inline Error Read(const json5::Value& in, unsigned& out) { return ReadNumber(in, out); }
    inline Error Read(const json5::Value& in, float& out) { return ReadNumber(in, out); }
    inline Error Read(const json5::Value& in, double& out) { return ReadNumber(in, out); }

    //---------------------------------------------------------------------------------------------------------------------
    inline Error Read(const json5::Value& in, const char*& out)
    {
      if (!in.isString())
        return {Error::StringExpected};

      out = in.getCStr();
      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    inline Error Read(const json5::Value& in, std::string& out)
    {
      if (!in.isString())
        return {Error::StringExpected};

      out = in.getCStr();
      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    inline Error Read(const json5::Value& in, T (&out)[N])
    {
      return ReadArray(in, out, N);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    inline Error Read(const json5::Value& in, std::array<T, N>& out)
    {
      return ReadArray(in, out.data(), N);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, typename A>
    inline Error Read(const json5::Value& in, std::vector<T, A>& out)
    {
      if (!in.isArray() && !in.isNull())
        return {Error::ArrayExpected};

      auto arr = json5::ArrayView(in);

      out.clear();
      out.reserve(arr.size());
      for (const auto& i : arr)
        if (auto err = Read(i, out.emplace_back()))
          return err;

      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename K, typename T, typename P, typename A>
    inline Error Read(const json5::Value& in, std::map<K, T, P, A>& out)
    {
      return ReadMap(in, out);
    }

    template <typename K, typename T, typename H, typename EQ, typename A>
    inline Error Read(const json5::Value& in, std::unordered_map<K, T, H, EQ, A>& out)
    {
      return ReadMap(in, out);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename Type>
    inline Error ReadTupleValue(const json5::ObjectView::Iterator iter, Type& out)
    {
      if constexpr (is_optional_v<Type>)
      {
        if ((*iter).second.isNull())
        {
          out = std::nullopt;
        }
        else
        {
          out = typename Type::value_type();
          if (auto err = ReadTupleValue<typename Type::value_type>(iter, *out))
            return err;
        }
      }
      else if constexpr (std::is_enum_v<Type>)
      {
        if constexpr (EnumTable<Type>())
        {
          if (auto err = ReadEnum((*iter).second, out))
            return err;
        }
        else
        {
          std::underlying_type_t<Type> temp;
          if (auto err = Read((*iter).second, temp))
            return err;

          out = Type(temp);
        }
      }
      else
      {
        if (auto err = Read((*iter).second, out))
          return err;
      }

      return {Error::None};
    }
    //---------------------------------------------------------------------------------------------------------------------
    template <size_t Index = 0, typename... Types>
    inline Error ReadTuple(const json5::ObjectView& obj, const char* names, std::tuple<Types...>& t)
    {
      auto& out = std::get<Index>(t);
      using Type = std::remove_reference_t<decltype(out)>;

      auto name = GetNameSlice(names, Index);

      auto iter = obj.find(name);
      if (iter != obj.end())
      {
        if (auto err = ReadTupleValue<Type>(iter, out))
          return err;
      }

      if constexpr (Index + 1 != sizeof...(Types))
      {
        if (auto err = ReadTuple<Index + 1>(obj, names, t))
          return err;
      }

      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <size_t Index = 0, typename... Types>
    inline Error ReadNamedTuple(const json5::ObjectView& obj, std::tuple<Types...>& t)
    {
       if (auto err = ReadTuple(obj, std::get<Index>(t), std::get<Index + 1>(t)))
         return err;

       if constexpr (Index + 2 != sizeof...(Types) )
         return ReadNamedTuple<Index + 2>(obj, t);

       return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline Error Read(const json5::Value& in, T& out)
    {
      if (!in.isObject())
        return {Error::ObjectExpected};

      auto namedTuple = ClassWrapper<T>::MakeNamedTuple(out);
      return ReadNamedTuple(json5::ObjectView(in), namedTuple);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <size_t Index = 0, typename Head, typename... Tail>
    inline Error Read(const json5::ArrayView& arr, Head& out, Tail&... tail)
    {
      if constexpr (Index == 0)
      {
        if (!arr.isValid())
          return {Error::ArrayExpected};

        if (arr.size() != (1 + sizeof...(Tail)))
          return {Error::WrongArraySize};
      }

      if (auto err = Read(arr[Index], out))
        return err;

      if constexpr (sizeof...(Tail) > 0)
        return Read<Index + 1>(arr, tail...);

      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline Error ReadArray(const json5::Value& in, T* out, size_t numItems)
    {
      if (!in.isArray())
        return {Error::ArrayExpected};

      auto arr = json5::ArrayView(in);
      if (arr.size() != numItems)
        return {Error::WrongArraySize};

      for (size_t i = 0; i < numItems; ++i)
        if (auto err = Read(arr[i], out[i]))
          return err;

      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline Error ReadMap(const json5::Value& in, T& out)
    {
      if (!in.isObject() && !in.isNull())
        return {Error::ObjectExpected};

      std::pair<typename T::key_type, typename T::mapped_type> kvp;

      out.clear();
      for (auto jsKV : json5::ObjectView(in))
      {
        kvp.first = jsKV.first;

        if (auto err = Read(jsKV.second, kvp.second))
          return err;

        out.emplace(std::move(kvp));
      }

      return {Error::None};
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline Error ReadEnum(const json5::Value& in, T& out)
    {
      if (!in.isString() && !in.isNumber())
        return {Error::StringExpected};

      size_t index = 0;
      const auto* names = EnumTable<T>::Names;
      const auto* values = EnumTable<T>::Values;

      while (true)
      {
        auto name = GetNameSlice(names, index);

        if (name.empty())
          break;

        if (in.isString() && name == in.getCStr())
        {
          out = values[index];
          return {Error::None};
        }
        else if (in.isNumber() && in.get<int>() == int(values[index]))
        {
          out = values[index];
          return {Error::None};
        }

        ++index;
      }

      return {Error::InvalidEnum};
    }

  } // namespace detail

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline void ToDocument(Document& doc, const T& in, const WriterParams& wp)
  {
    detail::Writer w(doc, wp);
    detail::Write(w, in);
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline void ToStream(std::ostream& os, const T& in, const WriterParams& wp)
  {
    Document doc;
    ToDocument(doc, in, wp);
    ToStream(os, doc, wp);
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline void ToString(std::string& str, const T& in, const WriterParams& wp)
  {
    Document doc;
    ToDocument(doc, in);
    ToString(str, doc, wp);
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

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline Error FromDocument(const Document& doc, T& out)
  {
    return detail::Read(doc, out);
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline Error FromString(std::string_view str, T& out)
  {
    Document doc;
    if (auto err = FromString(str, doc))
      return err;

    return FromDocument(doc, out);
  }

  //---------------------------------------------------------------------------------------------------------------------
  template <typename T>
  inline Error FromFile(std::string_view fileName, T& out)
  {
    Document doc;
    if (auto err = FromFile(fileName, doc))
      return err;

    return FromDocument(doc, out);
  }

} // namespace json5
