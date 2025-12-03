#pragma once

#include <array>
#include <fstream>
#include <map>
#include <math.h>
#include <optional>
#include <set>
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

    struct IvarJsonNameSubstitution
    {
      std::string_view ivarName;
      std::string_view jsonName;
    };

    struct WrapperHelper
    {
      // Several constexpr functions which will not actually be compiled because they only compute constants in the ClassWrapper below
      static constexpr size_t CommaCount(std::string_view str)
      {
        size_t count = 0;
        for (char ch : str)
        {
          if (ch == ',')
            count++;
        }

        return count;
      }

      template <size_t Count>
      static constexpr std::array<size_t, Count> GetCommas(std::string_view str)
      {
        if constexpr (Count == 0)
        {
          return std::array<size_t, 0>();
        }
        else
        {
          std::array<size_t, Count> ret;
          size_t index = 0;
          size_t pos = 0;
          for (char ch : str)
          {
            if (ch == ',')
            {
              ret[index] = pos;
              index++;
            }

            pos++;
          }

          return ret;
        }
      }

      template <size_t N, size_t Count>
      static constexpr std::string_view GetName(const std::array<size_t, Count - 1>& commas, std::string_view namesStr)
      {
        static_assert(N < Count);
        if constexpr (N == 0)
        {
          if constexpr (Count != 1)
            return namesStr.substr(0, commas[0]);

          return namesStr;
        }
        else
        {
          size_t start = commas[N - 1];

          // Advance past comma and whitespace
          do {
            start++;
          } while (namesStr[start] <= 32);

          if constexpr (N != Count - 1)
            return namesStr.substr(start, commas[N] - start);

          return namesStr.substr(start);
        }
      }

      template <size_t N, size_t Count>
      static constexpr void FillNames(std::array<std::string_view, Count>& ret, const std::array<size_t, Count - 1>& commas, std::string_view namesStr)
      {
        ret[N] = GetName<N, Count>(commas, namesStr);
        if constexpr (N < Count - 1)
          FillNames<N + 1, Count>(ret, commas, namesStr);
      }

      template <size_t Count>
      static constexpr std::array<std::string_view, Count> GetNames(const std::array<size_t, Count - 1>& commas, std::string_view namesStr)
      {
        static_assert(Count > 0);
        std::array<std::string_view, Count> ret;
        if constexpr (Count == 1)
          ret[0] = namesStr;
        else
          FillNames<0, Count>(ret, commas, namesStr);

        return ret;
      }

      template <size_t Count, size_t SubstitutionCount>
      static constexpr std::array<std::string_view, Count> SubstituteNames(std::array<std::string_view, Count> names, std::array<IvarJsonNameSubstitution, SubstitutionCount> substitutions)
      {
        for (size_t substitutionIndex = 0; substitutionIndex < SubstitutionCount; substitutionIndex++)
        {
          for (size_t nameIndex = 0; nameIndex < Count; nameIndex++)
          {
            if (names[nameIndex] == substitutions[substitutionIndex].ivarName)
              names[nameIndex] = substitutions[substitutionIndex].jsonName;
          }
        }

        return names;
      }
    };

    template <typename T>
    struct NameSubstitution
    {
      static constexpr std::array<IvarJsonNameSubstitution, 0> Substitutions = {};
    };

    template <typename T>
    class ClassWrapper : public Json5Access<T>
    {
    public:
      // Constants for use in actual compiled functions
      static constexpr std::string_view NamesStr = Json5Access<T>::GetNames();
      static constexpr size_t NameCount = WrapperHelper::CommaCount(NamesStr) + 1;
      static constexpr std::array<size_t, NameCount - 1> Commas = WrapperHelper::GetCommas<NameCount - 1>(NamesStr);
      static constexpr std::array<std::string_view, NameCount> Names = WrapperHelper::SubstituteNames(WrapperHelper::GetNames<NameCount>(Commas, NamesStr), NameSubstitution<T>::Substitutions);

      // Some sanity checks
      static_assert(NameCount == std::tuple_size_v<decltype(Json5Access<T>::GetTie(std::declval<T&>()))>);
      static_assert(NameCount == std::tuple_size_v<decltype(Json5Access<T>::GetTie(std::declval<const T&>()))>);
    };

    template <typename T>
    class EnumWrapper : public EnumTable<T>
    {
    public:
      // Constants for use in actual compiled functions
      static constexpr size_t NameCount = WrapperHelper::CommaCount(EnumTable<T>::Names) + 1;
      static constexpr std::array<size_t, NameCount - 1> Commas = WrapperHelper::GetCommas<NameCount - 1>(EnumTable<T>::Names);
      static constexpr std::array<std::string_view, NameCount> Names = WrapperHelper::GetNames<NameCount>(Commas, EnumTable<T>::Names);

      // Sanity check
      static_assert(NameCount == std::size(EnumTable<T>::Values));
    };

    /* Forward declarations */
    template <typename T>
    inline void Write(Writer& w, const T& in);
    template <typename T>
    inline void WriteObjTuples(Writer& w, const T& in);
    template <typename T>
    void WriteArray(Writer& w, const T* in, size_t numItems);
    template <typename T>
    void WriteMap(Writer& w, const T& in);
    template <typename T>
    void WriteEnum(Writer& w, T in);

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, typename Enable = void>
    struct ReflectionWriter
    {
      static inline void Write(Writer& w, const T& in)
      {
        w.beginObject();
        WriteObjTuples(w, in);
        w.endObject();
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <>
    struct ReflectionWriter<std::monostate>
    {
      static inline void Write(Writer& w, std::monostate in) { w.writeNull(); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    struct ReflectionWriter<T, std::enable_if_t<std::is_arithmetic_v<T>>>
    {
      static inline void Write(Writer& w, T in)
      {
        if constexpr (std::is_integral_v<T>)
          assert(in < (1LL << 53));

        w.writeNumber((double)in);
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <>
    struct ReflectionWriter<bool>
    {
      static inline void Write(Writer& w, bool in) { w.writeBoolean(in); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <>
    struct ReflectionWriter<char*>
    {
      static inline void Write(Writer& w, const char* in) { w.writeString(in); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <>
    struct ReflectionWriter<std::string>
    {
      static inline void Write(Writer& w, const std::string& in) { w.writeString(in); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, typename A>
    struct ReflectionWriter<std::vector<T, A>>
    {
      static inline void Write(Writer& w, const std::vector<T, A>& in) { WriteArray(w, in.data(), in.size()); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename A>
    struct ReflectionWriter<std::vector<bool, A>>
    {
      static inline void Write(Writer& w, const std::vector<bool, A>& in)
      {
        w.beginArray();
        for (size_t i = 0; i < in.size(); ++i)
        {
          w.beginArrayElement();
          ReflectionWriter<bool>::Write(w, (bool)in[i]);
        }

        w.endArray();
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, typename A>
    struct ReflectionWriter<std::set<T, A>>
    {
      static inline void Write(Writer& w, const std::set<T, A>& in)
      {
        w.beginArray();
        for (const T& value : in)
        {
          w.beginArrayElement();
          ReflectionWriter<T>::Write(w, value);
        }

        w.endArray();
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    struct ReflectionWriter<T[N]>
    {
      static inline void Write(Writer& w, const T (&in)[N]) { return WriteArray(w, in, N); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t N>
    struct ReflectionWriter<std::array<T, N>>
    {
      static inline void Write(Writer& w, const std::array<T, N>& in) { return WriteArray(w, in.data(), N); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename K, typename T, typename P, typename A>
    struct ReflectionWriter<std::map<K, T, P, A>>
    {
      static inline void Write(Writer& w, const std::map<K, T, P, A>& in) { WriteMap(w, in); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename K, typename T, typename H, typename EQ, typename A>
    struct ReflectionWriter<std::unordered_map<K, T, H, EQ, A>>
    {
      static inline void Write(Writer& w, const std::unordered_map<K, T, H, EQ, A>& in) { WriteMap(w, in); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename K, typename T, typename C, typename A>
    struct ReflectionWriter<std::multimap<K, T, C, A>>
    {
      static inline void Write(Writer& w, const std::multimap<K, T, C, A>& in)
      {
        w.beginObject();

        if (!in.empty())
        {
          std::optional<std::reference_wrapper<const K>> lastKey;

          for (const auto& pair : in)
          {
            if (!lastKey || lastKey->get() != pair.first)
            {
              if (lastKey)
                w.endArray();

              w.beginObjectElement();
              w.writeObjectKey(pair.first);
              w.beginArray();

              lastKey = std::reference_wrapper<const K>(pair.first);
            }

            w.beginArrayElement();
            ReflectionWriter<T>::Write(w, pair.second);
          }

          w.endArray();
        }

        w.endObject();
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename... Types>
    class ReflectionWriter<std::variant<Types...>>
    {
    public:
      static inline void Write(Writer& w, const std::variant<Types...>& in)
      {
        std::visit([&w](const auto& value) { ReflectionWriter<std::decay_t<decltype(value)>>::Write(w, value); }, in);
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    class ReflectionWriter<std::shared_ptr<T>>
    {
    public:
      static inline void Write(Writer& w, const std::shared_ptr<T>& in)
      {
        if (in)
          ReflectionWriter<T>::Write(w, *in);
        else
          w.writeNull();
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <>
    struct ReflectionWriter<IndependentValue>
    {
    public:
      static inline void Write(Writer& w, const IndependentValue& value) { ReflectionWriter<IndependentValue::ValueType>::Write(w, value.value); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <>
    struct ReflectionWriter<Document>
    {
      static inline void Write(Writer& w, const Document& in) { json5::Write(w, (const Value&)in); }
    };

    //---------------------------------------------------------------------------------------------------------------------
    class TupleWriter
    {
    public:
      template <typename FirstType, typename... RemainingTypes>
      static inline void Write(Writer& w, const FirstType& first, const RemainingTypes&... remaining)
      {
        w.beginArrayElement();
        json5::detail::Write(w, first);
        TupleWriter::Write<RemainingTypes...>(w, remaining...);
      }

      template <typename OnlyType>
      static inline void Write(Writer& w, const OnlyType& only)
      {
        w.beginArrayElement();
        json5::detail::Write(w, only);
      }
    };

    template <typename... Types>
    class TupleReflectionWriter
    {
    public:
      static inline void Write(Writer& w, const Types&... obj)
      {
        w.beginArray();
        TupleWriter::Write<Types...>(w, obj...);
        w.endArray();
      }
    };

    //---------------------------------------------------------------------------------------------------------------------
    template <typename Type>
    inline void WriteTupleValue(Writer& w, std::string_view name, const Type& in)
    {
      w.beginObjectElement();
      w.writeObjectKey(name);

      if constexpr (std::is_enum_v<Type>)
      {
        if constexpr (EnumTable<Type>())
          WriteEnum(w, in);
        else
          ReflectionWriter<std::underlying_type_t<Type>>::Write(w, std::underlying_type_t<Type>(in));
      }
      else
      {
        ReflectionWriter<Type>::Write(w, in);
      }
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename Type>
    inline void WriteTupleValue(Writer& w, std::string_view name, const std::optional<typename Type::value_type>& in)
    {
      if (in)
        WriteTupleValue<typename Type::value_type>(w, name, *in);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename Type>
    inline void WriteTupleValue(Writer& w, std::string_view name, const std::shared_ptr<typename Type::element_type>& in)
    {
      if (in)
        WriteTupleValue<typename Type::element_type>(w, name, *in);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T, size_t Index = 0, typename... Types>
    inline void WriteTuple(Writer& w, const std::tuple<Types...>& t)
    {
      const auto& in = std::get<Index>(t);
      using Type = std::decay_t<decltype(in)>;

      WriteTupleValue<Type>(w, ClassWrapper<T>::Names[Index], in);
      if constexpr (Index + 1 != sizeof...(Types))
        WriteTuple<T, Index + 1>(w, t);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline void WriteObjTuples(Writer& w, const T& in)
    {
      WriteTuple<T>(w, ClassWrapper<T>::GetTie(in));

      if constexpr (!std::is_same_v<typename Json5Access<T>::SuperCls, std::false_type>)
        WriteObjTuples<typename Json5Access<T>::SuperCls>(w, in);
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline void Write(Writer& w, const T& in)
    {
      ReflectionWriter<T>::Write(w, in);
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
    template <typename T, size_t Index = 0>
    inline bool WriteEnumValue(Writer& w, T in)
    {
      if (in == EnumTable<T>::Values[Index])
      {
        w.writeString(EnumWrapper<T>::Names[Index]);
        return true;
      }

      if constexpr (Index + 1 != std::size(EnumTable<T>::Values))
        return WriteEnumValue<T, Index + 1>(w, in);

      return false;
    }

    //---------------------------------------------------------------------------------------------------------------------
    template <typename T>
    inline void WriteEnum(Writer& w, T in)
    {
      if (!WriteEnumValue<T>(w, in))
        Write(w, std::underlying_type_t<T>(in)); // Not in table so fallback to underlying value
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class BaseReflector
    {
    public:
      static bool AllowsType(ValueType type) { return false; }

      virtual ~BaseReflector() {}
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

      static bool AllowsType(ValueType type) { return type == ValueType::Object; }
      Error::Type getNonTypeError() override { return Error::ObjectExpected; }

      bool allowObject() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override { return GetValueReflector(m_obj, key); }

      //---------------------------------------------------------------------------------------------------------------------
      template <typename ValueType>
      inline static std::unique_ptr<BaseReflector> MakeReflector(ValueType& in)
      {
        return std::make_unique<Reflector<ValueType>>(in);
      }

      //---------------------------------------------------------------------------------------------------------------------
      template <size_t Index = 0>
      inline static std::unique_ptr<BaseReflector> GetValueReflectorInType(T& in, std::string_view name)
      {
        if (name == ClassWrapper<T>::Names[Index])
          return MakeReflector(std::get<Index>(Json5Access<T>::GetTie(in)));
        else if constexpr (Index < ClassWrapper<T>::NameCount - 1)
          return GetValueReflectorInType<Index + 1>(in, name);
        else
          return {};
      }

      //---------------------------------------------------------------------------------------------------------------------
      inline static std::unique_ptr<BaseReflector> GetValueReflector(T& in, std::string_view name)
      {
        if (std::unique_ptr<BaseReflector> reflector = GetValueReflectorInType(in, name))
          return reflector;

        if constexpr (!std::is_same_v<typename Json5Access<T>::SuperCls, std::false_type>)
          return Reflector<typename Json5Access<T>::SuperCls>::GetValueReflector(in, name);

        return std::make_unique<IgnoreReflector>();
      }
    };

    template <>
    class Reflector<std::monostate> : public RefReflector<std::monostate>
    {
    public:
      using RefReflector<std::monostate>::RefReflector;
      static bool AllowsType(ValueType type) { return type == ValueType::Null; }
      Error::Type getNonTypeError() override { return Error::NullExpected; }
      Error::Type setValue(std::nullptr_t) override { return Error::None; }
    };

    template <typename T>
    class Reflector<std::optional<T>> : public RefReflector<std::optional<T>>
    {
    public:
      using OptType = std::optional<T>;
      using RefReflector<OptType>::m_obj;

      static T& MakeInnerRef(OptType& opt)
      {
        opt = std::optional<T>(std::in_place);
        return *opt;
      }

      explicit Reflector(OptType& obj)
        : RefReflector<OptType>(obj)
        , m_inner(MakeInnerRef(obj))
      {}

      static bool AllowsType(ValueType type) { return Reflector<T>::AllowsType(type); }
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
    class Reflector<std::shared_ptr<T>> : public RefReflector<std::shared_ptr<T>>
    {
    public:
      using PtrType = std::shared_ptr<T>;
      using RefReflector<PtrType>::m_obj;

      static T& MakeInnerRef(PtrType& ptr)
      {
        if (!ptr)
          ptr = std::make_shared<T>();

        return *ptr;
      }

      explicit Reflector(PtrType& obj)
        : RefReflector<PtrType>(obj)
        , m_inner(MakeInnerRef(obj))
      {}

      static bool AllowsType(ValueType type) { return Reflector<T>::AllowsType(type); }
      Error::Type getNonTypeError() override { return m_inner.getNonTypeError(); }
      Error::Type setValue(double number) override { return m_inner.setValue(number); }
      Error::Type setValue(bool boolean) override { return m_inner.setValue(boolean); }
      Error::Type setValue(std::nullptr_t) override
      {
        if (m_inner.setValue(nullptr) != Error::None)
          m_obj.reset();

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

      static bool AllowsType(ValueType type) { return type == ValueType::Number || type == ValueType::String; }
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
          for (size_t index = 0; index < sizeof(EnumWrapper<T>::Names); index++)
          {
            if (str == EnumWrapper<T>::Names[index])
            {
              m_obj = EnumTable<T>::Values[index];
              return;
            }
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

      static bool AllowsType(ValueType type)
      {
        if constexpr (!std::is_integral_v<T>)
          return type == ValueType::Number || type == ValueType::Null;

        return type == ValueType::Number;
      }

      Error::Type getNonTypeError() override { return Error::NumberExpected; }
      Error::Type setValue(std::nullptr_t) override
      {
        if constexpr (std::is_integral_v<T>)
        {
          return getNonTypeError();
        }
        else
        {
          m_obj = NAN;
          return Error::None;
        }
      }

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

      static bool AllowsType(ValueType type) { return type == ValueType::Boolean; }
      Error::Type getNonTypeError() override { return Error::BooleanExpected; }
      Error::Type setValue(bool boolean) override
      {
        m_obj = boolean;
        return Error::None;
      }
    };

    template <>
    class Reflector<std::vector<bool>::reference> : public BaseReflector
    {
    public:
      explicit Reflector(std::vector<bool>::reference ref)
        : m_obj(std::move(ref))
      {}

      static bool AllowsType(ValueType type) { return type == ValueType::Boolean; }
      Error::Type getNonTypeError() override { return Error::BooleanExpected; }
      Error::Type setValue(bool boolean) override
      {
        m_obj = boolean;
        return Error::None;
      }

    protected:
      std::vector<bool>::reference m_obj;
    };

    template <>
    class Reflector<std::string> : public RefReflector<std::string>
    {
    public:
      using RefReflector<std::string>::RefReflector;
      using RefReflector<std::string>::m_obj;
      using RefReflector<std::string>::setValue;

      static bool AllowsType(ValueType type) { return type == ValueType::String; }
      Error::Type getNonTypeError() override { return Error::StringExpected; }
      bool allowString() override { return true; }
      void setValue(std::string str) override { m_obj = std::move(str); }
    };

    template <typename T, typename A>
    class Reflector<std::vector<T, A>> : public RefReflector<std::vector<T, A>>
    {
    public:
      using RefReflector<std::vector<T>>::m_obj;

      explicit Reflector(std::vector<T, A>& obj)
        : RefReflector<std::vector<T, A>>::RefReflector(obj)
      {
        m_obj.clear();
      }

      static bool AllowsType(ValueType type) { return type == ValueType::Array; }
      Error::Type getNonTypeError() override { return Error::ArrayExpected; }
      bool allowArray() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorInArray() override
      {
        m_obj.push_back({});
        if constexpr (std::is_same_v<T, bool>)
          return std::make_unique<Reflector<std::vector<bool>::reference>>(m_obj.back());
        else
          return std::make_unique<Reflector<T>>(m_obj.back());
      }
    };

    template <typename T, typename A>
    class Reflector<std::set<T, A>> : public RefReflector<std::set<T, A>>
    {
    public:
      using RefReflector<std::set<T, A>>::m_obj;

      explicit Reflector(std::set<T, A>& obj)
        : RefReflector<std::set<T, A>>::RefReflector(obj)
        , m_inner(m_vec)
      {
        m_obj.clear();
      }

      static bool AllowsType(ValueType type) { return type == ValueType::Array; }
      Error::Type getNonTypeError() override { return Error::ArrayExpected; }
      bool allowArray() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorInArray() override { return m_inner.getReflectorInArray(); }

      Error::Type complete() override
      {
        if (Error::Type err = m_inner.complete(); err != Error::None)
          return err;

        for (T& value : m_vec)
          m_obj.insert(std::move(value));

        return Error::None;
      }

    private:
      std::vector<T> m_vec;
      Reflector<std::vector<T>> m_inner;
    };

    template <typename ContainerType, typename T, size_t N>
    class ArrayReflector : public RefReflector<ContainerType>
    {
    public:
      using RefReflector<ContainerType>::RefReflector;
      using RefReflector<ContainerType>::m_obj;

      static bool AllowsType(ValueType type) { return type == ValueType::Array; }
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

      static bool AllowsType(ValueType type) { return type == ValueType::Object; }
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

    // Reads into a variant using the first type that can take the type of value.
    // So, if you have a std::variant<ObjType1, ObjType2> where both are JSON objects,
    // it will always attempt to read the object into ObjType1.
    //
    template <typename... Types>
    class Reflector<std::variant<Types...>> : public RefReflector<std::variant<Types...>>
    {
    public:
      using VariantType = std::variant<Types...>;
      using RefReflector<VariantType>::RefReflector;
      using RefReflector<VariantType>::m_obj;

      static bool AllowsType(ValueType type) { return VariantAllowsType<>(type); }
      Error::Type getNonTypeError() override { return GetNonTypeError<Types...>(); }

      Error::Type setValue(double number) override
      {
        if (!setupReflector<>(ValueType::Number))
          return getNonTypeError();

        return m_reflector->setValue(number);
      }

      Error::Type setValue(bool boolean) override
      {
        if (!setupReflector<>(ValueType::Boolean))
          return getNonTypeError();

        return m_reflector->setValue(boolean);
      }

      Error::Type setValue(std::nullptr_t) override
      {
        if (!setupReflector<>(ValueType::Null))
          return getNonTypeError();

        return m_reflector->setValue(nullptr);
      }

      bool allowString() override
      {
        if (!setupReflector<>(ValueType::String))
          return false;

        return m_reflector->allowString();
      }

      void setValue(std::string str) override { m_reflector->setValue(std::move(str)); }

      bool allowObject() override
      {
        if (!setupReflector<>(ValueType::Object))
          return false;

        return m_reflector->allowObject();
      }

      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override { return m_reflector->getReflectorForKey(std::move(key)); }

      bool allowArray() override
      {
        if (!setupReflector<>(ValueType::Array))
          return false;

        return m_reflector->allowArray();
      }

      std::unique_ptr<BaseReflector> getReflectorInArray() override { return m_reflector->getReflectorInArray(); }

      Error::Type complete() override { return m_reflector->complete(); }

    protected:
      template <size_t N = 0>
      static bool VariantAllowsType(ValueType type)
      {
        if constexpr (N == std::variant_size_v<VariantType>)
        {
          return false;
        }
        else
        {
          using Type = std::decay_t<decltype(std::get<N>(VariantType()))>;
          if (Reflector<Type>::AllowsType(type))
            return true;

          return VariantAllowsType<N + 1>(type);
        }
      }

      template <typename FirstType, typename... RemainingTypes>
      static Error::Type GetNonTypeError()
      {
        FirstType dummy;
        return Reflector<FirstType>(dummy).getNonTypeError();
      }

      template <typename Type>
      bool makeReflector(ValueType type)
      {
        if (!Reflector<Type>::AllowsType(type))
          return false;

        m_obj = VariantType(std::in_place_type<Type>);
        m_reflector = std::make_unique<Reflector<Type>>(std::get<Type>(m_obj));
        return true;
      }

      template <size_t N = 0>
      bool setupReflector(ValueType type)
      {
        if constexpr (N == std::variant_size_v<VariantType>)
        {
          return false;
        }
        else
        {
          using Type = std::decay_t<decltype(std::get<N>(VariantType()))>;
          if (makeReflector<Type>(type))
            return true;

          return setupReflector<N + 1>(type);
        }
      }

      std::unique_ptr<BaseReflector> m_reflector;
    };
    template <typename K, typename T, typename C, typename A>
    class Reflector<std::multimap<K, T, C, A>> : public RefReflector<std::multimap<K, T, C, A>>
    {
    public:
      using Type = std::multimap<K, T, C, A>;
      using RefReflector<Type>::RefReflector;
      using RefReflector<Type>::m_obj;

      static bool AllowsType(ValueType type) { return type == ValueType::Object; }
      Error::Type getNonTypeError() override { return Error::ObjectExpected; }
      bool allowObject() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override
      {
        insertLast();
        m_lastKey = std::move(key);
        return std::make_unique<Reflector<std::vector<T>>>(m_insertValues);
      }

      Error::Type complete() override
      {
        insertLast();
        return Error::None;
      }

      void insertLast()
      {
        for (T& value : m_insertValues)
          m_obj.insert({m_lastKey, std::move(value)});

        m_lastKey.clear();
        m_insertValues.clear();
      }

    protected:
      K m_lastKey;
      std::vector<T> m_insertValues;
    };

    class DocumentBuilderReflector : public RefReflector<DocumentBuilder>
    {
    public:
      using RefReflector<DocumentBuilder>::m_obj;

      DocumentBuilderReflector(DocumentBuilder& builder, bool objectValue, bool arrayValue, bool root = false)
        : RefReflector<DocumentBuilder>(builder)
        , m_objectValue(objectValue)
        , m_arrayValue(arrayValue)
        , m_root(root)
      {}

      ~DocumentBuilderReflector() override
      {
        if (m_needPop)
          m_obj.pop();
        else if (m_root)
          m_obj.assignRoot(); // The root is assigned on the top-level array or object pop but we want to allow other json type roots in this case

        if (m_objectValue)
          m_obj.addKeyedValue();
        if (m_arrayValue)
          m_obj.addArrayValue();
      }

      Error::Type getNonTypeError() override { return Error::ObjectExpected; }
      Error::Type setValue(double number) override
      {
        m_obj.setValue(number);
        return Error::None;
      }

      Error::Type setValue(bool boolean) override
      {
        m_obj.setValue(boolean);
        return Error::None;
      }

      Error::Type setValue(std::nullptr_t) override
      {
        m_obj.setValue(nullptr);
        return Error::None;
      }

      bool allowString() override
      {
        m_obj.setString();
        return true;
      }

      void setValue(std::string str) override
      {
        m_obj.stringBufferAdd(str);
        m_obj.stringBufferEnd();
      }

      bool allowObject() override
      {
        m_obj.pushObject();
        m_needPop = true;
        return true;
      }

      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override
      {
        m_obj.setString();
        m_obj.stringBufferAdd(key);
        m_obj.addKey();
        return std::make_unique<DocumentBuilderReflector>(m_obj, true, false);
      }

      bool allowArray() override
      {
        m_obj.pushArray();
        m_needPop = true;
        return true;
      }

      std::unique_ptr<BaseReflector> getReflectorInArray() override
      {
        return std::make_unique<DocumentBuilderReflector>(m_obj, false, true);
      }

    protected:
      bool m_needPop = false;
      bool m_arrayValue = false;
      bool m_objectValue = false;
      bool m_root = false;
    };

    template <>
    class Reflector<Document> : public RefReflector<Document>
    {
    public:
      using RefReflector<Document>::m_obj;

      explicit Reflector(Document& doc)
        : RefReflector<Document>::RefReflector(doc)
        , m_builder(m_obj)
        , m_builderReflector(m_builder, false, false, true)
      {}

      static bool AllowsType(ValueType type) { return true; }
      Error::Type getNonTypeError() override { return m_builderReflector.getNonTypeError(); }
      Error::Type setValue(double number) override { return m_builderReflector.setValue(number); }
      Error::Type setValue(bool boolean) override { return m_builderReflector.setValue(boolean); }
      Error::Type setValue(std::nullptr_t) override { return m_builderReflector.setValue(nullptr); }

      bool allowString() override { return m_builderReflector.allowString(); }
      void setValue(std::string str) override { return m_builderReflector.setValue(std::move(str)); }

      bool allowObject() override { return m_builderReflector.allowObject(); }
      std::unique_ptr<BaseReflector> getReflectorForKey(std::string key) override { return m_builderReflector.getReflectorForKey(std::move(key)); }

      bool allowArray() override { return m_builderReflector.allowArray(); }
      std::unique_ptr<BaseReflector> getReflectorInArray() override { return m_builderReflector.getReflectorInArray(); }

    protected:
      DocumentBuilder m_builder;
      DocumentBuilderReflector m_builderReflector;
    };

    template <>
    class Reflector<IndependentValue> : public Reflector<IndependentValue::ValueType>
    {
    public:
      explicit Reflector(IndependentValue& obj)
        : Reflector<IndependentValue::ValueType>(obj.value)
      {}
    };

    class BaseTupleComponent
    {
    public:
      virtual std::optional<std::reference_wrapper<BaseTupleComponent>> next() = 0;
      virtual std::unique_ptr<BaseReflector> reflector() = 0;
    };

    template <typename T>
    class TupleComponentRef : public BaseTupleComponent
    {
    public:
      explicit TupleComponentRef(T& obj)
        : m_obj(obj)
      {}

      std::unique_ptr<BaseReflector> reflector() override { return std::make_unique<Reflector<T>>(m_obj); }

    protected:
      T& m_obj;
    };

    template <typename FirstType, typename... RemainingTypes>
    class RefsTuple : public TupleComponentRef<FirstType>
    {
    public:
      explicit RefsTuple(FirstType& first, RemainingTypes&... remaining)
        : TupleComponentRef<FirstType>(first)
        , m_remaining(remaining...)
      {}

      std::optional<std::reference_wrapper<BaseTupleComponent>> next() override { return m_remaining; }

    protected:
      RefsTuple<RemainingTypes...> m_remaining;
    };

    template <typename OnlyType>
    class RefsTuple<OnlyType> : public TupleComponentRef<OnlyType>
    {
    public:
      explicit RefsTuple(OnlyType& only)
        : TupleComponentRef<OnlyType>(only)
      {}

      std::optional<std::reference_wrapper<BaseTupleComponent>> next() override { return std::nullopt; }
    };

    template <typename... Types>
    class TupleReflector : public BaseReflector
    {
    public:
      explicit TupleReflector(Types&... obj)
        : m_components(obj...)
      {}

      static bool AllowsType(ValueType type) { return type == ValueType::Array; }
      Error::Type getNonTypeError() override { return Error::ArrayExpected; }
      bool allowArray() override { return true; }
      std::unique_ptr<BaseReflector> getReflectorInArray() override
      {
        m_writtenElements++;

        if (!m_current)
          m_current = m_components;
        else if (auto next = m_current->get().next())
          m_current = *next;
        else
          return std::make_unique<IgnoreReflector>();

        return m_current->get().reflector();
      }

      Error::Type complete() override
      {
        if (m_writtenElements != sizeof...(Types))
          return Error::WrongArraySize;

        return Error::None;
      }

    protected:
      size_t m_writtenElements = 0;
      std::optional<std::reference_wrapper<BaseTupleComponent>> m_current;
      RefsTuple<Types...> m_components;
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
    void stringBufferAdd(std::string_view str) override { m_str = std::string(str); }
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
    std::vector<bool> m_firstElementStack;
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
