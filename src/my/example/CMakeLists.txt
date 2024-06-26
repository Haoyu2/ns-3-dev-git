set(source_files
        my-dumbbell-dctcp-main.cc
        my-config.cc
        my-config-queue.cc
)

build_lib_example(
          NAME my-main-example
          SOURCE_FILES ${source_files}
          LIBRARIES_TO_LINK
            ${libmy}
            ${libcore}
            ${libnetwork}
            ${libinternet}
            ${libpoint-to-point}
            ${libapplications}
            ${libtraffic-control}
            ${libconfig-store}
  )

build_lib_example(
        NAME my-scratch-example
        SOURCE_FILES my-scratch.cc
        LIBRARIES_TO_LINK
        ${libmy}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libpoint-to-point}
        ${libapplications}
        ${libtraffic-control}
        ${libconfig-store}
)
