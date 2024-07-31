#pragma once

#include <charconv>
#include <fstream>
#include <sstream>

#include "json5_builder.hpp"

namespace json5
{

  // Parse json5::Builder from stream
  Error FromStream(std::istream& is, Builder& builder);

  // Parse json5::Builder from string
  Error FromString(std::string_view str, Builder& builder);

  // Parse json5::Builder from file
  Error FromFile(std::string_view fileName, Builder& builder);

  // Parse json5::Document
  Error FromStream(std::istream& is, Document& doc);
  Error FromString(std::string_view str, Document& doc);
  Error FromFile(std::string_view fileName, Document& doc);

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  class Parser final
  {
  public:
    Parser(Builder& builder, detail::CharSource& chars)
      : m_builder(builder)
      , m_chars(chars)
    {}

    Error parse();

  private:
    int next() { return m_chars.next(); }
    int peek() { return m_chars.peek(); }
    bool eof() const { return m_chars.eof(); }
    Error makeError(int type) const noexcept { return m_chars.makeError(type); }

    enum class TokenType
    {
      Unknown,
      Identifier,
      String,
      Number,
      Colon,
      Comma,
      ObjectBegin,
      ObjectEnd,
      ArrayBegin,
      ArrayEnd,
      LiteralTrue,
      LiteralFalse,
      LiteralNull
    };

    Error parseValue();
    Error parseObject();
    Error parseArray();
    Error peekNextToken(TokenType& result);
    Error parseNumber(double& result);
    Error parseString();
    Error parseIdentifier();
    Error parseLiteral(TokenType& result);

    Builder& m_builder;
    detail::CharSource& m_chars;
  };

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  namespace detail
  {

    //---------------------------------------------------------------------------------------------------------------------
    class StlIstream : public CharSource
    {
    public:
      explicit StlIstream(std::istream& is)
        : m_is(is)
      {}

      int next() override
      {
        if (m_is.peek() == '\n')
        {
          m_column = 0;
          ++m_line;
        }

        ++m_column;
        return m_is.get();
      }

      int peek() override { return m_is.peek(); }

      bool eof() const override { return m_is.eof() || m_is.fail(); }

    protected:
      std::istream& m_is;
    };

    //---------------------------------------------------------------------------------------------------------------------
    class MemoryBlock : public CharSource
    {
    public:
      MemoryBlock(const void* ptr, size_t size)
        : m_cursor(reinterpret_cast<const char*>(ptr))
        , m_size(ptr ? size : 0)
      {
      }

      int next() override
      {
        if (m_size == 0)
          return -1;

        int ch = uint8_t(*m_cursor++);

        if (ch == '\n')
        {
          m_column = 0;
          ++m_line;
        }

        ++m_column;
        --m_size;
        return ch;
      }

      int peek() override
      {
        if (m_size == 0)
          return -1;

        return uint8_t(*m_cursor);
      }

      bool eof() const override { return m_size == 0; }

    protected:
      const char* m_cursor = nullptr;
      size_t m_size = 0;
    };

  } // namespace detail

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parse()
  {
    if (auto err = parseValue())
      return err;

    if (!m_builder.isValidRoot())
      return makeError(Error::InvalidRoot);

    return {Error::None};
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parseValue()
  {
    TokenType tt = TokenType::Unknown;
    if (auto err = peekNextToken(tt))
      return err;

    switch (tt)
    {
    case TokenType::Number: {
      if (double number = 0.0; auto err = parseNumber(number))
        return err;
      else if (Error::Type errType = m_builder.setValue(number))
        return makeError(errType);
    }
    break;

    case TokenType::String: {
      if (auto err = parseString())
        return err;
    }
    break;

    case TokenType::Identifier: {
      if (TokenType lit = TokenType::Unknown; auto err = parseLiteral(lit))
        return err;
      else
      {
        Error::Type errType;
        if (lit == TokenType::LiteralTrue)
          errType = m_builder.setValue(true);
        else if (lit == TokenType::LiteralFalse)
          errType = m_builder.setValue(false);
        else if (lit == TokenType::LiteralNull)
          errType = m_builder.setValue(nullptr);
        else
          return makeError(Error::InvalidLiteral);

        if (errType)
          return makeError(errType);
      }
    }
    break;

    case TokenType::ObjectBegin: {
      if (Error::Type errType = m_builder.pushObject())
        return makeError(errType);

      {
        if (auto err = parseObject())
          return err;
      }
      if (Error::Type errType = m_builder.pop())
        return makeError(errType);
    }
    break;

    case TokenType::ArrayBegin: {
      if (Error::Type errType = m_builder.pushArray())
        return makeError(errType);

      {
        if (auto err = parseArray())
          return err;
      }
      if (Error::Type errType = m_builder.pop())
        return makeError(errType);
    }
    break;

    default:
      return makeError(Error::SyntaxError);
    }

    return {Error::None};
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parseObject()
  {
    next(); // Consume '{'

    bool expectComma = false;
    while (!eof())
    {
      TokenType tt = TokenType::Unknown;
      if (auto err = peekNextToken(tt))
        return err;

      switch (tt)
      {
      case TokenType::Identifier:
      case TokenType::String: {
        if (expectComma)
          return makeError(Error::CommaExpected);
      }
      break;

      case TokenType::ObjectEnd:
        next(); // Consume '}'
        return {Error::None};

      case TokenType::Comma:
        if (!expectComma)
          return makeError(Error::SyntaxError);

        next(); // Consume ','
        expectComma = false;
        continue;

      default:
        return expectComma ? makeError(Error::CommaExpected) : makeError(Error::SyntaxError);
      }

      if (auto err = parseIdentifier())
        return err;

      if (auto err = peekNextToken(tt))
        return err;

      if (tt != TokenType::Colon)
        return makeError(Error::ColonExpected);

      next(); // Consume ':'

      m_builder.addKey();
      if (auto err = parseValue())
        return err;

      m_builder.addKeyedValue();
      expectComma = true;
    }

    return makeError(Error::UnexpectedEnd);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parseArray()
  {
    next(); // Consume '['

    bool expectComma = false;
    while (!eof())
    {
      TokenType tt = TokenType::Unknown;
      if (auto err = peekNextToken(tt))
        return err;

      if (tt == TokenType::ArrayEnd && next()) // Consume ']'
        return {Error::None};
      else if (expectComma)
      {
        expectComma = false;

        if (tt != TokenType::Comma)
          return makeError(Error::CommaExpected);

        next(); // Consume ','
        continue;
      }

      m_builder.beginArrayValue();
      if (auto err = parseValue())
        return err;

      m_builder.addArrayValue();
      expectComma = true;
    }

    return makeError(Error::UnexpectedEnd);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::peekNextToken(TokenType& result)
  {
    enum class CommentType
    {
      None,
      Line,
      Block
    } parsingComment = CommentType::None;

    while (!eof())
    {
      int ch = peek();
      if (ch == '\n')
      {
        if (parsingComment == CommentType::Line)
          parsingComment = CommentType::None;
      }
      else if (parsingComment != CommentType::None || (ch > 0 && ch <= 32))
      {
        if (parsingComment == CommentType::Block && ch == '*' && next()) // Consume '*'
        {
          if (peek() == '/')
            parsingComment = CommentType::None;
        }
      }
      else if (ch == '/' && next()) // Consume '/'
      {
        if (peek() == '/')
          parsingComment = CommentType::Line;
        else if (peek() == '*')
          parsingComment = CommentType::Block;
        else
          return makeError(Error::SyntaxError);
      }
      else if (strchr("{}[]:,", ch))
      {
        if (ch == '{')
          result = TokenType::ObjectBegin;
        else if (ch == '}')
          result = TokenType::ObjectEnd;
        else if (ch == '[')
          result = TokenType::ArrayBegin;
        else if (ch == ']')
          result = TokenType::ArrayEnd;
        else if (ch == ':')
          result = TokenType::Colon;
        else if (ch == ',')
          result = TokenType::Comma;

        return {Error::None};
      }
      else if (isalpha(ch) || ch == '_')
      {
        result = TokenType::Identifier;
        return {Error::None};
      }
      else if (isdigit(ch) || ch == '.' || ch == '+' || ch == '-')
      {
        if (ch == '+')
          next(); // Consume leading '+'

        result = TokenType::Number;
        return {Error::None};
      }
      else if (ch == '"' || ch == '\'')
      {
        result = TokenType::String;
        return {Error::None};
      }
      else
        return makeError(Error::SyntaxError);

      next();
    }

    return makeError(Error::UnexpectedEnd);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parseNumber(double& result)
  {
    char buff[256] = {};
    size_t length = 0;

    while (!eof() && length < sizeof(buff))
    {
      buff[length++] = next();

      int ch = peek();
      if ((ch > 0 && ch <= 32) || ch == ',' || ch == '}' || ch == ']')
        break;
    }

#if defined(_JSON5_HAS_CHARCONV)
    auto convResult = std::from_chars(buff, buff + length, result);

    if (convResult.ec != std::errc())
      return makeError(Error::SyntaxError);
#else
    char* buffEnd = nullptr;
    result = strtod(buff, &buffEnd);

    if (result == 0.0 && buffEnd == buff)
      return makeError(Error::SyntaxError);
#endif

    return {Error::None};
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parseString()
  {
    static const constexpr char* HexChars = "0123456789abcdefABCDEF";

    bool singleQuoted = peek() == '\'';
    next(); // Consume '\'' or '"'

    if (Error::Type errType = m_builder.setString())
      return makeError(errType);

    while (!eof())
    {
      int ch = peek();
      if (((singleQuoted && ch == '\'') || (!singleQuoted && ch == '"')) && next()) // Consume '\'' or '"'
        break;
      else if (ch == '\\' && next()) // Consume '\\'
      {
        ch = peek();
        if (ch == '\n' || ch == 'v' || ch == 'f')
          next();
        else if (ch == 't' && next())
          m_builder.stringBufferAdd('\t');
        else if (ch == 'n' && next())
          m_builder.stringBufferAdd('\n');
        else if (ch == 'r' && next())
          m_builder.stringBufferAdd('\r');
        else if (ch == 'b' && next())
          m_builder.stringBufferAdd('\b');
        else if (ch == '\\' && next())
          m_builder.stringBufferAdd('\\');
        else if (ch == '\'' && next())
          m_builder.stringBufferAdd('\'');
        else if (ch == '"' && next())
          m_builder.stringBufferAdd('"');
        else if (ch == '\\' && next())
          m_builder.stringBufferAdd('\\');
        else if (ch == '/' && next())
          m_builder.stringBufferAdd('/');
        else if (ch == '0' && next())
          m_builder.stringBufferAdd(0);
        else if ((ch == 'x' || ch == 'u') && next())
        {
          char code[5] = {};

          for (size_t i = 0, s = (ch == 'x') ? 2 : 4; i < s; ++i)
            if (!strchr(HexChars, code[i] = char(next())))
              return makeError(Error::InvalidEscapeSeq);

          uint64_t unicodeChar = 0;

#if defined(_JSON5_HAS_CHARCONV)
          std::from_chars(code, code + 5, unicodeChar, 16);
#else
          char* codeEnd = nullptr;
          unicodeChar = strtoull(code, &codeEnd, 16);

          if (!unicodeChar && codeEnd == code)
            return makeError(Error::InvalidEscapeSeq);
#endif

          m_builder.stringBufferAddUtf8(uint32_t(unicodeChar));
        }
        else
          return makeError(Error::InvalidEscapeSeq);
      }
      else
        m_builder.stringBufferAdd(next());
    }

    if (eof())
      return makeError(Error::UnexpectedEnd);

    m_builder.stringBufferEnd();
    return {Error::None};
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parseIdentifier()
  {
    int firstCh = peek();
    bool isString = (firstCh == '\'') || (firstCh == '"');

    if (Error::Type errType = m_builder.setString())
      return makeError(errType);

    if (isString) // Use the string parsing function
      return parseString();

    while (!eof())
    {
      m_builder.stringBufferAdd(next());

      int ch = peek();
      if (!isalpha(ch) && !isdigit(ch) && ch != '_')
        break;
    }

    if (isString && firstCh != next()) // Consume '\'' or '"'
      return makeError(Error::SyntaxError);

    m_builder.stringBufferEnd();
    return {Error::None};
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error Parser::parseLiteral(TokenType& result)
  {
    int ch = peek();

    // "true"
    if (ch == 't')
    {
      if (next() && next() == 'r' && next() == 'u' && next() == 'e')
      {
        result = TokenType::LiteralTrue;
        return {Error::None};
      }
    }
    // "false"
    else if (ch == 'f')
    {
      if (next() && next() == 'a' && next() == 'l' && next() == 's' && next() == 'e')
      {
        result = TokenType::LiteralFalse;
        return {Error::None};
      }
    }
    // "null"
    else if (ch == 'n')
    {
      if (next() && next() == 'u' && next() == 'l' && next() == 'l')
      {
        result = TokenType::LiteralNull;
        return {Error::None};
      }
    }

    return makeError(Error::InvalidLiteral);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  inline Error FromStream(std::istream& is, Builder& builder)
  {
    detail::StlIstream src(is);
    Parser r(builder, src);
    return r.parse();
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error FromString(std::string_view str, Builder& builder)
  {
    detail::MemoryBlock src(str.data(), str.size());
    Parser r(builder, src);
    return r.parse();
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error FromFile(std::string_view fileName, Builder& builder)
  {
    std::ifstream ifs(std::string(fileName).c_str());
    if (!ifs.is_open())
      return {Error::CouldNotOpen};

    auto str = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    return FromString(std::string_view(str), builder);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error FromStream(std::istream& is, Document& doc)
  {
    Builder builder(doc);
    return FromStream(is, builder);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error FromString(std::string_view str, Document& doc)
  {
    Builder builder(doc);
    return FromString(str, builder);
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline Error FromFile(std::string_view fileName, Document& doc)
  {
    Builder builder(doc);
    return FromFile(fileName, builder);
  }
} // namespace json5
