#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "test_util.h"
#include "kota/deco/option.h"
#include "kota/zest/zest.h"

namespace kota::option {
namespace {

using namespace std::literals::string_view_literals;
using test::parse_all;
using test::ParseCapture;
using test::split2vec;

enum MainOptionID {
    MAIN_OPT_INVALID = 0,
    MAIN_OPT_INPUT = 1,
    MAIN_OPT_UNKNOWN = 2,
    MAIN_OPT_HELP,
    MAIN_OPT_HELP_SHORT,
    MAIN_OPT_SCRIPT,
};

constexpr auto kMainOptInfos = std::array{
    Option::input(MAIN_OPT_INPUT),
    Option::unknown(MAIN_OPT_UNKNOWN),
    Option::unaliased_one(pfx_double, "--help", MAIN_OPT_HELP, Kind::Flag, 0, "Display help", ""),
    Option::unaliased_one(pfx_dash, "-h", MAIN_OPT_HELP_SHORT, Kind::Flag, 0, "Display help", "")
        .alias_of(MAIN_OPT_HELP),
    Option::unaliased_one(pfx_dash, "-s", MAIN_OPT_SCRIPT, Kind::Separate, 1, "Script path", ""),
};

OptTable make_main_opt_table() {
    return OptTable(std::span<const Option>(kMainOptInfos));
}

ParseOptions make_main_parse_options() {
    ParseOptions opts;
    opts.dash_dash_parsing = true;
    opts.dash_dash_packing = true;
    return opts;
}

enum ProxyOptionID {
    PROXY_OPT_INVALID = 0,
    PROXY_OPT_INPUT = 1,
    PROXY_OPT_UNKNOWN = 2,
    PROXY_OPT_PARENT_ID,
    PROXY_OPT_EXEC,
};

constexpr auto kProxyOptInfos = std::array{
    Option::input(PROXY_OPT_INPUT),
    Option::unknown(PROXY_OPT_UNKNOWN),
    Option::unaliased_one(pfx_dash,
                          "-p",
                          PROXY_OPT_PARENT_ID,
                          Kind::Separate,
                          1,
                          "Parent process id",
                          ""),
    Option::unaliased_one(pfx_dash_double, "--exec", PROXY_OPT_EXEC, Kind::Separate, 1, "Exec", ""),
};

OptTable make_proxy_opt_table() {
    return OptTable(std::span<const Option>(kProxyOptInfos));
}

ParseOptions make_proxy_parse_options() {
    ParseOptions opts;
    opts.dash_dash_parsing = true;
    opts.dash_dash_packing = true;
    opts.greedy_unknown = true;
    return opts;
}

TEST_SUITE(option_parse_view) {

TEST_CASE(main_option_table_basic) {
    auto table = make_main_opt_table();
    auto opts = make_main_parse_options();
    auto parsed = parse_all(
        table,
        split2vec("-p 1234 -s script::profile --dest=114514 -- /usr/bin/clang++ --version"),
        opts);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 5U);

    EXPECT_EQ(parsed.args[0].id, MAIN_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "-p");
    EXPECT_EQ(parsed.args[0].index, 0U);

    EXPECT_EQ(parsed.args[1].id, MAIN_OPT_INPUT);
    EXPECT_EQ(parsed.args[1].spelling, "1234");
    EXPECT_EQ(parsed.args[1].index, 1U);

    EXPECT_EQ(parsed.args[2].id, MAIN_OPT_SCRIPT);
    ASSERT_EQ(parsed.args[2].values.size(), 1U);
    EXPECT_EQ(parsed.args[2].values[0], "script::profile");
    EXPECT_EQ(parsed.args[2].spelling, "-s");
    EXPECT_EQ(parsed.args[2].index, 2U);

    EXPECT_EQ(parsed.args[3].id, MAIN_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[3].spelling, "--dest=114514");
    EXPECT_EQ(parsed.args[3].index, 4U);

    EXPECT_EQ(parsed.args[4].id, MAIN_OPT_INPUT);
    EXPECT_EQ(parsed.args[4].spelling, "--");
    ASSERT_EQ(parsed.args[4].values.size(), 2U);
    EXPECT_EQ(parsed.args[4].values[0], "/usr/bin/clang++");
    EXPECT_EQ(parsed.args[4].values[1], "--version");
    EXPECT_EQ(parsed.args[4].index, 5U);
}

TEST_CASE(alias_resolves_to_canonical) {
    auto table = make_main_opt_table();
    auto opts = make_main_parse_options();
    auto parsed = parse_all(table, split2vec("-h"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, MAIN_OPT_HELP);
    EXPECT_EQ(parsed.args[0].spelling, "-h");
    EXPECT_EQ(parsed.args[0].values.size(), 0U);
}

TEST_CASE(proxy_option_table_basic) {
    auto table = make_proxy_opt_table();
    auto opts = make_proxy_parse_options();

    auto parsed = parse_all(table, split2vec("-p 1234"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_PARENT_ID);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "1234");
    EXPECT_EQ(parsed.args[0].spelling, "-p");
    EXPECT_EQ(parsed.args[0].index, 0U);

    parsed = parse_all(table, split2vec("--exec /bin/ls"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_EXEC);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "/bin/ls");
    EXPECT_EQ(parsed.args[0].spelling, "--exec");
    EXPECT_EQ(parsed.args[0].index, 0U);

    parsed =
        parse_all(table, split2vec("-p 12 --exec /usr/bin/clang++ -- clang++ --version"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_PARENT_ID);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "12");
    EXPECT_EQ(parsed.args[0].index, 0U);

    EXPECT_EQ(parsed.args[1].id, PROXY_OPT_EXEC);
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "/usr/bin/clang++");
    EXPECT_EQ(parsed.args[1].index, 2U);

    EXPECT_EQ(parsed.args[2].id, PROXY_OPT_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "--");
    ASSERT_EQ(parsed.args[2].values.size(), 2U);
    EXPECT_EQ(parsed.args[2].values[0], "clang++");
    EXPECT_EQ(parsed.args[2].values[1], "--version");
    EXPECT_EQ(parsed.args[2].index, 4U);
}

TEST_CASE(proxy_missing_value_error) {
    auto table = make_proxy_opt_table();
    auto opts = make_proxy_parse_options();
    auto parsed = parse_all(table, split2vec("-p"), opts);
    EXPECT_EQ(parsed.args.size(), 0U);
    ASSERT_EQ(parsed.errors.size(), 1U);
    EXPECT_TRUE(std::string_view(parsed.errors[0].message).contains("missing"));
}

TEST_CASE(unknown_consumes_until_known) {
    auto table = make_proxy_opt_table();
    auto opts = make_proxy_parse_options();
    auto parsed = parse_all(table, split2vec("--unknown-cmd xxx yyy -p 1234"), opts);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);

    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "--unknown-cmd");
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "xxx");
    EXPECT_EQ(parsed.args[0].values[1], "yyy");

    EXPECT_EQ(parsed.args[1].id, PROXY_OPT_PARENT_ID);
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "1234");
}

TEST_CASE(unknown_consumes_all_when_no_known_follows) {
    auto table = make_proxy_opt_table();
    auto opts = make_proxy_parse_options();
    auto parsed = parse_all(table, split2vec("--unknown xxx yyy"), opts);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);

    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "--unknown");
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "xxx");
    EXPECT_EQ(parsed.args[0].values[1], "yyy");
}

TEST_CASE(consecutive_unknown_prefixed) {
    auto table = make_proxy_opt_table();
    auto opts = make_proxy_parse_options();
    auto parsed = parse_all(table, split2vec("--unknown1 --unknown2 -p 1234"), opts);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);

    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "--unknown1");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "--unknown2");

    EXPECT_EQ(parsed.args[1].id, PROXY_OPT_PARENT_ID);
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "1234");
}

TEST_CASE(unknown_prefix_match_not_treated_as_boundary) {
    auto table = make_proxy_opt_table();
    auto opts = make_proxy_parse_options();
    auto parsed = parse_all(table, split2vec("--unknown --execute-thing -p 1234"), opts);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);

    EXPECT_EQ(parsed.args[0].id, PROXY_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "--unknown");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "--execute-thing");

    EXPECT_EQ(parsed.args[1].id, PROXY_OPT_PARENT_ID);
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "1234");
}

};  // TEST_SUITE(option_parse_view)

enum GroupedOptionID {
    GROUPED_OPT_INVALID = 0,
    GROUPED_OPT_INPUT = 1,
    GROUPED_OPT_UNKNOWN = 2,
    GROUPED_OPT_A,
    GROUPED_OPT_B,
};

constexpr auto kGroupedOptInfos = std::array{
    Option::input(GROUPED_OPT_INPUT),
    Option::unknown(GROUPED_OPT_UNKNOWN),
    Option::unaliased_one(pfx_dash, "-a", GROUPED_OPT_A, Kind::Flag, 0, "", ""),
    Option::unaliased_one(pfx_dash, "-b", GROUPED_OPT_B, Kind::Flag, 0, "", ""),
};

OptTable make_grouped_opt_table() {
    return OptTable(std::span<const Option>(kGroupedOptInfos));
}

ParseOptions make_grouped_parse_options() {
    ParseOptions opts;
    opts.grouped_short_options = true;
    return opts;
}

enum IgnoreCaseOptionID {
    IGNORE_CASE_OPT_INVALID = 0,
    IGNORE_CASE_OPT_INPUT = 1,
    IGNORE_CASE_OPT_UNKNOWN = 2,
    IGNORE_CASE_OPT_HELP,
};

constexpr auto kIgnoreCaseOptInfos = std::array{
    Option::input(IGNORE_CASE_OPT_INPUT),
    Option::unknown(IGNORE_CASE_OPT_UNKNOWN),
    Option::unaliased_one(pfx_double, "--help", IGNORE_CASE_OPT_HELP, Kind::Flag, 0, "", ""),
};

OptTable make_ignore_case_opt_table(bool ignore_case) {
    return OptTable(std::span<const Option>(kIgnoreCaseOptInfos), ignore_case);
}

enum FilterOptionID {
    FILTER_OPT_INVALID = 0,
    FILTER_OPT_INPUT = 1,
    FILTER_OPT_UNKNOWN = 2,
    FILTER_OPT_PUBLIC,
    FILTER_OPT_HIDDEN,
    FILTER_OPT_FLAGGED,
};

constexpr std::uint32_t kInternalVisibility = 1U << 1;
constexpr std::uint32_t kExperimentalFlag = 1U << 6;

constexpr auto kFilterOptInfos = std::array{
    Option::input(FILTER_OPT_INPUT),
    Option::unknown(FILTER_OPT_UNKNOWN),
    Option::unaliased_one(pfx_double, "--public", FILTER_OPT_PUBLIC, Kind::Flag, 0, "", ""),
    Option::unaliased_one(pfx_double,
                          "--hidden",
                          FILTER_OPT_HIDDEN,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_double,
                          "--flagged",
                          FILTER_OPT_FLAGGED,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          kExperimentalFlag,
                          DefaultVis),
};

OptTable make_filter_opt_table() {
    return OptTable(std::span<const Option>(kFilterOptInfos));
}

enum SkipOptionID {
    SKIP_OPT_INVALID = 0,
    SKIP_OPT_INPUT = 1,
    SKIP_OPT_UNKNOWN = 2,
    SKIP_OPT_VISIBLE_FLAG,      // -v   Flag, DefaultVis
    SKIP_OPT_HIDDEN_FLAG,       // -x   Flag, kInternalVisibility
    SKIP_OPT_HIDDEN_JOINED,     // -j   Joined, kInternalVisibility
    SKIP_OPT_HIDDEN_SEPARATE,   // -o   Separate, kInternalVisibility
    SKIP_OPT_HIDDEN_REMAINING,  // --rem RemainingArgs, kInternalVisibility
    SKIP_OPT_GROUP_A,           // -a   Flag, kInternalVisibility
    SKIP_OPT_GROUP_B,           // -b   Flag, DefaultVis
    SKIP_OPT_HIDDEN_DUP,        // -d   Flag, kInternalVisibility (duplicate name)
    SKIP_OPT_VISIBLE_DUP,       // -d   Flag, DefaultVis (duplicate name)
};

constexpr auto kSkipOptInfos = std::array{
    Option::input(SKIP_OPT_INPUT),
    Option::unknown(SKIP_OPT_UNKNOWN),
    Option::unaliased_one(pfx_dash, "-v", SKIP_OPT_VISIBLE_FLAG, Kind::Flag, 0, "", ""),
    Option::unaliased_one(pfx_dash,
                          "-x",
                          SKIP_OPT_HIDDEN_FLAG,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_dash,
                          "-j",
                          SKIP_OPT_HIDDEN_JOINED,
                          Kind::Joined,
                          1,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_dash,
                          "-o",
                          SKIP_OPT_HIDDEN_SEPARATE,
                          Kind::Separate,
                          1,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_double,
                          "--rem",
                          SKIP_OPT_HIDDEN_REMAINING,
                          Kind::RemainingArgs,
                          0,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_dash,
                          "-a",
                          SKIP_OPT_GROUP_A,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_dash, "-b", SKIP_OPT_GROUP_B, Kind::Flag, 0, "", ""),
    Option::unaliased_one(pfx_dash,
                          "-d",
                          SKIP_OPT_HIDDEN_DUP,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_dash, "-d", SKIP_OPT_VISIBLE_DUP, Kind::Flag, 0, "", ""),
};

OptTable make_skip_opt_table() {
    return OptTable(std::span<const Option>(kSkipOptInfos));
}

ParseOptions make_skip_parse_options() {
    ParseOptions opts;
    opts.visibility = DefaultVis;
    opts.skip_excluded = true;
    return opts;
}

enum GroupedDupOptionID {
    GRPDUP_OPT_INVALID = 0,
    GRPDUP_OPT_INPUT = 1,
    GRPDUP_OPT_UNKNOWN = 2,
    GRPDUP_OPT_A_VISIBLE,  // -a Flag DefaultVis (first)
    GRPDUP_OPT_A_HIDDEN,   // -a Flag kInternalVisibility (second)
    GRPDUP_OPT_B,          // -b Flag DefaultVis
};

constexpr auto kGroupedDupOptInfos = std::array{
    Option::input(GRPDUP_OPT_INPUT),
    Option::unknown(GRPDUP_OPT_UNKNOWN),
    Option::unaliased_one(pfx_dash, "-a", GRPDUP_OPT_A_VISIBLE, Kind::Flag, 0, "", ""),
    Option::unaliased_one(pfx_dash,
                          "-a",
                          GRPDUP_OPT_A_HIDDEN,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_dash, "-b", GRPDUP_OPT_B, Kind::Flag, 0, "", ""),
};

OptTable make_grouped_dup_opt_table() {
    return OptTable(std::span<const Option>(kGroupedDupOptInfos));
}

enum GreedyDupOptionID {
    GREEDY_DUP_OPT_INVALID = 0,
    GREEDY_DUP_OPT_INPUT = 1,
    GREEDY_DUP_OPT_UNKNOWN = 2,
    GREEDY_DUP_OPT_X_HIDDEN,  // -x Flag kInternalVisibility (first)
    GREEDY_DUP_OPT_X_VISIBLE  // -x Flag DefaultVis (second)
};

constexpr auto kGreedyDupOptInfos = std::array{
    Option::input(GREEDY_DUP_OPT_INPUT),
    Option::unknown(GREEDY_DUP_OPT_UNKNOWN),
    Option::unaliased_one(pfx_dash,
                          "-x",
                          GREEDY_DUP_OPT_X_HIDDEN,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          0,
                          kInternalVisibility),
    Option::unaliased_one(pfx_dash, "-x", GREEDY_DUP_OPT_X_VISIBLE, Kind::Flag, 0, "", ""),
};

OptTable make_greedy_dup_opt_table() {
    return OptTable(std::span<const Option>(kGreedyDupOptInfos));
}

enum KindsOptionID {
    KINDS_OPT_INVALID = 0,
    KINDS_OPT_INPUT = 1,
    KINDS_OPT_UNKNOWN = 2,
    KINDS_OPT_JOINED,
    KINDS_OPT_COMMA_JOINED,
    KINDS_OPT_MULTI_ARG,
    KINDS_OPT_JOINED_OR_SEPARATE,
    KINDS_OPT_JOINED_AND_SEPARATE,
    KINDS_OPT_REMAINING,
    KINDS_OPT_REMAINING_JOINED,
};

constexpr auto kKindsOptInfos = std::array{
    Option::input(KINDS_OPT_INPUT),
    Option::unknown(KINDS_OPT_UNKNOWN),
    Option::unaliased_one(pfx_dash, "-j", KINDS_OPT_JOINED, Kind::Joined, 1, "", ""),
    Option::unaliased_one(pfx_double,
                          "--list",
                          KINDS_OPT_COMMA_JOINED,
                          Kind::CommaJoined,
                          1,
                          "",
                          ""),
    Option::unaliased_one(pfx_double, "--pair", KINDS_OPT_MULTI_ARG, Kind::MultiArg, 2, "", ""),
    Option::unaliased_one(pfx_dash,
                          "-o",
                          KINDS_OPT_JOINED_OR_SEPARATE,
                          Kind::JoinedOrSeparate,
                          1,
                          "",
                          ""),
    Option::unaliased_one(pfx_dash,
                          "-x",
                          KINDS_OPT_JOINED_AND_SEPARATE,
                          Kind::JoinedAndSeparate,
                          2,
                          "",
                          ""),
    Option::unaliased_one(pfx_double,
                          "--rest",
                          KINDS_OPT_REMAINING,
                          Kind::RemainingArgs,
                          0,
                          "",
                          ""),
    Option::unaliased_one(pfx_double,
                          "--tail",
                          KINDS_OPT_REMAINING_JOINED,
                          Kind::RemainingArgsJoined,
                          0,
                          "",
                          ""),
};

OptTable make_kinds_opt_table() {
    return OptTable(std::span<const Option>(kKindsOptInfos));
}

enum MatchOptionID {
    MATCH_OPT_INVALID = 0,
    MATCH_OPT_INPUT = 1,
    MATCH_OPT_UNKNOWN = 2,
    MATCH_OPT_GROUP,
    MATCH_OPT_MEMBER,
    MATCH_OPT_ALIAS_MEMBER,
    MATCH_OPT_JOINED,
    MATCH_OPT_OVERRIDE_FLAG,
};

constexpr auto kMatchOptInfos = std::array{
    Option::input(MATCH_OPT_INPUT),
    Option::unknown(MATCH_OPT_UNKNOWN),
    Option::unaliased_one(pfx_none, "group", MATCH_OPT_GROUP, Kind::Group, 0, "", ""),
    Option::unaliased_one(pfx_dash, "-m", MATCH_OPT_MEMBER, Kind::Flag, 0, "", "", MATCH_OPT_GROUP),
    Option::unaliased_one(pfx_dash, "-am", MATCH_OPT_ALIAS_MEMBER, Kind::Flag, 0, "", "")
        .alias_of(MATCH_OPT_MEMBER),
    Option::unaliased_one(pfx_dash, "-j", MATCH_OPT_JOINED, Kind::Joined, 1, "", ""),
    Option::unaliased_one(pfx_dash,
                          "-r",
                          MATCH_OPT_OVERRIDE_FLAG,
                          Kind::Flag,
                          0,
                          "",
                          "",
                          0,
                          RenderJoined),
};

OptTable make_match_opt_table() {
    return OptTable(std::span<const Option>(kMatchOptInfos));
}

enum AliasOptionID {
    ALIAS_OPT_INVALID = 0,
    ALIAS_OPT_INPUT = 1,
    ALIAS_OPT_UNKNOWN = 2,
    ALIAS_OPT_TRAP_EQ,
    ALIAS_OPT_TRAP_DEFAULTS,
    ALIAS_OPT_EMIT_EQ,
    ALIAS_OPT_EMIT_LLVM,
};

constexpr auto kAliasOptInfos = std::array{
    Option::input(ALIAS_OPT_INPUT),
    Option::unknown(ALIAS_OPT_UNKNOWN),
    Option::unaliased_one(pfx_double, "--trap=", ALIAS_OPT_TRAP_EQ, Kind::CommaJoined, 1, "", ""),
    Option::unaliased_one(pfx_double,
                          "--trap-defaults",
                          ALIAS_OPT_TRAP_DEFAULTS,
                          Kind::Flag,
                          0,
                          "",
                          "")
        .alias_of(ALIAS_OPT_TRAP_EQ, "all\0undefined\0"),
    Option::unaliased_one(pfx_double, "--emit=", ALIAS_OPT_EMIT_EQ, Kind::Joined, 1, "", ""),
    Option::unaliased_one(pfx_double, "--emit-llvm", ALIAS_OPT_EMIT_LLVM, Kind::Flag, 0, "", "")
        .alias_of(ALIAS_OPT_EMIT_EQ),
};

OptTable make_alias_opt_table() {
    return OptTable(std::span<const Option>(kAliasOptInfos));
}

TEST_SUITE(option_extended_coverage) {

TEST_CASE(ignore_case_controls_matching) {
    auto strict_table = make_ignore_case_opt_table(false);
    auto parsed = parse_all(strict_table, split2vec("--HELP"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, IGNORE_CASE_OPT_UNKNOWN);

    auto ignore_case_table = make_ignore_case_opt_table(true);
    parsed = parse_all(ignore_case_table, split2vec("--HELP"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, IGNORE_CASE_OPT_HELP);
}

TEST_CASE(grouped_short_option_parsing) {
    auto table = make_grouped_opt_table();
    auto opts = make_grouped_parse_options();

    auto parsed = parse_all(table, split2vec("-ab"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);
    EXPECT_EQ(parsed.args[0].id, GROUPED_OPT_A);
    EXPECT_EQ(parsed.args[1].id, GROUPED_OPT_B);
}

TEST_CASE(grouped_unknown_splits) {
    auto table = make_grouped_opt_table();
    auto opts = make_grouped_parse_options();

    auto parsed = parse_all(table, split2vec("-zx"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);
    EXPECT_EQ(parsed.args[0].id, GROUPED_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[1].id, GROUPED_OPT_UNKNOWN);
}

TEST_CASE(grouped_equals_form_is_unknown) {
    auto table = make_grouped_opt_table();
    auto opts = make_grouped_parse_options();

    auto parsed = parse_all(table, split2vec("-a=1"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, GROUPED_OPT_UNKNOWN);
}

TEST_CASE(visibility_filter_affects_matching) {
    auto table = make_filter_opt_table();

    auto argv = split2vec("--hidden");
    ParseCapture default_vis;
    ParseOptions default_opts;
    default_opts.visibility = DefaultVis;
    for(auto& result: table.parse(argv, default_opts)) {
        if(result.has_value())
            default_vis.args.push_back(*result);
        else
            default_vis.errors.push_back(result.error());
    }
    EXPECT_TRUE(default_vis.errors.empty());
    ASSERT_EQ(default_vis.args.size(), 1U);
    EXPECT_EQ(default_vis.args[0].id, FILTER_OPT_UNKNOWN);

    argv = split2vec("--hidden");
    ParseCapture capture;
    ParseOptions internal_opts;
    internal_opts.visibility = kInternalVisibility;
    for(auto& result: table.parse(argv, internal_opts)) {
        if(result.has_value())
            capture.args.push_back(*result);
        else
            capture.errors.push_back(result.error());
    }
    EXPECT_TRUE(capture.errors.empty());
    ASSERT_EQ(capture.args.size(), 1U);
    EXPECT_EQ(capture.args[0].id, FILTER_OPT_HIDDEN);
}

TEST_CASE(flag_filter_affects_matching) {
    auto table = make_filter_opt_table();

    auto argv = split2vec("--flagged");
    ParseCapture include_capture;
    ParseOptions include_opts;
    include_opts.include_flags = kExperimentalFlag;
    for(auto& result: table.parse(argv, include_opts)) {
        if(result.has_value())
            include_capture.args.push_back(*result);
        else
            include_capture.errors.push_back(result.error());
    }
    EXPECT_TRUE(include_capture.errors.empty());
    ASSERT_EQ(include_capture.args.size(), 1U);
    EXPECT_EQ(include_capture.args[0].id, FILTER_OPT_FLAGGED);

    argv = split2vec("--flagged");
    ParseCapture exclude_capture;
    ParseOptions exclude_opts;
    exclude_opts.exclude_flags = kExperimentalFlag;
    for(auto& result: table.parse(argv, exclude_opts)) {
        if(result.has_value())
            exclude_capture.args.push_back(*result);
        else
            exclude_capture.errors.push_back(result.error());
    }
    EXPECT_TRUE(exclude_capture.errors.empty());
    ASSERT_EQ(exclude_capture.args.size(), 1U);
    EXPECT_EQ(exclude_capture.args[0].id, FILTER_OPT_UNKNOWN);
}

TEST_CASE(skip_excluded_hides_matches_and_values) {
    auto table = make_skip_opt_table();
    auto opts = make_skip_parse_options();

    // Hidden options are consumed (with their full values) but never surface,
    // and the surrounding inputs/options are unaffected.
    auto parsed =
        parse_all(table, split2vec("-v -x main.cc -jfoo -o out.o tail.cc --rem a b"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);
    EXPECT_EQ(parsed.args[0].id, SKIP_OPT_VISIBLE_FLAG);
    EXPECT_EQ(parsed.args[0].spelling, "-v");
    EXPECT_EQ(parsed.args[1].id, SKIP_OPT_INPUT);
    EXPECT_EQ(parsed.args[1].spelling, "main.cc");
    EXPECT_EQ(parsed.args[2].id, SKIP_OPT_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "tail.cc");
}

TEST_CASE(skip_excluded_default_off_preserves_unknown_behavior) {
    auto table = make_skip_opt_table();
    auto opts = make_skip_parse_options();
    opts.skip_excluded = false;

    // Without skip_excluded, hidden options degrade to unknown/input exactly
    // as before this feature.
    auto parsed = parse_all(table, split2vec("-x -jfoo -o out.o"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 4U);
    EXPECT_EQ(parsed.args[0].id, SKIP_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "-x");
    EXPECT_EQ(parsed.args[1].id, SKIP_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[1].spelling, "-jfoo");
    EXPECT_EQ(parsed.args[2].id, SKIP_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[2].spelling, "-o");
    EXPECT_EQ(parsed.args[3].id, SKIP_OPT_INPUT);
    EXPECT_EQ(parsed.args[3].spelling, "out.o");
}

TEST_CASE(skip_excluded_missing_value_is_silent) {
    auto table = make_skip_opt_table();
    auto opts = make_skip_parse_options();

    // A hidden Separate option at end-of-argv must not report a missing-value
    // error; it is simply consumed.
    auto parsed = parse_all(table, split2vec("-o"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    EXPECT_TRUE(parsed.args.empty());
}

TEST_CASE(skip_excluded_keeps_visible_missing_value_errors) {
    auto table = make_proxy_opt_table();
    auto opts = make_proxy_parse_options();
    opts.skip_excluded = true;

    // The visibility filter only silences excluded options; visible options
    // still report missing values.
    auto parsed = parse_all(table, split2vec("-p"), opts);
    EXPECT_TRUE(parsed.args.empty());
    ASSERT_EQ(parsed.errors.size(), 1U);
    EXPECT_TRUE(std::string_view(parsed.errors[0].message).contains("missing"));
}

TEST_CASE(skip_excluded_grouped_short_options) {
    auto table = make_skip_opt_table();
    auto opts = make_skip_parse_options();
    opts.grouped_short_options = true;

    // "-ab" expands to hidden "-a" (consumed silently) + visible "-b".
    auto parsed = parse_all(table, split2vec("-ab"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, SKIP_OPT_GROUP_B);
    EXPECT_EQ(parsed.args[0].spelling, "-b");
}

TEST_CASE(skip_excluded_visible_duplicate_wins) {
    auto table = make_skip_opt_table();
    auto opts = make_skip_parse_options();

    // The hidden "-d" entry comes first in the table; the visible duplicate
    // with the same spelling must still win instead of being swallowed.
    auto parsed = parse_all(table, split2vec("-d"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, SKIP_OPT_VISIBLE_DUP);
    EXPECT_EQ(parsed.args[0].spelling, "-d");
}

TEST_CASE(skip_excluded_visible_duplicate_wins_tablegen) {
    auto table = make_skip_opt_table();
    table.tablegen_mode = true;
    auto opts = make_skip_parse_options();

    // Same as above but exercising the tablegen binary-search scan path used
    // by catter's generated LLVM option tables.
    auto parsed = parse_all(table, split2vec("-d"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, SKIP_OPT_VISIBLE_DUP);
}

TEST_CASE(skip_excluded_all_duplicates_hidden) {
    auto table = make_skip_opt_table();
    auto opts = make_skip_parse_options();
    // Neither duplicate carries this visibility bit, so both are excluded.
    opts.visibility = kExperimentalFlag;

    // When every duplicate is excluded, the argument is consumed silently.
    auto parsed = parse_all(table, split2vec("-d"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    EXPECT_TRUE(parsed.args.empty());
}

TEST_CASE(skip_excluded_grouped_visible_duplicate_wins) {
    auto table = make_grouped_dup_opt_table();
    auto opts = make_skip_parse_options();
    opts.grouped_short_options = true;

    // "-ab" expands to visible "-a" + "-b"; the hidden duplicate of "-a" that
    // follows it in the table must not steal the grouped fallback.
    auto parsed = parse_all(table, split2vec("-ab"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);
    EXPECT_EQ(parsed.args[0].id, GRPDUP_OPT_A_VISIBLE);
    EXPECT_EQ(parsed.args[0].spelling, "-a");
    EXPECT_EQ(parsed.args[1].id, GRPDUP_OPT_B);
    EXPECT_EQ(parsed.args[1].spelling, "-b");
}

TEST_CASE(greedy_unknown_default_keeps_visible_duplicate_boundary) {
    auto table = make_greedy_dup_opt_table();
    ParseOptions opts;
    opts.greedy_unknown = true;
    opts.visibility = DefaultVis;
    // skip_excluded stays false (default).

    // The hidden "-x" precedes the visible "-x"; greedy unknown must still
    // stop before it and yield the visible option instead of swallowing it.
    auto parsed = parse_all(table, split2vec("--unknown -x"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);
    EXPECT_EQ(parsed.args[0].id, GREEDY_DUP_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "--unknown");
    EXPECT_TRUE(parsed.args[0].values.empty());
    EXPECT_EQ(parsed.args[1].id, GREEDY_DUP_OPT_X_VISIBLE);
    EXPECT_EQ(parsed.args[1].spelling, "-x");
}

TEST_CASE(greedy_unknown_skip_excluded_stops_at_hidden_option) {
    auto table = make_greedy_dup_opt_table();
    ParseOptions opts;
    opts.greedy_unknown = true;
    // Neither duplicate carries this visibility bit, so both are excluded.
    opts.visibility = kExperimentalFlag;
    opts.skip_excluded = true;

    // Both "-x" entries are excluded here; greedy unknown stops at "-x", which
    // is then consumed silently, leaving "tail" as the only input.
    auto parsed = parse_all(table, split2vec("--unknown -x tail"), opts);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);
    EXPECT_EQ(parsed.args[0].id, GREEDY_DUP_OPT_UNKNOWN);
    EXPECT_EQ(parsed.args[0].spelling, "--unknown");
    EXPECT_TRUE(parsed.args[0].values.empty());
    EXPECT_EQ(parsed.args[1].id, GREEDY_DUP_OPT_INPUT);
    EXPECT_EQ(parsed.args[1].spelling, "tail");
}

TEST_CASE(option_kinds_parse_correctly) {
    auto table = make_kinds_opt_table();

    auto parsed = parse_all(table, split2vec("-jabc"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "abc");

    parsed = parse_all(table, split2vec("--list=a,,b,c"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_COMMA_JOINED);
    ASSERT_EQ(parsed.args[0].values.size(), 3U);
    EXPECT_EQ(parsed.args[0].values[0], "=a");
    EXPECT_EQ(parsed.args[0].values[1], "b");
    EXPECT_EQ(parsed.args[0].values[2], "c");

    parsed = parse_all(table, split2vec("--pair left right"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_MULTI_ARG);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "left");
    EXPECT_EQ(parsed.args[0].values[1], "right");

    parsed = parse_all(table, split2vec("-o2"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED_OR_SEPARATE);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "2");

    parsed = parse_all(table, split2vec("-o 3"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED_OR_SEPARATE);
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "3");

    parsed = parse_all(table, split2vec("-x4 tail"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_JOINED_AND_SEPARATE);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "4");
    EXPECT_EQ(parsed.args[0].values[1], "tail");

    parsed = parse_all(table, split2vec("--rest one two"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_REMAINING);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "one");
    EXPECT_EQ(parsed.args[0].values[1], "two");

    parsed = parse_all(table, split2vec("--tailz one two"));
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, KINDS_OPT_REMAINING_JOINED);
    ASSERT_EQ(parsed.args[0].values.size(), 3U);
    EXPECT_EQ(parsed.args[0].values[0], "z");
    EXPECT_EQ(parsed.args[0].values[1], "one");
    EXPECT_EQ(parsed.args[0].values[2], "two");
}

TEST_CASE(option_matches_and_render_style) {
    auto table = make_match_opt_table();

    const auto member = table.option(MATCH_OPT_MEMBER);
    const auto alias = table.option(MATCH_OPT_ALIAS_MEMBER);
    EXPECT_TRUE(member->matches(MATCH_OPT_GROUP));
    EXPECT_TRUE(alias->matches(MATCH_OPT_GROUP));
    EXPECT_EQ(alias->unaliased_option().id(), MATCH_OPT_MEMBER);

    EXPECT_EQ(table.option(MATCH_OPT_JOINED)->render_style(), RenderStyle::Joined);
    EXPECT_EQ(table.option(MATCH_OPT_MEMBER)->render_style(), RenderStyle::Separate);
    EXPECT_EQ(table.option(MATCH_OPT_OVERRIDE_FLAG)->render_style(), RenderStyle::Joined);
}

TEST_CASE(flag_aliases_merge_values) {
    auto table = make_alias_opt_table();
    auto parsed = parse_all(table, split2vec("--trap-defaults --emit-llvm"));

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);

    EXPECT_EQ(parsed.args[0].id, ALIAS_OPT_TRAP_EQ);
    ASSERT_EQ(parsed.args[0].values.size(), 2U);
    EXPECT_EQ(parsed.args[0].values[0], "all");
    EXPECT_EQ(parsed.args[0].values[1], "undefined");

    EXPECT_EQ(parsed.args[1].id, ALIAS_OPT_EMIT_EQ);
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "");
}

TEST_CASE(find_option_basic) {
    auto table = make_main_opt_table();

    EXPECT_EQ(table.find_option("--help")->id(), MAIN_OPT_HELP);
    EXPECT_EQ(table.find_option("-h")->id(), MAIN_OPT_HELP);
    EXPECT_EQ(table.find_option("-s")->id(), MAIN_OPT_SCRIPT);
    EXPECT_TRUE(!table.find_option("--nonexistent"));
}

};  // TEST_SUITE(option_extended_coverage)

TEST_SUITE(option_render) {

auto collect(const OptTable& table, const ParsedArg& arg) {
    std::vector<std::string> out;
    auto cb = [&](std::string_view sv) {
        out.push_back(std::string(sv));
    };
    table.render(arg, cb);
    return out;
}

TEST_CASE(render_flag_separate) {
    auto table = make_main_opt_table();
    auto parsed = parse_all(table, split2vec("--help"));
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "--help");
}

TEST_CASE(render_alias_uses_canonical_name) {
    auto table = make_main_opt_table();
    auto parsed = parse_all(table, split2vec("-h"));
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, MAIN_OPT_HELP);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "--help");
}

TEST_CASE(render_separate_option) {
    auto table = make_main_opt_table();
    auto opts = make_main_parse_options();
    auto parsed = parse_all(table, split2vec("-s script.py"), opts);
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 2U);
    EXPECT_EQ(rendered[0], "-s");
    EXPECT_EQ(rendered[1], "script.py");
}

TEST_CASE(render_joined_option) {
    auto table = make_kinds_opt_table();
    auto parsed = parse_all(table, split2vec("-jabc"));
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "-jabc");
}

TEST_CASE(render_comma_joined_option) {
    auto table = make_kinds_opt_table();
    auto parsed = parse_all(table, split2vec("--list=a,b,c"));
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "--list=a,b,c");
}

TEST_CASE(render_multi_arg_option) {
    auto table = make_kinds_opt_table();
    auto parsed = parse_all(table, split2vec("--pair left right"));
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 3U);
    EXPECT_EQ(rendered[0], "--pair");
    EXPECT_EQ(rendered[1], "left");
    EXPECT_EQ(rendered[2], "right");
}

TEST_CASE(render_joined_or_separate_as_joined) {
    auto table = make_kinds_opt_table();
    auto parsed = parse_all(table, split2vec("-o2"));
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 2U);
    EXPECT_EQ(rendered[0], "-o");
    EXPECT_EQ(rendered[1], "2");
}

TEST_CASE(render_joined_and_separate) {
    auto table = make_kinds_opt_table();
    auto parsed = parse_all(table, split2vec("-x4 tail"));
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 2U);
    EXPECT_EQ(rendered[0], "-x4");
    EXPECT_EQ(rendered[1], "tail");
}

TEST_CASE(render_input_preserves_spelling) {
    auto table = make_main_opt_table();
    auto opts = make_main_parse_options();
    auto parsed = parse_all(table, split2vec("myfile.txt"), opts);
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, MAIN_OPT_INPUT);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "myfile.txt");
}

TEST_CASE(render_unknown_preserves_spelling) {
    auto table = make_main_opt_table();
    auto opts = make_main_parse_options();
    auto parsed = parse_all(table, split2vec("--unknown-flag"), opts);
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, MAIN_OPT_UNKNOWN);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "--unknown-flag");
}

TEST_CASE(render_alias_with_args) {
    auto table = make_alias_opt_table();
    auto parsed = parse_all(table, split2vec("--trap-defaults"));
    ASSERT_EQ(parsed.args.size(), 1U);
    EXPECT_EQ(parsed.args[0].id, ALIAS_OPT_TRAP_EQ);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "--trap=all,undefined");
}

TEST_CASE(render_override_flag_uses_joined_style) {
    auto table = make_match_opt_table();
    auto parsed = parse_all(table, split2vec("-r"));
    ASSERT_EQ(parsed.args.size(), 1U);

    auto rendered = collect(table, parsed.args[0]);
    ASSERT_EQ(rendered.size(), 1U);
    EXPECT_EQ(rendered[0], "-r");
}

};  // TEST_SUITE(option_render)

}  // namespace
}  // namespace kota::option
