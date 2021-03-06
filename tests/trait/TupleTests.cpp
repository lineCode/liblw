
#include <gtest/gtest.h>
#include <string>
#include <tuple>

#include "lw/trait.hpp"

using namespace std;

namespace lw {
namespace tests {

#define LW_DEFINE_GET_TYPE(T) std::string get_type(const T&){ return #T; }
#define LW_DEFINE_CHECK_VALUE(T) bool check_value(const T& val){ return val == T##_val; }
#define LW_DEFINE_TYPE_METHODS(T) LW_DEFINE_GET_TYPE(T) LW_DEFINE_CHECK_VALUE(T)
#define LW_DEFINE_GET_TUPLE_TYPES(...) \
    std::string get_tuple_types(const std::tuple<__VA_ARGS__>&){ return #__VA_ARGS__; }

struct TupleTests : public testing::Test {
    typedef std::tuple<double, int, std::string> tuple_type;
    tuple_type tup = std::make_tuple(3.14, 42, "Hello, World!");
    const tuple_type const_tup = std::make_tuple(3.14, 42, "Hello, World!");
    int int_val = 42;
    float float_val = 6.28;
    double double_val = 3.14;
    std::string string_val = "Hello, World!";

    LW_DEFINE_TYPE_METHODS(int)
    LW_DEFINE_TYPE_METHODS(double)
    LW_DEFINE_TYPE_METHODS(float)
    LW_DEFINE_TYPE_METHODS(string)

    LW_DEFINE_GET_TUPLE_TYPES(double, int, std::string)
    LW_DEFINE_GET_TUPLE_TYPES(double, std::string)
    LW_DEFINE_GET_TUPLE_TYPES(int, std::string)
    LW_DEFINE_GET_TUPLE_TYPES(double, int)
};

// ---------------------------------------------------------------------------------------------- //

TEST_F(TupleTests, ForEach){
    int call_count = 0;
    trait::for_each(tup, [&](auto& val){
        ++call_count;
        EXPECT_LT(call_count, 4);
        if (call_count == 1) {
            EXPECT_EQ("double", this->get_type(val)) << "On iteration " << call_count;
        }
        else if (call_count == 2) {
            EXPECT_EQ("int", this->get_type(val)) << "On iteration " << call_count;
        }
        else if (call_count == 3) {
            EXPECT_EQ("string", this->get_type(val)) << "On iteration " << call_count;
        }
        EXPECT_TRUE(this->check_value(val)) << "On iteration " << call_count;
    });
}

// ---------------------------------------------------------------------------------------------- //

TEST_F(TupleTests, ConstForEach){
    int call_count = 0;
    trait::for_each(const_tup, [&](const auto& val){
        ++call_count;
        EXPECT_LT(call_count, 4);
        if (call_count == 1) {
            EXPECT_EQ("double", this->get_type(val)) << "On iteration " << call_count;
        }
        else if (call_count == 2) {
            EXPECT_EQ("int", this->get_type(val)) << "On iteration " << call_count;
        }
        else if (call_count == 3) {
            EXPECT_EQ("string", this->get_type(val)) << "On iteration " << call_count;
        }
        EXPECT_TRUE(this->check_value(val)) << "On iteration " << call_count;
    });
}

// ---------------------------------------------------------------------------------------------- //

TEST_F(TupleTests, RemoveType){
    EXPECT_EQ("double, int", get_tuple_types(trait::remove_type<std::string, tuple_type>::type()));
    EXPECT_EQ("double, std::string", get_tuple_types(trait::remove_type<int, tuple_type>::type()));
    EXPECT_EQ("int, std::string", get_tuple_types(trait::remove_type<double, tuple_type>::type()));
}

}
}
