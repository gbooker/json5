#pragma once

#include <functional>

#include "json5.hpp"

namespace json5
{

  //
  template <typename Func>
  void Filter(const json5::Value& in, std::string_view pattern, Func&& func);

  //
  std::vector<json5::Value> Filter(const json5::Value& in, std::string_view pattern);

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //---------------------------------------------------------------------------------------------------------------------
  template <typename Func>
  inline void Filter(const json5::Value& in, std::string_view pattern, Func&& func)
  {
    if (pattern.empty())
    {
      func(in);
      return;
    }

    std::string_view head = pattern;
    std::string_view tail = pattern;

    if (auto slash = pattern.find("/"); slash != std::string_view::npos)
    {
      head = pattern.substr(0, slash);
      tail = pattern.substr(slash + 1);
    }
    else
      tail = std::string_view();

    // Trim whitespace
    {
      while (!head.empty() && isspace(head.front())) head.remove_prefix(1);
      while (!head.empty() && isspace(head.back())) head.remove_suffix(1);
    }

    if (head == "*")
    {
      if (in.isObject())
      {
        for (auto kvp : ObjectView(in))
          Filter(kvp.second, tail, std::forward<Func>(func));
      }
      else if (in.isArray())
      {
        for (auto v : ArrayView(in))
          Filter(v, tail, std::forward<Func>(func));
      }
      else
        Filter(in, std::string_view(), std::forward<Func>(func));
    }
    else if (head == "**")
    {
      if (in.isObject())
      {
        Filter(in, tail, std::forward<Func>(func));

        for (auto kvp : ObjectView(in))
        {
          Filter(kvp.second, tail, std::forward<Func>(func));
          Filter(kvp.second, pattern, std::forward<Func>(func));
        }
      }
      else if (in.isArray())
      {
        for (auto v : ArrayView(in))
        {
          Filter(v, tail, std::forward<Func>(func));
          Filter(v, pattern, std::forward<Func>(func));
        }
      }
    }
    else
    {
      if (in.isObject())
      {
        // Remove string quotes
        if (head.size() >= 2)
        {
          auto first = head.front();
          if ((first == '\'' || first == '"') && head.back() == first)
            head = head.substr(1, head.size() - 2);
        }

        for (auto kvp : ObjectView(in))
        {
          if (head == kvp.first)
            Filter(kvp.second, tail, std::forward<Func>(func));
        }
      }
    }
  }

  //---------------------------------------------------------------------------------------------------------------------
  inline std::vector<json5::Value> Filter(const json5::Value& in, std::string_view pattern)
  {
    std::vector<Value> result;
    Filter(in, pattern, [&result](const Value& v) { result.push_back(v); });
    return result;
  }

} // namespace json5
