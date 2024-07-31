#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "json5_base.hpp"

namespace json5
{

  /*

  json5::Value

  */
  class Value
  {
  public:
    // Construct null value
    Value() noexcept = default;

    // Construct null value
    explicit Value(std::nullptr_t) noexcept
      : data(TypeNull)
    {}

    // Construct boolean value
    explicit Value(bool val) noexcept
      : data(val ? TypeTrue : TypeFalse)
    {}

    // Construct number value from int (will be converted to double)
    explicit Value(int val) noexcept
      : dbl(val)
    {}

    // Construct number value from float (will be converted to double)
    explicit Value(float val) noexcept
      : dbl(val)
    {}

    // Construct number value from double
    explicit Value(double val) noexcept
      : dbl(val)
    {}

    // Return value type
    ValueType type() const noexcept;

    // Checks, if value is null
    bool isNull() const noexcept { return data == TypeNull; }

    // Checks, if value stores boolean. Use 'getBool' for reading.
    bool isBoolean() const noexcept { return data == TypeTrue || data == TypeFalse; }

    // Checks, if value stores number. Use 'get' or 'tryGet' for reading.
    bool isNumber() const noexcept { return (data & MaskNaNbits) != MaskNaNbits; }

    // Checks, if value stores string. Use 'getCStr' for reading.
    bool isString() const noexcept { return (data & MaskType) == TypeString; }

    // Checks, if value stores JSON object. Use 'ObjectView' wrapper
    // to iterate over key-value pairs (properties).
    bool isObject() const noexcept { return (data & MaskType) == TypeObject; }

    // Checks, if value stores JSON array. Use 'ArrayView' wrapper
    // to iterate over array elements.
    bool isArray() const noexcept { return (data & MaskType) == TypeArray; }

    // Get stored bool. Returns 'defaultValue', if this value is not a boolean.
    bool getBool(bool defaultValue = false) const noexcept;

    // Get stored string. Returns 'defaultValue', if this value is not a string.
    const char* getCStr(const char* defaultValue = "") const noexcept;

    // Get stored number as type 'T'. Returns 'defaultValue', if this value is not a number.
    template <typename T>
    T get(T defaultValue = 0) const noexcept
    {
      return isNumber() ? T(dbl) : defaultValue;
    }

    // Try to get stored number as type 'T'. Returns false, if this value is not a number.
    template <typename T>
    bool tryGet(T& out) const noexcept
    {
      if (!isNumber())
        return false;

      out = T(dbl);
      return true;
    }

    // Equality test against another value. Note that this might be a very expensive operation
    // for large nested JSON objects!
    bool operator==(const Value& other) const noexcept;

    // Non-equality test
    bool operator!=(const Value& other) const noexcept { return !((*this) == other); }

    // Use value as JSON object and get property value under 'key'. If this value
    // is not an object or 'key' is not found, null value is always returned.
    Value operator[](std::string_view key) const noexcept;

    // Use value as JSON array and get item at 'index'. If this value is not
    // an array or index is out of bounds, null value is returned.
    Value operator[](size_t index) const noexcept;

    // Get value payload (lower 48bits of data) converted to type 'T'
    template <typename T>
    T payload() const noexcept
    {
      return (T)(data & MaskPayload);
    }

  protected:
    Value(ValueType t, uint64_t dataIn);
    Value(ValueType t, const void* data)
      : Value(t, reinterpret_cast<uint64_t>(data))
    {}

    void relink(const class Document* prevDoc, const class Document& doc) noexcept;

    // NaN-boxed data
    union
    {
      double dbl;
      uint64_t data;
    };

    static constexpr uint64_t MaskNaNbits = 0xFFF0000000000000ull;
    static constexpr uint64_t MaskType = 0xFFFF000000000000ull;
    static constexpr uint64_t MaskPayload = 0x0000FFFFFFFFFFFFull;
    static constexpr uint64_t TypeNull = 0xFFFC000000000000ull;
    static constexpr uint64_t TypeFalse = 0xFFF1000000000000ull;
    static constexpr uint64_t TypeTrue = 0xFFF3000000000000ull;
    static constexpr uint64_t TypeString = 0xFFF2000000000000ull;
    static constexpr uint64_t TypeArray = 0xFFF4000000000000ull;
    static constexpr uint64_t TypeObject = 0xFFF6000000000000ull;

    // Stores lower 48bits of uint64 as payload
    void payload(uint64_t p) noexcept { data = (data & ~MaskPayload) | p; }

    // Stores lower 48bits of a pointer as payload
    void payload(const void* p) noexcept { payload(reinterpret_cast<uint64_t>(p)); }

    friend Document;
    friend DocumentBuilder;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /*

  json5::Document

  */
  class Document final : public Value
  {
  public:
    // Construct empty document
    Document() = default;

    // Construct a document copy
    Document(const Document& copy) { assignCopy(copy); }

    // Construct a document from r-value
    Document(Document&& rValue) noexcept { assignRvalue(std::forward<Document>(rValue)); }

    // Copy data from another document (does a deep copy)
    Document& operator=(const Document& copy)
    {
      assignCopy(copy);
      return *this;
    }

    // Assign data from r-value (does a swap)
    Document& operator=(Document&& rValue) noexcept
    {
      assignRvalue(std::forward<Document>(rValue));
      return *this;
    }

  private:
    void assignCopy(const Document& copy);
    void assignRvalue(Document&& rValue) noexcept;
    void assignRoot(Value root) noexcept;

    std::string m_strings;
    std::vector<Value> m_values;

    friend Value;
    friend DocumentBuilder;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /*

  json5::ObjectView

  */
  class ObjectView final
  {
  public:
    // Construct an empty object view
    ObjectView() noexcept = default;

    // Construct object view over a value. If the provided value does not reference a JSON object,
    // this ObjectView will be created empty (and invalid)
    explicit ObjectView(const Value& v) noexcept
      : m_pair(v.isObject() ? (v.payload<const Value*>() + 1) : nullptr)
      , m_count(m_pair ? (m_pair[-1].get<size_t>() / 2) : 0)
    {}

    // Checks, if object view was constructed from valid value
    bool isValid() const noexcept { return m_pair != nullptr; }

    using KeyValuePair = std::pair<const char*, Value>;

    class Iterator final
    {
    public:
      explicit Iterator(const Value* p = nullptr) noexcept
        : m_pair(p)
      {}
      bool operator!=(const Iterator& other) const noexcept { return m_pair != other.m_pair; }
      bool operator==(const Iterator& other) const noexcept { return m_pair == other.m_pair; }
      Iterator& operator++() noexcept
      {
        m_pair += 2;
        return *this;
      }
      KeyValuePair operator*() const noexcept { return KeyValuePair(m_pair[0].getCStr(), m_pair[1]); }

    private:
      const Value* m_pair = nullptr;
    };

    // Get an iterator to the beginning of the object (first key-value pair)
    Iterator begin() const noexcept { return Iterator(m_pair); }

    // Get an iterator to the end of the object (past the last key-value pair)
    Iterator end() const noexcept { return Iterator(m_pair + m_count * 2); }

    // Find property value with 'key'. Returns end iterator, when not found.
    Iterator find(std::string_view key) const noexcept;

    // Get number of key-value pairs
    size_t size() const noexcept { return m_count; }

    bool empty() const noexcept { return size() == 0; }
    Value operator[](std::string_view key) const noexcept;

    bool operator==(const ObjectView& other) const noexcept;
    bool operator!=(const ObjectView& other) const noexcept { return !((*this) == other); }

  private:
    const Value* m_pair = nullptr;
    size_t m_count = 0;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /*

  json5::ArrayView

  */
  class ArrayView final
  {
  public:
    // Construct an empty array view
    ArrayView() noexcept = default;

    // Construct array view over a value. If the provided value does not reference a JSON array,
    // this ArrayView will be created empty (and invalid)
    explicit ArrayView(const Value& v) noexcept
      : m_value(v.isArray() ? (v.payload<const Value*>() + 1) : nullptr)
      , m_count(m_value ? m_value[-1].get<size_t>() : 0)
    {}

    // Checks, if array view was constructed from valid value
    bool isValid() const noexcept { return m_value != nullptr; }

    using iterator = const Value*;

    iterator begin() const noexcept { return m_value; }
    iterator end() const noexcept { return m_value + m_count; }
    size_t size() const noexcept { return m_count; }
    bool empty() const noexcept { return m_count == 0; }
    Value operator[](size_t index) const noexcept;

    bool operator==(const ArrayView& other) const noexcept;
    bool operator!=(const ArrayView& other) const noexcept { return !((*this) == other); }

  private:
    const Value* m_value = nullptr;
    size_t m_count = 0;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  inline Value::Value(ValueType t, uint64_t dataIn)
  {
    if (t == ValueType::Object)
      data = TypeObject | dataIn;
    else if (t == ValueType::Array)
      data = TypeArray | dataIn;
    else if (t == ValueType::String)
      data = TypeString | dataIn;
    else
      data = TypeNull;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline ValueType Value::type() const noexcept
  {
    if ((data & MaskNaNbits) != MaskNaNbits)
      return ValueType::Number;

    if ((data & MaskType) == TypeObject)
      return ValueType::Object;
    else if ((data & MaskType) == TypeArray)
      return ValueType::Array;
    else if ((data & MaskType) == TypeString)
      return ValueType::String;
    if (data == TypeTrue || data == TypeFalse)
      return ValueType::Boolean;

    return ValueType::Null;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline bool Value::getBool(bool defaultValue) const noexcept
  {
    if (data == TypeTrue)
      return true;
    else if (data == TypeFalse)
      return false;

    return defaultValue;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline const char* Value::getCStr(const char* defaultValue) const noexcept
  {
    return isString() ? payload<const char*>() : defaultValue;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline bool Value::operator==(const Value& other) const noexcept
  {
    if (auto t = type(); t == other.type())
    {
      if (t == ValueType::Null)
        return true;
      else if (t == ValueType::Boolean)
        return data == other.data;
      else if (t == ValueType::Number)
        return dbl == other.dbl;
      else if (t == ValueType::String)
        return std::string_view(payload<const char*>()) == std::string_view(other.payload<const char*>());
      else if (t == ValueType::Array)
        return ArrayView(*this) == ArrayView(other);
      else if (t == ValueType::Object)
        return ObjectView(*this) == ObjectView(other);
    }

    return false;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Value Value::operator[](std::string_view key) const noexcept
  {
    if (!isObject())
      return Value();

    ObjectView ov(*this);
    return ov[key];
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Value Value::operator[](size_t index) const noexcept
  {
    if (!isArray())
      return Value();

    ArrayView av(*this);
    return av[index];
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Value::relink(const class Document* prevDoc, const class Document& doc) noexcept
  {
    if (isString())
    {
      if (prevDoc)
        payload(payload<const char*>() - prevDoc->m_strings.data());

      payload(doc.m_strings.data() + payload<uint64_t>());
    }
    else if (isObject() || isArray())
    {
      if (prevDoc)
        payload(payload<const Value*>() - prevDoc->m_values.data());

      payload(doc.m_values.data() + payload<uint64_t>());
    }
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Document::assignCopy(const Document& copy)
  {
    data = copy.data;
    m_strings = copy.m_strings;
    m_values = copy.m_values;

    for (auto& v : m_values)
      v.relink(&copy, *this);

    relink(&copy, *this);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Document::assignRvalue(Document&& rValue) noexcept
  {
    data = std::move(rValue.data);
    m_strings = std::move(rValue.m_strings);
    m_values = std::move(rValue.m_values);

    for (auto& v : m_values)
      v.relink(&rValue, *this);

    for (auto& v : rValue.m_values)
      v.relink(this, rValue);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Document::assignRoot(Value root) noexcept
  {
    data = root.data;

    for (auto& v : m_values)
      v.relink(nullptr, *this);

    relink(nullptr, *this);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline ObjectView::Iterator ObjectView::find(std::string_view key) const noexcept
  {
    if (!key.empty())
    {
      for (auto iter = begin(); iter != end(); ++iter)
        if (key == (*iter).first)
          return iter;
    }

    return end();
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Value ObjectView::operator[](std::string_view key) const noexcept
  {
    const auto iter = find(key);
    return (iter != end()) ? (*iter).second : Value();
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline bool ObjectView::operator==(const ObjectView& other) const noexcept
  {
    if (size() != other.size())
      return false;

    if (empty())
      return true;

    static constexpr size_t StackPairCount = 256;
    KeyValuePair tempPairs1[StackPairCount];
    KeyValuePair tempPairs2[StackPairCount];
    KeyValuePair* pairs1 = m_count <= StackPairCount ? tempPairs1 : new KeyValuePair[m_count];
    KeyValuePair* pairs2 = m_count <= StackPairCount ? tempPairs2 : new KeyValuePair[m_count];
    {
      size_t i = 0;
      for (const auto kvp : *this) pairs1[i++] = kvp;
    }
    {
      size_t i = 0;
      for (const auto kvp : other) pairs2[i++] = kvp;
    }

    const auto comp = [](const KeyValuePair& a, const KeyValuePair& b) noexcept -> bool {
      return strcmp(a.first, b.first) < 0;
    };

    std::sort(pairs1, pairs1 + m_count, comp);
    std::sort(pairs2, pairs2 + m_count, comp);

    bool result = true;
    for (size_t i = 0; i < m_count; ++i)
    {
      if (strcmp(pairs1[i].first, pairs2[i].first) || pairs1[i].second != pairs2[i].second)
      {
        result = false;
        break;
      }
    }

    if (m_count > StackPairCount)
    {
      delete[] pairs1;
      delete[] pairs2;
    }
    return result;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Value ArrayView::operator[](size_t index) const noexcept
  {
    return (index < m_count) ? m_value[index] : Value();
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline bool ArrayView::operator==(const ArrayView& other) const noexcept
  {
    if (size() != other.size())
      return false;

    auto iter = begin();
    for (const auto& v : other)
      if (*iter++ != v)
        return false;

    return true;
  }

} // namespace json5
