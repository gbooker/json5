#pragma once

#include <fstream>
#include <iomanip>
#include <math.h>
#include <sstream>

#include "json5.hpp"

namespace json5
{
  class Writer
  {
  public:
    virtual void writeNull() = 0;
    virtual void writeBoolean(bool boolean) = 0;
    virtual void writeNumber(double number) = 0;
    virtual void writeString(const char* str) = 0;
    virtual void writeString(std::string_view str) = 0;

    virtual void beginArray() = 0;
    virtual void beginArrayElement() = 0;
    virtual void endArray() = 0;
    virtual void writeEmptyArray() = 0;

    virtual void beginObject() = 0;
    virtual void beginObjectElement() = 0;
    virtual void writeObjectKey(const char* str) = 0;
    virtual void writeObjectKey(std::string_view str) = 0;
    virtual void endObject() = 0;
    virtual void writeEmptyObject() = 0;

    virtual void complete() = 0;
  };

  // Writes json5::document into stream
  void ToStream(std::ostream& os, const Document& doc, const WriterParams& wp = WriterParams());

  // Converts json5::document to string
  void ToString(std::string& str, const Document& doc, const WriterParams& wp = WriterParams());

  // Returns json5::document converted to string
  std::string ToString(const Document& doc, const WriterParams& wp = WriterParams());

  // Write json5::document into file, returns 'true' on success
  bool ToFile(const std::string& fileName, const Document& doc, const WriterParams& wp = WriterParams());

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  class Json5Writer : public Writer
  {
  public:
    Json5Writer(std::ostream& os, const WriterParams& wp)
      : m_os(os)
      , m_wp(wp)
    {
      m_eol = wp.eol;

      if (wp.compact)
      {
        m_depth = -1;
        m_kvSeparator = ":";
        m_eol = "";
      }
    }

    void writeNull() override { m_os << "null"; }
    void writeBoolean(bool boolean) override { m_os << (boolean ? "true" : "false"); }
    void writeNumber(double number) override
    {
      if (double _; modf(number, &_) == 0.0)
      {
        m_os << (int64_t)number;
      }
      else if (isnan(number))
      {
        // JSON cannot express NaN so we use null instead to be consistent with JS
        if (m_wp.compact)
          m_os << "null";
        else
          m_os << "NaN";
      }
      else
      {
        m_os << number;
      }
    }

    void writeString(const char* str) override
    {
      writeString(str, std::numeric_limits<size_t>::max(), '"', m_wp.escapeUnicode);
    }

    void writeString(std::string_view str) override
    {
      writeString(str.data(), str.length(), '"', m_wp.escapeUnicode);
    }

    void writeString(const char* str, size_t len, char quotes, bool escapeUnicode)
    {
      if (quotes)
        m_os << quotes;

      auto iterate = [&len](char ch) -> bool {
        if (len == std::numeric_limits<size_t>::max())
          return ch;

        return len-- > 0;
      };

      while (iterate(*str))
      {
        bool advance = true;

        if (str[0] == '\n')
          m_os << "\\n";
        else if (str[0] == '\r')
          m_os << "\\r";
        else if (str[0] == '\t')
          m_os << "\\t";
        else if (str[0] == '"' && quotes == '"')
          m_os << "\\\"";
        else if (str[0] == '\'' && quotes == '\'')
          m_os << "\\'";
        else if (str[0] == '\\')
          m_os << "\\\\";
        else if (uint8_t(str[0]) >= 128 && escapeUnicode)
        {
          uint32_t ch = 0;

          if ((*str & 0b1110'0000u) == 0b1100'0000u)
          {
            ch |= ((*str++) & 0b0001'1111u) << 6;
            ch |= ((*str++) & 0b0011'1111u);
          }
          else if ((*str & 0b1111'0000u) == 0b1110'0000u)
          {
            ch |= ((*str++) & 0b0000'1111u) << 12;
            ch |= ((*str++) & 0b0011'1111u) << 6;
            ch |= ((*str++) & 0b0011'1111u);
          }
          else if ((*str & 0b1111'1000u) == 0b1111'0000u)
          {
            ch |= ((*str++) & 0b0000'0111u) << 18;
            ch |= ((*str++) & 0b0011'1111u) << 12;
            ch |= ((*str++) & 0b0011'1111u) << 6;
            ch |= ((*str++) & 0b0011'1111u);
          }
          else if ((*str & 0b1111'1100u) == 0b1111'1000u)
          {
            ch |= ((*str++) & 0b0000'0011u) << 24;
            ch |= ((*str++) & 0b0011'1111u) << 18;
            ch |= ((*str++) & 0b0011'1111u) << 12;
            ch |= ((*str++) & 0b0011'1111u) << 6;
            ch |= ((*str++) & 0b0011'1111u);
          }
          else if ((*str & 0b1111'1110u) == 0b1111'1100u)
          {
            ch |= ((*str++) & 0b0000'0001u) << 30;
            ch |= ((*str++) & 0b0011'1111u) << 24;
            ch |= ((*str++) & 0b0011'1111u) << 18;
            ch |= ((*str++) & 0b0011'1111u) << 12;
            ch |= ((*str++) & 0b0011'1111u) << 6;
            ch |= ((*str++) & 0b0011'1111u);
          }

          if (ch <= std::numeric_limits<uint16_t>::max())
          {
            m_os << "\\u" << std::hex << std::setfill('0') << std::setw(4) << ch;
          }
          else
            m_os << "?"; // JSON can't encode Unicode chars > 65535 (emojis)

          advance = false;
        }
        else
          m_os << *str;

        if (advance)
          ++str;
      }

      if (quotes)
        m_os << quotes;
    }

    void push()
    {
      m_firstElementStack.push_back(m_firstElement);
      m_firstElement = true;
      if (m_depth != -1)
        m_depth++;
    }

    void pop()
    {
      m_firstElement = m_firstElementStack.back();
      m_firstElementStack.pop_back();
      if (m_depth != -1)
        m_depth--;
    }

    void writeSeparatorAndIndent()
    {
      if (m_firstElement)
        m_firstElement = false;
      else
        m_os << ",";

      m_os << m_eol;
      indent();
    }

    void indent()
    {
      for (int i = 0; i < m_depth; ++i) m_os << m_wp.indentation;
    }

    void beginArray() override
    {
      push();
      m_os << "[";
    }

    void beginArrayElement() override { writeSeparatorAndIndent(); }

    void endArray() override
    {
      m_os << m_eol;
      pop();
      indent();
      m_os << "]";
    }

    void writeEmptyArray() override { m_os << "[]"; }

    void beginObject() override
    {
      push();
      m_os << "{";
    }

    void writeObjectKey(const char* str) override
    {
      writeObjectKey(str, std::numeric_limits<size_t>::max());
    }

    void writeObjectKey(std::string_view str) override
    {
      writeObjectKey(str.data(), str.length());
    }

    void writeObjectKey(const char* str, size_t len)
    {
      if (m_wp.jsonCompatible)
      {
        writeString(str, len, '"', m_wp.escapeUnicode);
        m_os << m_kvSeparator;
      }
      else if (len != std::numeric_limits<size_t>::max())
      {
        m_os << std::string_view(str, len) << m_kvSeparator;
      }
      else
      {
        m_os << str << m_kvSeparator;
      }
    }

    void endObject() override
    {
      m_os << m_eol;
      pop();
      indent();
      m_os << "}";
    }

    void beginObjectElement() override { writeSeparatorAndIndent(); }
    void writeEmptyObject() override { m_os << "{}"; }

    void complete() override { m_os << m_eol; }

  protected:
    std::ostream& m_os;
    const WriterParams& m_wp;

    bool m_firstElement = false;
    vector<bool> m_firstElementStack;
    int m_depth = 0;
    const char* m_kvSeparator = ": ";
    const char* m_eol;
  };

  //---------------------------------------------------------------------------------------------------------------------
  inline void Write(Writer& writer, const Value& v)
  {
    if (v.isNull())
    {
      writer.writeNull();
    }
    else if (v.isBoolean())
    {
      writer.writeBoolean(v.getBool());
    }
    else if (v.isNumber())
    {
      writer.writeNumber(v.get<double>());
    }
    else if (v.isString())
    {
      writer.writeString(v.getCStr());
    }
    else if (v.isArray())
    {
      if (auto av = json5::ArrayView(v); !av.empty())
      {
        writer.beginArray();
        for (size_t i = 0, s = av.size(); i < s; ++i)
        {
          writer.beginArrayElement();
          Write(writer, av[i]);
        }

        writer.endArray();
      }
      else
      {
        writer.writeEmptyArray();
      }
    }
    else if (v.isObject())
    {
      if (auto ov = json5::ObjectView(v); !ov.empty())
      {
        writer.beginObject();
        for (auto kvp : ov)
        {
          writer.beginObjectElement();
          writer.writeObjectKey(kvp.first);
          Write(writer, kvp.second);
        }

        writer.endObject();
      }
      else
      {
        writer.writeEmptyObject();
      }
    }
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void ToStream(std::ostream& os, const Document& doc, const WriterParams& wp)
  {
    Json5Writer writer(os, wp);
    Write(writer, doc);
    writer.complete();
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void ToString(std::string& str, const Document& doc, const WriterParams& wp)
  {
    std::ostringstream os;
    ToStream(os, doc, wp);
    str = os.str();
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline std::string ToString(const Document& doc, const WriterParams& wp)
  {
    std::string result;
    ToString(result, doc, wp);
    return result;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline bool ToFile(const std::string& fileName, const Document& doc, const WriterParams& wp)
  {
    std::ofstream ofs(fileName);
    if (!ofs.is_open())
      return false;

    ToStream(ofs, doc, wp);
    return true;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void ToStream(std::ostream& os, const Error& err)
  {
    os << err.TypeString[err.type] << " at " << err.line << ":" << err.column;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline std::string ToString(const Error& err)
  {
    std::ostringstream os;
    ToStream(os, err);
    return os.str();
  }

} // namespace json5
