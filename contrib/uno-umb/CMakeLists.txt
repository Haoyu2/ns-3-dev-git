set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/uno-umb-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME uno-umb
    SOURCE_FILES model/cell-sleep-controller.cc
    HEADER_FILES model/cell-sleep-controller.h
    LIBRARIES_TO_LINK ${libapplications}
                      ${libcore}
                      ${libflow-monitor}
                      ${libinternet}
                      ${liblte}
                      ${libmobility}
                      ${libnetwork}
                      ${libpoint-to-point}
    TEST_SOURCES test/uno-umb-test-suite.cc
                 ${examples_as_tests_sources}
)
