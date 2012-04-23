
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>

#include <string.h>

#include <iostream>
#include <ModuleLogger.h>
#include <iostream>

using namespace std;

int main(int argc, char* argv[]) {
    mlog_init();
    mlog_registerColorFormatter((color_formatter_func_t)sprintf_color);
    mlog_addOutputFile((char *)"/dev/tty", 1);
       
    char *testName = NULL;
    if (argc > 1) {
        std::cerr << "Turning on debugging" << std::endl;
        mlog_setModuleLevel((char *)"u2u", LOG_DEBUG);
    }

    // Create the event manager and test controller
    CPPUNIT_NS::TestResult controller;

    // Add a listener that colllects test result
    CPPUNIT_NS::TestResultCollector result;
    controller.addListener(&result);

    // Add a listener that print dots as test run.
    CPPUNIT_NS::BriefTestProgressListener progress;
    controller.addListener(&progress);

    // Add the top suite to the test runner
    CPPUNIT_NS::TestRunner runner;
    CPPUNIT_NS::Test* test_to_run = CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest();
    if (testName)
        test_to_run = test_to_run->findTest(testName);

    runner.addTest(test_to_run);
    runner.run(controller);

    // Print test in a compiler compatible format.
    CPPUNIT_NS::CompilerOutputter outputter(&result, CPPUNIT_NS::stdCOut());
    outputter.write();
    return result.wasSuccessful() ? 0 : 1;
}
