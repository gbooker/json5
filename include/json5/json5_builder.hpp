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
    explicit Builder(Document& doc)
      : m_doc(doc)
    {}

    Error::Type setValue(double number);
    Error::Type setValue(bool boolean);
    Error::Type setValue(std::nullptr_t);

    Error::Type setString();

    void stringBufferAdd(char ch) { m_doc.m_strings.push_back(ch); }
    void stringBufferAdd(std::string_view str);
    void stringBufferAddUtf8(uint32_t ch);
    void stringBufferEnd();

    Error::Type pushObject();
    Error::Type pushArray();
    Error::Type pop();

    void addKey();
    void addKeyedValue();
    void beginArrayValue() {}
    void addArrayValue();

    bool isValidRoot() const;

    // Used by tests
    Value getCurrentValue() const noexcept { return m_currentValue; }
    detail::StringOffset stringBufferOffset() const noexcept;
    detail::StringOffset stringBufferAddByOffset(std::string_view str);
    Value newString(detail::StringOffset stringOffset) { return Value(ValueType::String, stringOffset); }
    Value newString(std::string_view str) { return newString(stringBufferAddByOffset(str)); }

    Builder& operator+=(Value v);
    Value& operator[](detail::StringOffset keyOffset);
    Value& operator[](std::string_view key) { return (*this)[stringBufferAddByOffset(key)]; }

  protected:
    Document& m_doc;
    Value m_currentValue;
    std::vector<Value> m_stack;
    std::vector<Value> m_values;
    std::vector<size_t> m_counts;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  inline Error::Type Builder::setValue(double number)
  {
    m_currentValue = Value(number);
    return Error::None;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error::Type Builder::setValue(bool boolean)
  {
    m_currentValue = Value(boolean);
    return Error::None;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error::Type Builder::setValue(std::nullptr_t)
  {
    m_currentValue = Value(nullptr);
    return Error::None;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error::Type Builder::setString()
  {
    m_currentValue = Value(ValueType::String, m_doc.m_strings.size());
    return Error::None;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::stringBufferAdd(std::string_view str)
  {
    m_doc.m_strings += str;
    m_doc.m_strings.push_back(0);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::stringBufferAddUtf8(uint32_t ch)
  {
    StringBufferAddUtf8(m_doc.m_strings, ch);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::stringBufferEnd()
  {
    m_doc.m_strings.push_back(0);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error::Type Builder::pushObject()
  {
    auto v = Value(ValueType::Object, nullptr);
    m_stack.emplace_back(v);
    m_counts.push_back(0);
    return Error::None;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error::Type Builder::pushArray()
  {
    auto v = Value(ValueType::Array, nullptr);
    m_stack.emplace_back(v);
    m_counts.push_back(0);
    return Error::None;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error::Type Builder::pop()
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

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::addKey()
  {
    m_values.push_back(m_currentValue);
    m_counts.back()++;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::addKeyedValue()
  {
    m_values.push_back(m_currentValue);
    m_counts.back()++;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::addArrayValue()
  {
    m_values.push_back(m_currentValue);
    m_counts.back()++;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline bool Builder::isValidRoot() const
  {
    return m_doc.isObject() || m_doc.isArray();
   }

  //---------------------------------------------------------------------------------------------------------------------
  inline detail::StringOffset Builder::stringBufferOffset() const noexcept
  {
    return detail::StringOffset(m_doc.m_strings.size());
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline detail::StringOffset Builder::stringBufferAddByOffset(std::string_view str)
  {
    auto offset = stringBufferOffset();
    m_doc.m_strings += str;
    m_doc.m_strings.push_back(0);
    return offset;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Builder& Builder::operator+=(Value v)
  {
    m_values.push_back(v);
    m_counts.back() += 1;
    return *this;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Value& Builder::operator[](detail::StringOffset keyOffset)
  {
    m_values.push_back(newString(keyOffset));
    m_counts.back() += 2;
    return m_values.emplace_back();
  }

} // namespace json5
