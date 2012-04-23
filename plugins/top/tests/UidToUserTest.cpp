#include <iostream>
#include <string>
#include <vector>

using namespace std;


#include <ModuleLogger.h>
#include <UidToUserTest.h>
#include <UidToUser.h>

CPPUNIT_TEST_SUITE_REGISTRATION(U2UTest);

U2UTest::U2UTest() {
}

U2UTest::~U2UTest() {
}

void U2UTest::setUp() {
}

void U2UTest::tearDown() {
}

void U2UTest::testSimple() {
    UidToUser u2u;
    std::string user_name;
    
    user_name = u2u.get_user(-1);
    CPPUNIT_ASSERT_MESSAGE("translation -1 is not no-leagal", user_name.compare("not-leagal") == 0);
    
    user_name = u2u.get_user(0);
    CPPUNIT_ASSERT_MESSAGE("translation 0 is not root", user_name.compare("root") == 0);
    
    user_name = u2u.get_user(65534);
    CPPUNIT_ASSERT_MESSAGE("translation 0 is not root", user_name.compare("nobody") == 0);
}
