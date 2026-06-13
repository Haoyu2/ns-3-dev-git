build_lib_example(
    NAME aoi-twt-example
    SOURCE_FILES aoi-twt-example.cc
    LIBRARIES_TO_LINK ${libaoi-twt}
)

# Single-station AoI model validation against packet-level 802.11ax simulation.
build_lib_example(
    NAME twt-aoi-validation
    SOURCE_FILES twt-aoi-validation.cc
    LIBRARIES_TO_LINK ${libaoi-twt}
                      ${libwifi}
                      ${libinternet}
                      ${libapplications}
                      ${libmobility}
                      ${libnetwork}
                      ${libcore}
)

# Multi-station experiment: scheduler comparison (equal-interval, energy-greedy,
# harmonic-greedy) under heterogeneous weights, budgets, fading, and MCS.
build_lib_example(
    NAME twt-aoi-multista
    SOURCE_FILES twt-aoi-multista.cc
    LIBRARIES_TO_LINK ${libaoi-twt}
                      ${libwifi}
                      ${libinternet}
                      ${libapplications}
                      ${libmobility}
                      ${libnetwork}
                      ${libcore}
)
