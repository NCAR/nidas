#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/Variable.h>

using namespace nidas::util;
using namespace nidas::core;
using namespace std;

BOOST_AUTO_TEST_CASE(test_variable_attributes)
{
    Variable t;
    t.setName("T");

    t.setAttribute(Parameter("x", 1));
    t.setAttribute(Parameter("y", 2));

    std::vector<Parameter> xatts{
        Parameter("x", 1),
        Parameter("y", 2),
    };

    BOOST_TEST(t.getAttributes() == xatts);

    t.removeAttribute("x");
    t.setAttribute(Parameter("x", 1));

    std::vector<Parameter> xatts2{
        Parameter("y", 2),
        Parameter("x", 1),
    };

    BOOST_TEST(t.getAttributes() == xatts2);

    t.removeAttribute("x");
    BOOST_TEST(t.getAttributes() == vector<Parameter>{ Parameter("y", 2) });

    t.removeAttribute("z");
    BOOST_TEST(t.getAttributes() == vector<Parameter>{ Parameter("y", 2) });

    t.removeAttribute("y");
    BOOST_TEST(t.getAttributes() == vector<Parameter>{});

    t.removeAttribute("a");
    BOOST_TEST(t.getAttributes() == vector<Parameter>{});
}
