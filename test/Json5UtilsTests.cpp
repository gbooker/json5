#include <gtest/gtest.h>

/////////////////////////////////////////////////////////////////////////////////////////
bool PrintError(const json5::error& err)
{
  if (err)
  {
    std::cout << json5::to_string(err) << std::endl;
    return true;
  }

  return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, Build)
{
  json5::document doc;
  json5::builder b(doc);

  b.push_object();
  {
    b["x"] = b.new_string("Hello!");
    b["y"] = json5::value(123.0);
    b["z"] = json5::value(true);

    b.push_array();
    {
      b += b.new_string("a");
      b += b.new_string("b");
      b += b.new_string("c");
    }
    b["arr"] = b.pop();
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
  EXPECT_EQ(json5::to_string(doc), expected);
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, LoadFromFile)
{
  json5::document doc;
  fs::path path = "short_example.json5";
  PrintError(json5::from_file(path.string(), doc));

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
  EXPECT_EQ(json5::to_string(doc), expected);
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, Equality)
{
  json5::document doc1;
  json5::from_string("{ x: 1, y: 2, z: 3 }", doc1);

  json5::document doc2;
  json5::from_string("{ z: 3, x: 1, y: 2 }", doc2);

  EXPECT_EQ(doc1, doc2);
}

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, FileSaveLoad)
{
  json5::document doc1;
  json5::document doc2;
  fs::path path = "twitter.json";
  PrintError(json5::from_file(path.string(), doc1));

  {
    json5::writer_params wp;
    wp.compact = true;

    json5::to_file("twitter.json5", doc1, wp);
  }

  json5::from_file("twitter.json5", doc2);

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

/////////////////////////////////////////////////////////////////////////////////////////
TEST(Json5, Reflection)
{
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

  json5::to_file("Foo.json5", foo1);

  Foo foo2;
  json5::from_file("Foo.json5", foo2);

  EXPECT_EQ(foo1, foo2);
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
    json5::document doc;
    if (auto err = json5::from_string(str, doc))
      break;
  }
}
