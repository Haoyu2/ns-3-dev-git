set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/aoi-twt-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME aoi-twt
    SOURCE_FILES model/twt-schedule.cc
                 model/twt-scheduler.cc
    HEADER_FILES model/twt-schedule.h
                 model/twt-scheduler.h
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES test/aoi-twt-test-suite.cc
                 ${examples_as_tests_sources}
)
