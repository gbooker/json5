#pragma once

#include "json5.hpp"

namespace json5
{

  class Builder
  {
  public:
    explicit Builder(Document& doc)
      : m_doc(doc)
    {}

    const Document& doc() const noexcept { return m_doc; }

    detail::StringOffset stringBufferOffset() const noexcept;
    detail::StringOffset stringBufferAdd(std::string_view str);
    void stringBufferAdd(char ch) { m_doc.m_strings.push_back(ch); }
    void stringBufferAddUtf8(uint32_t ch);

    Value newString(detail::StringOffset stringOffset) { return Value(ValueType::String, stringOffset); }
    Value newString(std::string_view str) { return newString(stringBufferAdd(str)); }

    void pushObject();
    void pushArray();
    Value pop();

    Builder& operator+=(Value v);
    Value& operator[](detail::StringOffset keyOffset);
    Value& operator[](std::string_view key) { return (*this)[stringBufferAdd(key)]; }

  protected:
    void reset() noexcept;

    Document& m_doc;
    std::vector<Value> m_stack;
    std::vector<Value> m_values;
    std::vector<size_t> m_counts;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  inline detail::StringOffset Builder::stringBufferOffset() const noexcept
  {
    return detail::StringOffset(m_doc.m_strings.size());
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline detail::StringOffset Builder::stringBufferAdd(std::string_view str)
  {
    auto offset = stringBufferOffset();
    m_doc.m_strings += str;
    m_doc.m_strings.push_back(0);
    return offset;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::stringBufferAddUtf8(uint32_t ch)
  {
    auto& s = m_doc.m_strings;

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

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::pushObject()
  {
    auto v = Value(ValueType::Object, nullptr);
    m_stack.emplace_back(v);
    m_counts.push_back(0);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::pushArray()
  {
    auto v = Value(ValueType::Array, nullptr);
    m_stack.emplace_back(v);
    m_counts.push_back(0);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Value Builder::pop()
  {
    auto result = m_stack.back();
    auto count = m_counts.back();

    result.payload(m_doc.m_values.size());

    m_doc.m_values.push_back(Value(double(count)));

    auto startIndex = m_values.size() - count;
    for (size_t i = startIndex, s = m_values.size(); i < s; ++i)
      m_doc.m_values.push_back(m_values[i]);

    m_values.resize(m_values.size() - count);

    m_stack.pop_back();
    m_counts.pop_back();

    if (m_stack.empty())
    {
      m_doc.assignRoot(result);
      result = m_doc;
    }

    return result;
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

  //---------------------------------------------------------------------------------------------------------------------
  inline void Builder::reset() noexcept
  {
    m_doc.data = Value::TypeNull;
    m_doc.m_values.clear();
    m_doc.m_strings.clear();
    m_doc.m_strings.push_back(0);
  }

} // namespace json5
