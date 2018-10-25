#include "data_base.h"
#include <vector>
#include <string>

#define BOOST_TEST_MODULE test_main

#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;
BOOST_AUTO_TEST_SUITE(test_suite_main)

static std::vector<std::string> initialData {
    "INSERT A 0 lean",
    "INSERT A 1 sweater",
    "INSERT A 2 frank",
    "INSERT A 3 violation",
    "INSERT A 4 quality",
    "INSERT A 5 precision",
    "INSERT B 3 proposal",
    "INSERT B 4 example",
    "INSERT B 5 lake",
    "INSERT B 6 flour",
    "INSERT B 7 wonder",
    "INSERT B 8 selection",
};

constexpr const char* operation1 = "INTERSECTION";
constexpr const char* operation2 = "SYMMETRIC_DIFFERENCE";

static std::string intersection_result ("3,violation,proposal\n4,quality,example\n5,precision,lake\nOK\n");
static std::string simmdiff_result ("0,lean,\n1,sweater,\n2,frank,\n6,,flour\n7,,wonder\n8,,selection\nOK\n");

BOOST_AUTO_TEST_CASE(check_db_handling)
{
    DataBase db;
    size_t id = 0;
    for(const auto& command: initialData) {
        id = db.newDataHandling(command.c_str());
        BOOST_CHECK_MESSAGE(db.getResult(id) == "OK\n", "wrong result of command" << command);
    }

    std::cout << "main thread " << std::this_thread::get_id() << std::endl;
    id = db.newDataHandling(operation1);
    BOOST_CHECK_MESSAGE(db.getResult(id) == intersection_result, "\nwrong result of intersection ");

    id = db.newDataHandling(operation2);
    BOOST_CHECK_MESSAGE(db.getResult(id) == simmdiff_result, "\nwrong result of simm diff ");

    id = db.newDataHandling("INSERT A 0 understand");
    BOOST_CHECK(id == 0);
    BOOST_CHECK_MESSAGE(db.getResult(id) == "ERR duplicate 0\n", "\nwrong result of bad attempt ");
}

BOOST_AUTO_TEST_SUITE_END()
