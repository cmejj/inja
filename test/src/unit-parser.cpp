#include "catch.hpp"
#include "json.hpp"
#include "inja.hpp"


using json = nlohmann::json;
using Type = inja::Parser::Type;


TEST_CASE("Parse structure") {
	inja::Parser parser = inja::Parser();

	SECTION("Basic string") {
		std::string test = "lorem ipsum";
		json result = {{{"type", Type::String}, {"text", "lorem ipsum"}}};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Empty string") {
		std::string test = "";
		json result = {};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Variable") {
		std::string test = "{{ name }}";
		json result = {{{"type", Type::Variable}, {"command", "name"}}};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Combined string and variables") {
		std::string test = "Hello {{ name }}!";
		json result = {
			{{"type", Type::String}, {"text", "Hello "}},
			{{"type", Type::Variable}, {"command", "name"}},
			{{"type", Type::String}, {"text", "!"}}
		};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Multiple variables") {
		std::string test = "Hello {{ name }}! I come from {{ city }}.";
		json result = {
			{{"type", Type::String}, {"text", "Hello "}},
			{{"type", Type::Variable}, {"command", "name"}},
			{{"type", Type::String}, {"text", "! I come from "}},
			{{"type", Type::Variable}, {"command", "city"}},
			{{"type", Type::String}, {"text", "."}}
		};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Loops") {
		std::string test = "open {% for e in list %}lorem{% endfor %} closing";
		json result = {
			{{"type", Type::String}, {"text", "open "}},
			{{"type", Type::Loop}, {"command", "for e in list"}, {"children", {
				{{"type", Type::String}, {"text", "lorem"}}
			}}},
			{{"type", Type::String}, {"text", " closing"}}
		};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Nested loops") {
		std::string test = "{% for e in list %}{% for b in list2 %}lorem{% endfor %}{% endfor %}";
		json result = {
			{{"type", Type::Loop}, {"command", "for e in list"}, {"children", {
				{{"type", Type::Loop}, {"command", "for b in list2"}, {"children", {
					{{"type", Type::String}, {"text", "lorem"}}
				}}}
			}}}
		};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Basic conditional") {
		std::string test = "{% if true %}Hello{% endif %}";
		json result = {
			{{"type", Type::Condition}, {"children", {
				{{"type", Type::ConditionBranch}, {"command", "if true"}, {"children", {
					{{"type", Type::String}, {"text", "Hello"}}
				}}}
			}}}
		};
		CHECK( parser.parse(test) == result );
	}

	SECTION("If/else if/else conditional") {
		std::string test = "if: {% if maybe %}first if{% else if perhaps %}first else if{% else if sometimes %}second else if{% else %}test else{% endif %}";
		json result = {
			{{"type", Type::String}, {"text", "if: "}},
			{{"type", Type::Condition}, {"children", {
				{{"type", Type::ConditionBranch}, {"command", "if maybe"}, {"children", {
					{{"type", Type::String}, {"text", "first if"}}
				}}},
				{{"type", Type::ConditionBranch}, {"command", "else if perhaps"}, {"children", {
					{{"type", Type::String}, {"text", "first else if"}}
				}}},
				{{"type", Type::ConditionBranch}, {"command", "else if sometimes"}, {"children", {
					{{"type", Type::String}, {"text", "second else if"}}
				}}},
				{{"type", Type::ConditionBranch}, {"command", "else"}, {"children", {
					{{"type", Type::String}, {"text", "test else"}}
				}}},
			}}}
		};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Comments") {
		std::string test = "{# lorem ipsum #}";
		json result = {{{"type", Type::Comment}, {"text", "lorem ipsum"}}};
		CHECK( parser.parse(test) == result );
	}

	SECTION("Line Statements") {
		std::string test = R"(## if true
lorem ipsum
## endif)";
		json result = {
			{{"type", Type::Condition}, {"children", {
				{{"type", Type::ConditionBranch}, {"command", "if true"}, {"children", {
					{{"type", Type::String}, {"text", "lorem ipsum"}}
				}}}
			}}}
		};
		CHECK( parser.parse(test) == result );
	}
}

TEST_CASE("Parse variables") {
	inja::Environment env = inja::Environment();

	json data;
	data["name"] = "Peter";
	data["city"] = "Washington D.C.";
	data["age"] = 29;
	data["names"] = {"Jeff", "Seb"};
	data["brother"]["name"] = "Chris";
	data["brother"]["daughters"] = {"Maria", "Helen"};
	data["brother"]["daughter0"] = { { "name", "Maria" } };

	SECTION("Variables from values") {
		CHECK( env.eval_variable("42", data) == 42 );
		CHECK( env.eval_variable("3.1415", data) == 3.1415 );
		CHECK( env.eval_variable("\"hello\"", data) == "hello" );
		CHECK( env.eval_variable("true", data) == true );
		CHECK( env.eval_variable("[5, 6, 8]", data) == std::vector<int>({5, 6, 8}) );
	}

	SECTION("Variables from JSON data") {
		CHECK( env.eval_variable("name", data) == "Peter" );
		CHECK( env.eval_variable("age", data) == 29 );
		CHECK( env.eval_variable("names/1", data) == "Seb" );
		CHECK( env.eval_variable("brother/name", data) == "Chris" );
		CHECK( env.eval_variable("brother/daughters/0", data) == "Maria" );
		CHECK( env.eval_variable("/age", data) == 29 );

		CHECK_THROWS_WITH( env.eval_variable("noelement", data), "JSON pointer found no element." );
		CHECK_THROWS_WITH( env.eval_variable("&4s-", data), "JSON pointer found no element." );
	}
}

TEST_CASE("Parse conditions") {
	inja::Environment env = inja::Environment();

	json data;
	data["age"] = 29;
	data["brother"] = "Peter";
	data["father"] = "Peter";
	data["guests"] = {"Jeff", "Seb"};

	SECTION("Elements") {
		CHECK( env.eval_condition("age", data) );
		CHECK( env.eval_condition("guests", data) );
		CHECK_FALSE( env.eval_condition("size", data) );
		CHECK_FALSE( env.eval_condition("false", data) );
	}

	SECTION("Operators") {
		CHECK( env.eval_condition("not size", data) );
		CHECK_FALSE( env.eval_condition("not true", data) );
		CHECK( env.eval_condition("true and true", data) );
		CHECK( env.eval_condition("true or false", data) );
		CHECK_FALSE( env.eval_condition("true and not true", data) );
	}

	SECTION("Numbers") {
		CHECK( env.eval_condition("age == 29", data) );
		CHECK( env.eval_condition("age >= 29", data) );
		CHECK( env.eval_condition("age <= 29", data) );
		CHECK( env.eval_condition("age < 100", data) );
		CHECK_FALSE( env.eval_condition("age > 29", data) );
		CHECK_FALSE( env.eval_condition("age != 29", data) );
		CHECK_FALSE( env.eval_condition("age < 28", data) );
		CHECK_FALSE( env.eval_condition("age < -100.0", data) );
	}

	SECTION("Strings") {
		CHECK( env.eval_condition("brother == father", data) );
		CHECK( env.eval_condition("brother == \"Peter\"", data) );
		CHECK_FALSE( env.eval_condition("not brother == father", data) );
	}

	SECTION("Lists") {
		CHECK( env.eval_condition("\"Jeff\" in guests", data) );
		CHECK_FALSE( env.eval_condition("brother in guests", data) );
	}
}

TEST_CASE("Parse functions") {
	inja::Environment env = inja::Environment();

	json data;
	data["name"] = "Peter";
	data["city"] = "New York";
	data["names"] = {"Jeff", "Seb", "Peter", "Tom"};

	SECTION("Upper") {
		CHECK( env.eval_variable("upper(name)", data) == "PETER" );
		CHECK( env.eval_variable("upper(city)", data) == "NEW YORK" );
		CHECK_THROWS_WITH( env.eval_variable("upper(5)", data), "Argument in upper function is not a string." );
	}

	SECTION("Lower") {
		CHECK( env.eval_variable("lower(name)", data) == "peter" );
		CHECK( env.eval_variable("lower(city)", data) == "new york" );
		CHECK_THROWS_WITH( env.eval_variable("lower(5)", data), "Argument in lower function is not a string." );
	}

	SECTION("Range") {
		CHECK( env.eval_variable("range(4)", data) == std::vector<int>({0, 1, 2, 3}) );
		CHECK_THROWS_WITH( env.eval_variable("range(true)", data), "Argument in range function is not a number." );
	}

	SECTION("Length") {
		CHECK( env.eval_variable("length(names)", data) == 4 );
		CHECK_THROWS_WITH( env.eval_variable("length(5)", data), "Argument in length function is not a list." );
	}
}
