#include "tracker/cli.hpp"

#include <gtest/gtest.h>

#include <string>

#include "tracker/version.hpp"

using tracker::Command;
using tracker::parse_args;

TEST(ParseArgs, NoArgsDefaultsToRun) {
    const auto result = parse_args({});
    ASSERT_TRUE(result.options.has_value());
    EXPECT_EQ(result.options->command, Command::Run);
    EXPECT_FALSE(result.options->console);
    EXPECT_TRUE(result.error.empty());
}

TEST(ParseArgs, HelpLongAndShort) {
    for (const std::string flag : {"--help", "-h"}) {
        const auto result = parse_args({flag});
        ASSERT_TRUE(result.options.has_value()) << flag;
        EXPECT_EQ(result.options->command, Command::Help) << flag;
    }
}

TEST(ParseArgs, VersionLongAndShort) {
    for (const std::string flag : {"--version", "-v"}) {
        const auto result = parse_args({flag});
        ASSERT_TRUE(result.options.has_value()) << flag;
        EXPECT_EQ(result.options->command, Command::Version) << flag;
    }
}

TEST(ParseArgs, ConsoleFlagCombinesWithRun) {
    const auto result = parse_args({"--console"});
    ASSERT_TRUE(result.options.has_value());
    EXPECT_EQ(result.options->command, Command::Run);
    EXPECT_TRUE(result.options->console);
}

TEST(ParseArgs, ConsoleFlagCombinesWithCommand) {
    const auto result = parse_args({"--console", "--version"});
    ASSERT_TRUE(result.options.has_value());
    EXPECT_EQ(result.options->command, Command::Version);
    EXPECT_TRUE(result.options->console);
}

TEST(ParseArgs, UnknownOptionIsError) {
    const auto result = parse_args({"--bogus"});
    EXPECT_FALSE(result.options.has_value());
    EXPECT_NE(result.error.find("--bogus"), std::string::npos);
}

TEST(ParseArgs, ConflictingCommandsAreError) {
    const auto result = parse_args({"--help", "--version"});
    EXPECT_FALSE(result.options.has_value());
    EXPECT_NE(result.error.find("conflicting"), std::string::npos);
}

TEST(Usage, MentionsVersionAndAllOptions) {
    const std::string text = tracker::usage();
    EXPECT_NE(text.find(tracker::kVersion), std::string::npos);
    for (const char* opt : {"--help", "--version", "--console", "--install", "--uninstall", "--publish-now"}) {
        EXPECT_NE(text.find(opt), std::string::npos) << opt;
    }
}
