/*
 * Copyright Krzysztof Sinica 2015.
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#define CATCH_CONFIG_MAIN
#include <vector>
#include <catch.hpp>
#include <rxlog/v1/rx-log.hpp>

TEST_CASE("message can be logged using distinct severity levels") {
    GIVEN("a logger instance") {
        rxlog::logger logger{};
        std::vector<rxlog::record> result{};
        logger.on_record()
            .subscribe([&result](auto record) {
                result.push_back(record);
            });
        WHEN("message is logged using distinct severity levels") {
            LOG_DEBUG(logger) << "Debug message";
            LOG_INFO(logger) << "Info message";
            LOG_ERROR(logger) << "Error message";
            THEN("result turns out as expected") {
                const std::vector<rxlog::record> expected {
                    {rxlog::severity::debug, "Debug message"},
                    {rxlog::severity::info, "Info message"},
                    {rxlog::severity::error, "Error message"}
                };
                REQUIRE(result == expected);
            }
        }
    }
}

TEST_CASE("logger handles stream manipulators") {
    GIVEN("a logger instance") {
        rxlog::logger logger{};
        std::vector<rxlog::record> result{};
        logger.on_record()
            .subscribe([&result](auto record) {
                result.push_back(record);
            });
        WHEN("message is logged using stream manipulators") {
            LOG_INFO(logger) << std::nouppercase << std::setfill('0')
                             << std::hex << std::setw(5) << 54321;
            THEN("result turns out as expected") {
                REQUIRE(result.at(0).message == "0d431");
            }
        }
    }
}
