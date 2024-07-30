#pragma once

#include "json5.hpp"

namespace json5
{

  //---------------------------------------------------------------------------------------------------------------------
  static inline void StringBufferAddUtf8(std::string& s, uint32_t ch)
  {
    if (0 <= ch && ch <= 0x7f)
    {
      s += char(ch);
    }
    else if (0x80 <= ch && ch <= 0x7ff)
    {
      s += char(0xc0 | (ch >> 6));
      s += char(0x80 | (ch & 0x3f));
    }
    else if (0x800 <= ch && ch <= 0xffff)
    {
      s += char(0xe0 | (ch >> 12));
      s += char(0x80 | ((ch >> 6) & 0x3f));
      s += char(0x80 | (ch & 0x3f));
    }
    else if (0x10000 <= ch && ch <= 0x1fffff)
    {
      s += char(0xf0 | (ch >> 18));
      s += char(0x80 | ((ch >> 12) & 0x3f));
      s += char(0x80 | ((ch >> 6) & 0x3f));
      s += char(0x80 | (ch & 0x3f));
    }
    else if (0x200000 <= ch && ch <= 0x3ffffff)
    {
      s += char(0xf8 | (ch >> 24));
      s += char(0x80 | ((ch >> 18) & 0x3f));
      s += char(0x80 | ((ch >> 12) & 0x3f));
      s += char(0x80 | ((ch >> 6) & 0x3f));
      s += char(0x80 | (ch & 0x3f));
    }
    else if (0x4000000 <= ch && ch <= 0x7fffffff)
    {
      s += char(0xfc | (ch >> 30));
      s += char(0x80 | ((ch >> 24) & 0x3f));
      s += char(0x80 | ((ch >> 18) & 0x3f));
      s += char(0x80 | ((ch >> 12) & 0x3f));
      s += char(0x80 | ((ch >> 6) & 0x3f));
      s += char(0x80 | (ch & 0x3f));
    }
  }

  class Builder
  {
  public:
    virtual Error::Type setValue(double number) = 0;
    virtual Error::Type setValue(bool boolean) = 0;
    virtual Error::Type setValue(std::nullptr_t) = 0;

    virtual Error::Type setString() = 0;

    virtual void stringBufferAdd(char ch) = 0;
    virtual void stringBufferAdd(std::string_view str) = 0;
    virtual void stringBufferAddUtf8(uint32_t ch) = 0;
    virtual void stringBufferEnd() = 0;

    virtual Error::Type pushObject() = 0;
    virtual Error::Type pushArray() = 0;
    virtual Error::Type pop() = 0;

    virtual void addKey() = 0;
    virtual void addKeyedValue() = 0;
    virtual void beginArrayValue() = 0;
    virtual void addArrayValue() = 0;

    virtual bool isValidRoot() = 0;
  };

  class DocumentBuilder : public Builder
  {
  public:
    explicit DocumentBuilder(Document& doc)
      : m_doc(doc)
    {}

    Error::Type setValue(double number) override
    {
      m_currentValue = Value(number);
      return Error::None;
    }

    Error::Type setValue(bool boolean) override
    {
      m_currentValue = Value(boolean);
      return Error::None;
    }

    Error::Type setValue(std::nullptr_t) override
    {
      m_currentValue = Value(nullptr);
      return Error::None;
    }

    Error::Type setString() override
    {
      m_currentValue = Value(ValueType::String, m_doc.m_strings.size());
      return Error::None;
    }

    void stringBufferAdd(char ch) override { m_doc.m_strings.push_back(ch); }
    void stringBufferAdd(std::string_view str) override
    {
      m_doc.m_strings += str;
      m_doc.m_strings.push_back(0);
    }

    void stringBufferAddUtf8(uint32_t ch) override { StringBufferAddUtf8(m_doc.m_strings, ch); }
    void stringBufferEnd() override { m_doc.m_strings.push_back(0); }

    Error::Type pushObject() override
    {
      auto v = Value(ValueType::Object, nullptr);
      m_stack.emplace_back(v);
      m_counts.push_back(0);
      return Error::None;
    }

    Error::Type pushArray() override
    {
      auto v = Value(ValueType::Array, nullptr);
      m_stack.emplace_back(v);
      m_counts.push_back(0);
      return Error::None;
    }

    Error::Type pop() override
    {
      m_currentValue = m_stack.back();
      auto count = m_counts.back();

      m_currentValue.payload(m_doc.m_values.size());

      m_doc.m_values.push_back(Value(double(count)));

      auto startIndex = m_values.size() - count;
      for (size_t i = startIndex, s = m_values.size(); i < s; ++i)
        m_doc.m_values.push_back(m_values[i]);

      m_values.resize(m_values.size() - count);

      m_stack.pop_back();
      m_counts.pop_back();

      if (m_stack.empty())
      {
        m_doc.assignRoot(m_currentValue);
        m_currentValue = m_doc;
      }

      return Error::None;
    }

    void addKey() override
    {
      m_values.push_back(m_currentValue);
      m_counts.back()++;
    }

    void addKeyedValue() override
    {
      m_values.push_back(m_currentValue);
      m_counts.back()++;
    }

    void beginArrayValue() override {}

    void addArrayValue() override
    {
      m_values.push_back(m_currentValue);
      m_counts.back()++;
    }

    bool isValidRoot() override
    {
      return m_doc.isObject() || m_doc.isArray();
    }

    // Used by tests
    Value getCurrentValue() const noexcept { return m_currentValue; }
    detail::StringOffset stringBufferOffset() const noexcept { return detail::StringOffset(m_doc.m_strings.size()); }
    detail::StringOffset stringBufferAddByOffset(std::string_view str)
    {
      auto offset = stringBufferOffset();
      m_doc.m_strings += str;
      m_doc.m_strings.push_back(0);
      return offset;
    }

    Value newString(detail::StringOffset stringOffset) { return Value(ValueType::String, stringOffset); }
    Value newString(std::string_view str) { return newString(stringBufferAddByOffset(str)); }

    Builder& operator+=(Value v)
    {
      m_values.push_back(v);
      m_counts.back() += 1;
      return *this;
    }

    Value& operator[](detail::StringOffset keyOffset)
    {
      m_values.push_back(newString(keyOffset));
      m_counts.back() += 2;
      return m_values.emplace_back();
    }

    Value& operator[](std::string_view key) { return (*this)[stringBufferAddByOffset(key)]; }

  protected:
    Document& m_doc;
    Value m_currentValue;
    std::vector<Value> m_stack;
    std::vector<Value> m_values;
    std::vector<size_t> m_counts;
  };

  class IndependentValueBuilder : public Builder
  {
  public:
    explicit IndependentValueBuilder(IndependentValue& root)
      : m_currentValue(root)
    {
      m_currentKey.value = "";
    }

    Error::Type setValue(double number) override
    {
      m_currentValue.get().value = number;
      return Error::None;
    }

    Error::Type setValue(bool boolean) override
    {
      m_currentValue.get().value = boolean;
      return Error::None;
    }

    Error::Type setValue(std::nullptr_t) override
    {
      m_currentValue.get().value = std::monostate();
      return Error::None;
    }

    Error::Type setString() override
    {
      m_currentValue.get().value = "";
      return Error::None;
    }

    void stringBufferAdd(char ch) override { std::get<std::string>(m_currentValue.get().value).push_back(ch); }
    void stringBufferAdd(std::string_view str) override { std::get<std::string>(m_currentValue.get().value) = str; }
    void stringBufferAddUtf8(uint32_t ch) override { StringBufferAddUtf8(std::get<std::string>(m_currentValue.get().value), ch); }
    void stringBufferEnd() override {}

    Error::Type pushObject() override
    {
      m_currentValue.get().value = IndependentValue::Map();
      m_stack.push_back(m_currentValue);
      std::get<std::string>(m_currentKey.value).clear();
      m_currentValue = m_currentKey;
      return Error::None;
    }

    Error::Type pushArray() override
    {
      m_currentValue.get().value = IndependentValue::Array();
      m_stack.push_back(m_currentValue);
      return Error::None;
    }

    Error::Type pop() override
    {
      m_currentValue = m_stack.back();
      m_stack.pop_back();

      return Error::None;
    }

    void addKey() override { m_currentValue = std::get<IndependentValue::Map>(m_stack.back().get().value).insert({std::move(std::get<std::string>(m_currentKey.value)), {}}).first->second; }
    void addKeyedValue() override
    {
      std::get<std::string>(m_currentKey.value).clear();
      m_currentValue = m_currentKey;
    }

    void beginArrayValue() override
    {
      auto& array = std::get<IndependentValue::Array>(m_stack.back().get().value);
      array.push_back({});
      m_currentValue = array.back();
    }

    void addArrayValue() override {}

    bool isValidRoot() override
    {
      return std::get_if<IndependentValue::Map>(&m_currentValue.get().value) != nullptr || std::get_if<IndependentValue::Array>(&m_currentValue.get().value) != nullptr;
    }

  protected:
    IndependentValue m_currentKey;
    std::reference_wrapper<IndependentValue> m_currentValue;
    std::vector<std::reference_wrapper<IndependentValue>> m_stack;
  };
} // namespace json5
