// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_program_options.hpp>

#include <gtest/gtest.h>

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace po = boost::program_options;
namespace npo = nscapi::program_options;

namespace {

//////////////////////////////////////////////////////////////////////////
// option_parser_kvp
//
// This is the parser that makes REST work: over REST a check argument
// arrives as the single token "key=value", whereas the CLI hands us
// "--key" and "value" as two tokens. Everything below pins the single
// token form, since that is the one that only breaks in production.
//////////////////////////////////////////////////////////////////////////

std::vector<po::option> parse_kvp(std::vector<std::string> args, const std::string &break_at = "") {
  return npo::option_parser_kvp(args, break_at);
}

TEST(program_options_kvp, splits_key_value_token) {
  const std::vector<po::option> result = parse_kvp({"warning", "critical=load>5"});

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("warning", result[0].string_key);
  EXPECT_TRUE(result[0].value.empty()) << "a token without '=' carries no value";
  EXPECT_EQ("critical", result[1].string_key);
  ASSERT_EQ(1u, result[1].value.size());
  EXPECT_EQ("load>5", result[1].value[0]);
}

TEST(program_options_kvp, splits_on_the_first_equals_only) {
  // Filters routinely contain '=', e.g. filter=name='foo'. Splitting on the
  // last (or every) '=' would truncate them.
  const std::vector<po::option> result = parse_kvp({"filter=name='a=b'"});

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("filter", result[0].string_key);
  ASSERT_EQ(1u, result[0].value.size());
  EXPECT_EQ("name='a=b'", result[0].value[0]);
}

TEST(program_options_kvp, keeps_an_empty_value_as_a_value) {
  // "key=" must stay distinct from a bare "key": the former explicitly sets
  // the option to the empty string, the latter does not set it at all.
  const std::vector<po::option> result = parse_kvp({"empty-syntax="});

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("empty-syntax", result[0].string_key);
  ASSERT_EQ(1u, result[0].value.size());
  EXPECT_EQ("", result[0].value[0]);
}

TEST(program_options_kvp, records_the_original_token) {
  const std::vector<po::option> result = parse_kvp({"warning=load>5"});

  ASSERT_EQ(1u, result.size());
  ASSERT_EQ(1u, result[0].original_tokens.size());
  EXPECT_EQ("warning=load>5", result[0].original_tokens[0]);
}

TEST(program_options_kvp, break_at_swallows_the_remaining_tokens) {
  // Used to hand everything after a marker to a wrapped command verbatim.
  const std::vector<po::option> result = parse_kvp({"target=host", "--", "-x", "raw=arg"}, "--");

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("target", result[0].string_key);
  EXPECT_EQ("--", result[1].string_key);
  ASSERT_EQ(2u, result[1].value.size());
  EXPECT_EQ("-x", result[1].value[0]);
  EXPECT_EQ("raw=arg", result[1].value[1]) << "tokens after the break are not split on '='";
}

TEST(program_options_kvp, break_at_only_matches_a_bare_token) {
  // "--=x" contains '=' so it is a key/value pair, not the break marker.
  const std::vector<po::option> result = parse_kvp({"--=x", "after"}, "--");

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("--", result[0].string_key);
  ASSERT_EQ(1u, result[0].value.size());
  EXPECT_EQ("x", result[0].value[0]);
  EXPECT_EQ("after", result[1].string_key) << "parsing must continue past a non-break token";
}

TEST(program_options_kvp, empty_break_at_never_breaks) {
  const std::vector<po::option> result = parse_kvp({"a", "b"});

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("a", result[0].string_key);
  EXPECT_EQ("b", result[1].string_key);
}

TEST(program_options_kvp, consumes_the_input) {
  // The caller passes a mutable vector and boost::program_options relies on
  // the parser having taken ownership of everything it handled.
  std::vector<std::string> args = {"a=1", "b=2"};
  const std::vector<po::option> result = npo::option_parser_kvp(args, "");

  EXPECT_EQ(2u, result.size());
  EXPECT_TRUE(args.empty());
}

TEST(program_options_kvp, handles_no_arguments) {
  EXPECT_TRUE(parse_kvp({}).empty());
}

//////////////////////////////////////////////////////////////////////////
// extract_default_value
//
// Turns boost's format_parameter() rendering back into a bare default, for
// the help/CSV/show-default output.
//////////////////////////////////////////////////////////////////////////

TEST(program_options_default_value, a_value_without_a_default_has_none) {
  po::options_description desc;
  desc.add_options()("plain", po::value<std::string>(), "d");

  EXPECT_EQ("arg", desc.options()[0]->format_parameter()) << "guards the assumption the code below strips";
  EXPECT_EQ("", npo::extract_default_value(desc.options()[0]->format_parameter()));
}

TEST(program_options_default_value, strips_the_boost_decoration) {
  po::options_description desc;
  desc.add_options()("with-default", po::value<std::string>()->default_value("ok"), "d");

  EXPECT_EQ("ok", npo::extract_default_value(desc.options()[0]->format_parameter()));
}

TEST(program_options_default_value, strips_an_implicit_value) {
  // The shape every boolean check option uses: implicit_value(true) so REST
  // can pass "x=true", default_value(false) so the flag is off by default.
  po::options_description desc;
  desc.add_options()("flag", po::value<bool>()->implicit_value(true)->default_value(false), "d");

  const std::string stripped = npo::extract_default_value(desc.options()[0]->format_parameter());
  EXPECT_EQ(std::string::npos, stripped.find("arg")) << "no boost decoration may survive, got: " << stripped;
  EXPECT_EQ(std::string::npos, stripped.find('[')) << "no boost decoration may survive, got: " << stripped;
  // The default, not the implicit value: boost renders both, and "" would pass
  // the two checks above while telling the reader nothing.
  EXPECT_EQ("0", stripped);
}

TEST(program_options_default_value, input_without_a_default_has_none) {
  EXPECT_EQ("", npo::extract_default_value("arg"));
  EXPECT_EQ("", npo::extract_default_value("x"));
  EXPECT_EQ("", npo::extract_default_value(""));
}

//////////////////////////////////////////////////////////////////////////
// make_csv / help_csv
//
// help_csv output is consumed as CSV by the docs build, so quoting matters.
//////////////////////////////////////////////////////////////////////////

TEST(program_options_csv, leaves_a_plain_value_unquoted) {
  EXPECT_EQ("plain", npo::make_csv("plain"));
}

TEST(program_options_csv, quotes_a_value_containing_a_separator) {
  EXPECT_EQ("\"a,b\"", npo::make_csv("a,b"));
}

TEST(program_options_csv, escapes_and_quotes_embedded_quotes) {
  EXPECT_EQ("\"say \\\"hi\\\"\"", npo::make_csv("say \"hi\""));
}

TEST(program_options_csv, escapes_newlines_so_a_row_stays_one_line) {
  // A raw newline would split the record in two and corrupt every later column.
  const std::string csv = npo::make_csv("first\nsecond");
  EXPECT_EQ(std::string::npos, csv.find('\n'));
  EXPECT_EQ("first\\nsecond", csv);
}

TEST(program_options_csv, renders_one_row_per_option) {
  po::options_description desc;
  desc.add_options()("flag", "a flag")("value", po::value<std::string>()->default_value("5"), "a value");

  const std::string csv = npo::help_csv(desc, "");

  EXPECT_EQ("flag,false,,a flag\nvalue,true,5,a value\n", csv);
}

//////////////////////////////////////////////////////////////////////////
// help_show_default
//////////////////////////////////////////////////////////////////////////

TEST(program_options_show_default, lists_only_options_that_have_a_default) {
  po::options_description desc;
  desc.add_options()("flag", "no value at all")("no-default", po::value<std::string>(), "value, no default")(
      "with-default", po::value<std::string>()->default_value("42"), "value with default");

  const std::string shown = npo::help_show_default(desc);

  EXPECT_EQ("\"with-default=42\" ", shown);
}

//////////////////////////////////////////////////////////////////////////
// format_paragraph / format_description
//
// The wrapping used by every `--help` rendering.
//////////////////////////////////////////////////////////////////////////

TEST(program_options_format, leaves_a_short_paragraph_alone) {
  std::stringstream ss;
  npo::format_paragraph(ss, "short", 4, 40);

  EXPECT_EQ("short", ss.str());
}

TEST(program_options_format, wraps_a_long_paragraph_and_indents_continuations) {
  std::stringstream ss;
  npo::format_paragraph(ss, "aaaa bbbb cccc dddd eeee ffff gggg hhhh", 4, 20);

  const std::string out = ss.str();
  ASSERT_NE(std::string::npos, out.find('\n')) << "expected wrapping, got: " << out;
  // Every continuation line is padded to the indent.
  std::stringstream lines(out);
  std::string line;
  bool first = true;
  while (std::getline(lines, line)) {
    if (!first) {
      EXPECT_EQ("    ", line.substr(0, 4)) << "continuation not indented: " << line;
    }
    EXPECT_LE(line.size(), 20u) << "line exceeds the requested width: " << line;
    first = false;
  }
}

TEST(program_options_format, description_keeps_explicit_newlines_as_paragraphs) {
  // Check descriptions use "\n" to separate a summary from its detail; that
  // break has to survive into the help output.
  std::stringstream ss;
  npo::format_description(ss, "first\nsecond", 2, 40);

  const std::string out = ss.str();
  ASSERT_NE(std::string::npos, out.find('\n'));
  EXPECT_EQ("first", out.substr(0, out.find('\n')));
  EXPECT_NE(std::string::npos, out.find("second"));
}

//////////////////////////////////////////////////////////////////////////
// help
//////////////////////////////////////////////////////////////////////////

TEST(program_options_help, renders_options_with_their_defaults) {
  po::options_description desc;
  desc.add_options()("flag", "a flag")("value", po::value<std::string>()->default_value("5"), "a value");

  const std::string text = npo::help(desc, "");

  EXPECT_NE(std::string::npos, text.find("flag")) << text;
  EXPECT_NE(std::string::npos, text.find("value=ARG")) << "an option taking an argument is shown as such: " << text;
  EXPECT_EQ(std::string::npos, text.find("flag=ARG")) << "a flag takes no argument: " << text;
  EXPECT_NE(std::string::npos, text.find("Default value: value=5")) << text;
}

TEST(program_options_help, prefixes_extra_info) {
  po::options_description desc;
  desc.add_options()("flag", "a flag");

  const std::string text = npo::help(desc, "something went wrong");

  EXPECT_EQ(0u, text.find("something went wrong\n")) << text;
}

TEST(program_options_help, handles_an_option_longer_than_the_column) {
  // Long names push the description onto its own line; the branch is easy to
  // get wrong (it underflows an unsigned pad count if mishandled).
  po::options_description desc;
  desc.add_options()("an-extremely-long-option-name-that-exceeds-the-column", po::value<std::string>(), "described");

  const std::string text = npo::help(desc, "");

  EXPECT_NE(std::string::npos, text.find("an-extremely-long-option-name-that-exceeds-the-column")) << text;
  EXPECT_NE(std::string::npos, text.find("described")) << text;
}

//////////////////////////////////////////////////////////////////////////
// add_standard_filter
//
// Every modern_filter check gets its options from here, so the aliases and
// the defaults are effectively public API.
//////////////////////////////////////////////////////////////////////////

TEST(program_options_standard_filter, registers_the_documented_options) {
  po::options_description desc;
  npo::standard_filter_config filter;
  npo::add_standard_filter(desc, filter, "top", "top-keys", "detail", "keys");

  for (const char *name : {"filter", "warning", "warn", "critical", "crit", "ok", "top-syntax", "ok-syntax", "detail-syntax", "empty-syntax", "empty-state"}) {
    EXPECT_TRUE(desc.find_nothrow(name, false) != nullptr) << "missing option: " << name;
  }
}

TEST(program_options_standard_filter, applies_the_supplied_and_builtin_defaults) {
  po::options_description desc;
  npo::standard_filter_config filter;
  npo::add_standard_filter(desc, filter, "top", "top-keys", "detail", "keys");

  po::variables_map vm;
  std::vector<std::string> args;
  po::store(po::command_line_parser(args).options(desc).run(), vm);
  po::notify(vm);

  EXPECT_EQ("top", filter.syntax_top);
  EXPECT_EQ("detail", filter.syntax_detail);
  EXPECT_EQ("ok", filter.empty_state);
  EXPECT_EQ("%(status): Nothing found...", filter.syntax_empty);
}

TEST(program_options_standard_filter, warn_and_crit_are_aliases) {
  // Documented shorthand; both spellings write the same field, so a check
  // behaves identically however the user spelled it.
  po::options_description desc;
  npo::standard_filter_config filter;
  npo::add_standard_filter(desc, filter, "top", "top-keys", "detail", "keys");

  po::variables_map vm;
  std::vector<std::string> args = {"warn=w>1", "crit=c>2"};
  po::store(po::command_line_parser(args).options(desc).extra_style_parser([](std::vector<std::string> &a) { return npo::option_parser_kvp(a, ""); }).run(),
            vm);
  po::notify(vm);

  EXPECT_EQ("w>1", filter.warn_string);
  EXPECT_EQ("c>2", filter.crit_string);
}

//////////////////////////////////////////////////////////////////////////
// help_pb
//
// The machine-readable half of the help: what the registry answers with, and
// therefore what `desc`/`keywords` at the prompt and the web UI's argument
// editor read. The content type is the interesting part - it is what tells a
// consumer whether an option takes a value.
//////////////////////////////////////////////////////////////////////////

PB::Registry::ParameterDetails parse_help_pb(const po::options_description &desc) {
  PB::Registry::ParameterDetails details;
  EXPECT_TRUE(details.ParseFromString(npo::help_pb(desc)));
  return details;
}

const PB::Registry::ParameterDetail *find_parameter(const PB::Registry::ParameterDetails &details, const std::string &name) {
  for (const PB::Registry::ParameterDetail &p : details.parameter()) {
    if (p.name() == name) return &p;
  }
  return nullptr;
}

TEST(program_options_help_pb, reports_a_boolean_option_as_boolean) {
  // How every check declares a flag: a value<bool> with an implicit value,
  // because REST passes a flag as the single token `show-all=true` and
  // bool_switch rejects that. It still takes a token, so nothing but the
  // declared type tells it apart from a string option - and a consumer that
  // cannot tell completes it as `show-all=` and produces something REST
  // refuses.
  po::options_description desc;
  bool show_all = false;
  desc.add_options()("show-all", po::value<bool>(&show_all)->implicit_value(true)->default_value(false), "Show all");

  const PB::Registry::ParameterDetails details = parse_help_pb(desc);
  const PB::Registry::ParameterDetail *p = find_parameter(details, "show-all");
  ASSERT_NE(nullptr, p);
  EXPECT_EQ(PB::Common::BOOL, p->content_type());
  // And it keeps its default: unlike a switch, a flag declared this way has a
  // value it takes when left out, and that is worth reporting.
  EXPECT_EQ("false", p->default_value());
}

TEST(program_options_help_pb, reports_a_switch_as_boolean_with_no_default) {
  // An option that takes no token at all. BOOL with an empty default value is
  // the pair that says "a switch" - which is what makes the reference docs
  // print N/A in its Default Value column rather than an empty cell.
  po::options_description desc;
  desc.add_options()("help", "Show help");

  const PB::Registry::ParameterDetails details = parse_help_pb(desc);
  const PB::Registry::ParameterDetail *p = find_parameter(details, "help");
  ASSERT_NE(nullptr, p);
  EXPECT_EQ(PB::Common::BOOL, p->content_type());
  EXPECT_EQ("", p->default_value());
}

TEST(program_options_help_pb, reports_anything_that_takes_a_value_as_a_string) {
  // Everything else stays a string, including the numeric options: the
  // distinction that matters to a consumer is "does this take a value", and
  // retyping the rest would only move the guesswork somewhere else.
  po::options_description desc;
  std::string filter;
  int count = 0;
  desc.add_options()("filter", po::value<std::string>(&filter), "Filter")("count", po::value<int>(&count)->default_value(3), "Count");

  const PB::Registry::ParameterDetails details = parse_help_pb(desc);
  ASSERT_NE(nullptr, find_parameter(details, "filter"));
  EXPECT_EQ(PB::Common::STRING, find_parameter(details, "filter")->content_type());
  ASSERT_NE(nullptr, find_parameter(details, "count"));
  EXPECT_EQ(PB::Common::STRING, find_parameter(details, "count")->content_type());
  EXPECT_EQ("3", find_parameter(details, "count")->default_value());
}

TEST(program_options_help_pb, carries_the_filter_keywords_with_their_descriptions) {
  // The other half of the payload: the filter keywords, which the registry
  // spells with a trailing "()" when they are functions.
  po::options_description desc;
  npo::field_map fields;
  fields["free"] = "Free space";
  fields["convert_bytes()"] = "Convert a byte value";

  PB::Registry::ParameterDetails details;
  ASSERT_TRUE(details.ParseFromString(npo::help_pb(desc, fields)));
  ASSERT_EQ(2, details.fields_size());
  std::map<std::string, std::string> seen;
  for (const PB::Registry::FieldDetail &f : details.fields()) seen[f.name()] = f.long_description();
  EXPECT_EQ("Free space", seen["free"]);
  EXPECT_EQ("Convert a byte value", seen["convert_bytes()"]);
}

}  // namespace
