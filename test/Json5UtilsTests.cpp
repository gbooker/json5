#include <gtest/gtest.h>

/////////////////////////////////////////////////////////////////////////////////////////
bool PrintError(const json5::Error& err)
{
  if (err)
  {
    std::cout << json5::ToString(err) << std::endl;
    return true;
  }

  return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, Build)
{
  json5::Document doc;
  json5::DocumentBuilder b(doc);

  b.pushObject();
  {
    b["x"] = b.newString("Hello!");
    b["y"] = json5::Value(123.0);
    b["z"] = json5::Value(true);

    b.pushArray();
    {
      b += b.newString("a");
      b += b.newString("b");
      b += b.newString("c");
    }
    b.pop();
    b["arr"] = b.getCurrentValue();
  }
  b.pop();

  string expected = R"({
  x: "Hello!",
  y: 123,
  z: true,
  arr: [
    "a",
    "b",
    "c"
  ]
}
)";
  EXPECT_EQ(json5::ToString(doc), expected);
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, LoadFromFile)
{
  json5::Document doc;
  fs::path path = "short_example.json5";
  PrintError(json5::FromFile(path.string(), doc));

  string expected = R"({
  unquoted: "and you can quote me on that",
  singleQuotes: "I can use \"double quotes\" here",
  lineBreaks: "Look, Mom! No \\n's!",
  leadingDecimalPoint: 0.867531,
  andTrailing: 8675309,
  positiveSign: 1,
  trailingComma: "in objects",
  andIn: [
    "arrays"
  ],
  backwardsCompatible: "with JSON"
}
)";
  EXPECT_EQ(json5::ToString(doc), expected);
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, Equality)
{
  json5::Document doc1;
  json5::FromString("{ x: 1, y: 2, z: 3 }", doc1);

  json5::Document doc2;
  json5::FromString("{ z: 3, x: 1, y: 2 }", doc2);

  EXPECT_EQ(doc1, doc2);
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, FileSaveLoad)
{
  json5::Document doc1;
  json5::Document doc2;
  fs::path path = "twitter.json";
  PrintError(json5::FromFile(path.string(), doc1));

  {
    json5::WriterParams wp;
    wp.compact = true;

    json5::ToFile("twitter.json5", doc1, wp);
  }

  json5::FromFile("twitter.json5", doc2);

  EXPECT_EQ(doc1, doc2);
}

/////////////////////////////////////////////////////////////////////////////////////////

// This requires a newer compiler than our current clang
// enum class MyEnum
// {
//   Zero,
//   First,
//   Second,
//   Third
// };

// JSON5_ENUM(MyEnum, Zero, First, Second, Third)

/////////////////////////////////////////////////////////////////////////////////////////
struct BarBase
{
  std::string name;
};

JSON5_CLASS(BarBase, name)

/////////////////////////////////////////////////////////////////////////////////////////
struct Bar : BarBase
{
  int age = 0;
  bool operator==(const Bar& o) const { return std::tie(name, age) == std::tie(o.name, o.age); }
};

JSON5_CLASS_INHERIT(Bar, BarBase, age)

struct Foo
{
  int x = 0;
  float y = 0;
  bool z = false;
  std::string text;
  std::vector<int> numbers;
  std::map<std::string, Bar> barMap;

  std::array<float, 3> position = {0.0f, 0.0f, 0.0f};

  Bar bar;
  // MyEnum e;

  JSON5_MEMBERS(x, y, z, text, numbers, barMap, position, bar /*, e*/)
  bool operator==(const Foo& o) const noexcept { return std::tie(x, y, z, text, numbers, barMap, position, bar /*, e*/) == std::tie(o.x, o.y, o.z, o.text, o.numbers, o.barMap, o.position, o.bar /*, o.e*/); }
};

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, LowLevelReflection)
{
  {
    int i;
    json5::ReflectionBuilder builder(i);
    json5::Error err = FromString("5", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(i, 5);
  }

  {
    unsigned long l;
    json5::ReflectionBuilder builder(l);
    json5::Error err = FromString("5555", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(l, 5555);
  }

  {
    long l;
    json5::ReflectionBuilder builder(l);
    json5::Error err = FromString("5555", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(l, 5555);
  }

  {
    unsigned long long l;
    json5::ReflectionBuilder builder(l);
    json5::Error err = FromString("5555", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(l, 5555);
  }

  {
    long long l;
    json5::ReflectionBuilder builder(l);
    json5::Error err = FromString("5555", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(l, 5555);
  }

  {
    double d;
    json5::ReflectionBuilder builder(d);
    json5::Error err = FromString("5.5", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(d, 5.5);
  }

  {
    string str;
    json5::ReflectionBuilder builder(str);
    json5::Error err = FromString("\"Hahaha\"", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(str, "Hahaha");
  }

  {
    vector<string> v;
    json5::ReflectionBuilder builder(v);
    json5::Error err = FromString("[\"Hahaha\",\"Hoho\"]", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    vector<string> expected = {
      "Hahaha",
      "Hoho",
    };
    EXPECT_EQ(v, expected);
  }

  {
    map<string, int> m;
    json5::ReflectionBuilder builder(m);
    json5::Error err = FromString("{\"Hahaha\":3,\"Hoho\":2}", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    map<string, int> expected = {
      {"Hahaha", 3},
      {"Hoho", 2},
    };
    EXPECT_EQ(m, expected);
  }

  {
    std::multimap<string, string, ASCIICaseInsensitiveComparator> m;
    json5::ReflectionBuilder builder(m);
    json5::Error err = FromString("{\"First\":[\"1\", \"2\"],\"Second\":[\"3\"]}", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    std::multimap<string, string, ASCIICaseInsensitiveComparator> expected = {
      {"First", "1"},
      {"First", "2"},
      {"Second", "3"},
    };
    EXPECT_EQ(m, expected);
  }

  // {
  //   MyEnum e;
  //   json5::ReflectionBuilder builder(e);
  //   json5::Error err = FromString("\"Third\"", (json5::Builder&)builder);
  //   EXPECT_FALSE(err);
  //   EXPECT_EQ(e, MyEnum::Third);
  // }

  {
    std::optional<int> o;
    json5::ReflectionBuilder builder(o);
    json5::Error err = FromString("5", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_TRUE(o);
    EXPECT_EQ(*o, 5);

    err = json5::FromString("null", (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_FALSE(o.has_value());
  }

  {
    Foo foo1 =
      {
        123,
        456.0f,
        true,
        "Hello, world!",
        {1, 2, 3, 4, 5},
        {{"x", {{"a"}, 1}}, {"y", {{"b"}, 2}}, {"z", {{"c"}, 3}}},
        {10.0f, 20.0f, 30.0f},
        {{"Somebody Unknown"}, 500},
        // MyEnum::Second,
      };

    auto str = json5::ToString(foo1);

    Foo foo2;
    json5::ReflectionBuilder builder(foo2);
    json5::Error err = FromString(str, (json5::Builder&)builder);
    EXPECT_FALSE(err);
    EXPECT_EQ(foo1, foo2);
  }
}

struct OptIvar
{
  std::optional<int> val;

  JSON5_MEMBERS(val);
};

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, Reflection)
{
  Foo foo1 =
    {
      123,
      456.0f,
      true,
      "Hello, world!",
      {1, 2, 3, 4, 5},
      {{"x", {{"a"}, 1}}, {"y", {{"b"}, 2}}, {"z", {{"c"}, 3}}},
      {10.0f, 20.0f, 30.0f},
      {{"Somebody Unknown"}, 500},
      // MyEnum::Second,
    };

  json5::ToFile("Foo.json5", foo1);

  Foo foo2;
  json5::FromFile("Foo.json5", foo2);

  EXPECT_EQ(foo1, foo2);

  json5::Document doc;
  json5::ToDocument(doc, foo1);
  json5::ToFile("Foo2.json5", doc);

  Foo foo3;
  json5::FromFile("Foo2.json5", foo3);

  EXPECT_EQ(foo1, foo3);

  {
    json5::IndependentValue indepententFoo;
    json5::Error err = FromFile("Foo.json5", indepententFoo);
    EXPECT_FALSE(err);

    json5::IndependentValue expected = {json5::IndependentValue::Map {
      {"x", {123.0}},
      {"y", {456.0}},
      {"z", {true}},
      {"text", {"Hello, world!"}},
      {"numbers",
        {json5::IndependentValue::Array {
          {1.0},
          {2.0},
          {3.0},
          {4.0},
          {5.0},
        }}},
      {"barMap",
        {json5::IndependentValue::Map {
          {"x",
            {json5::IndependentValue::Map {
              {"name", {"a"}},
              {"age", {1.0}},
            }}},
          {"y",
            {json5::IndependentValue::Map {
              {"name", {"b"}},
              {"age", {2.0}},
            }}},
          {"z",
            {json5::IndependentValue::Map {
              {"name", {"c"}},
              {"age", {3.0}},
            }}},
        }}},
      {"position",
        {json5::IndependentValue::Array {
          {10.0},
          {20.0},
          {30.0},
        }}},
      {"bar",
        {{json5::IndependentValue::Map {
          {"name", {"Somebody Unknown"}},
          {"age", {500.0}},
        }}}},
      // {"e", {"Second"}},
    }};
    EXPECT_EQ(indepententFoo, expected);
  }

  {
    OptIvar empty;
    string emptyStr = json5::ToString(empty);
    string emptyExpected = R"({
}
)";
    EXPECT_EQ(emptyStr, emptyExpected);

    OptIvar result;
    json5::FromString(emptyStr, result);
    EXPECT_FALSE(result.val.has_value());

    OptIvar set = {42};
    string setStr = json5::ToString(set);
    string setExpected = R"({
  val: 42
}
)";
    EXPECT_EQ(setStr, setExpected);

    json5::FromString(setStr, result);
    EXPECT_EQ(result.val, 42);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
struct Stopwatch
{
  const char* name = "";
  std::chrono::nanoseconds start = std::chrono::high_resolution_clock::now().time_since_epoch();

  ~Stopwatch()
  {
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch() - start;
    std::cout << name << ": " << duration.count() / 1000000 << " ms" << std::endl;
  }
};

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, DISABLED_Performance)
/// Performance test
{
  fs::path path = "twitter.json";
  std::ifstream ifs(path.string());
  std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  Stopwatch sw {"Parse twitter.json 100x"};

  for (int i = 0; i < 100; ++i)
  {
    json5::Document doc;
    if (auto err = json5::FromString(str, doc))
      break;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, DISABLED_PerformanceOfIndependentValue)
/// Performance test
{
  fs::path path = "twitter.json";
  std::ifstream ifs(path.string());
  std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  Stopwatch sw {"Parse twitter.json 100x"};

  for (int i = 0; i < 100; ++i)
  {
    json5::IndependentValue value;
    if (auto err = json5::FromString(str, value))
      break;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, Independent)
{
  json5::IndependentValue value;
  string json = R"(
    {
      "bool": false,
      "num": 435.243,
      "str": "a string",
      "arr": ["str", 8, false],
      "obj": {
        "a": "value"
      }
    }
  )";
  json5::Error error = json5::FromString(json, value);
  EXPECT_EQ(error.type, json5::Error::None);

  json5::IndependentValue expected = {json5::IndependentValue::Map {
    {"bool", {false}},
    {"num", {435.243}},
    {"str", {"a string"}},
    {"arr",
      {json5::IndependentValue::Array {
        {"str"s},
        {8.0},
        {false},
      }}},
    {"obj",
      {json5::IndependentValue::Map {
        {"a", {"value"}},
      }}},
  }};

  EXPECT_EQ(value, expected);

  string str = json5::ToString(value, StandardJsonWriteParams);
  EXPECT_EQ(str, R"({"arr":["str",8,false],"bool":false,"num":435.243,"obj":{"a":"value"},"str":"a string"})");
}
