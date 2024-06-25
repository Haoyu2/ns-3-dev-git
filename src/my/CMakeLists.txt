set(source_files
        model/my-queue-disc.cc
        model/my-point-to-point-dumbbell.cc
        model/my-red-queue-disc.cc

        helper/my-utils.cc
)

set(header_files
        model/my-queue-disc.h
        model/my-red-queue-disc.h
        model/my-point-to-point-dumbbell.h

        helper/my-utils.h
)


build_lib(
        LIBNAME my
        SOURCE_FILES ${source_files}
        HEADER_FILES ${header_files}
        LIBRARIES_TO_LINK
          ${libcore}
          ${libnetwork}
          ${libinternet}
          ${libpoint-to-point}
          ${libapplications}
          ${libtraffic-control}
          ${libmobility}
        TEST_SOURCES ""
)
