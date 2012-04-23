#ifndef U2U_TEST_H
#define	U2U_TEST_H

#include <cppunit/extensions/HelperMacros.h>

class U2UTest : public CPPUNIT_NS::TestFixture {
    CPPUNIT_TEST_SUITE(U2UTest);
    CPPUNIT_TEST(testSimple);
    
    CPPUNIT_TEST_SUITE_END();

public:
    U2UTest();
    virtual ~U2UTest();
    void setUp();
    void tearDown();

private:
    void testSimple();
};

#endif	/* InitrdRepTEST_H */

