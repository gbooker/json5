#pragma once

#include <fstream>
#include <iomanip>
#include <sstream>

#include "json5.hpp"

namespace json5
{

  // Writes json5::document into stream
  void ToStream(std::ostream& os, const Document& doc, const WriterParams& wp = WriterParams());

  // Converts json5::document to string
  void ToString(std::string& str, const Document& doc, const WriterParams& wp = WriterParams());

  // Returns json5::document converted to string
  std::string ToString(const Document& doc, const WriterParams& wp = WriterParams());

  // Write json5::document into file, returns 'true' on success
  bool ToFile(const std::string& fileName, const Document& doc, const WriterParams& wp = WriterParams());

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  inline void ToStream(std::ostream& os, const char* str, char quotes, bool escapeUnicode)
  {
    if (quotes)
      os << quotes;

    while (*str)
    {
      bool advance = true;

      if (str[0] == '\n')
        os << "\\n";
      else if (str[0] == '\r')
        os << "\\r";
      else if (str[0] == '\t')
        os << "\\t";
      else if (str[0] == '"' && quotes == '"')
        os << "\\\"";
      else if (str[0] == '\'' && quotes == '\'')
        os << "\\'";
      else if (str[0] == '\\')
        os << "\\\\";
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
          os << "\\u" << std::hex << std::setfill('0') << std::setw(4) << ch;
        }
        else
          os << "?"; // JSON can't encode Unicode chars > 65535 (emojis)

        advance = false;
      }
      else
        os << *str;

      if (advance)
        ++str;
    }

    if (quotes)
      os << quotes;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void ToStream(std::ostream& os, const Value& v, const WriterParams& wp, int depth)
  {
    const char* kvSeparator = ": ";
    const char* eol = wp.eol;

    if (wp.compact)
    {
      depth = -1;
      kvSeparator = ":";
      eol = "";
    }

    if (v.isNull())
      os << "null";
    else if (v.isBoolean())
      os << (v.getBool() ? "true" : "false");
    else if (v.isNumber())
    {
      if (double _, d = v.get<double>(); modf(d, &_) == 0.0)
        os << v.get<int64_t>();
      else
        os << d;
    }
    else if (v.isString())
    {
      ToStream(os, v.getCStr(), '"', wp.escapeUnicode);
    }
    else if (v.isArray())
    {
      if (auto av = json5::ArrayView(v); !av.empty())
      {
        os << "[" << eol;
        for (size_t i = 0, s = av.size(); i < s; ++i)
        {
          for (int i = 0; i <= depth; ++i) os << wp.indentation;
          ToStream(os, av[i], wp, depth + 1);
          if (i < s - 1)
            os << ",";
          os << eol;
        }

        for (int i = 0; i < depth; ++i) os << wp.indentation;
        os << "]";
      }
      else
        os << "[]";
    }
    else if (v.isObject())
    {
      if (auto ov = json5::ObjectView(v); !ov.empty())
      {
        os << "{" << eol;
        size_t count = ov.size();
        for (auto kvp : ov)
        {
          for (int i = 0; i <= depth; ++i) os << wp.indentation;

          if (wp.jsonCompatible)
          {
            ToStream(os, kvp.first, '"', wp.escapeUnicode);
            os << kvSeparator;
          }
          else
            os << kvp.first << kvSeparator;

          ToStream(os, kvp.second, wp, depth + 1);
          if (--count)
            os << ",";
          os << eol;
        }

        for (int i = 0; i < depth; ++i) os << wp.indentation;
        os << "}";
      }
      else
        os << "{}";
    }

    if (!depth)
      os << eol;
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline void ToStream(std::ostream& os, const Document& doc, const WriterParams& wp)
  {
    ToStream(os, doc, wp, 0);
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
