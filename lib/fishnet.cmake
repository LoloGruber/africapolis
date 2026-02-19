include(FetchContent)
set(FISHNET_TEST OFF)
set(FISHNET_APPS ON)
set(FISHNET_EXAMPLES OFF)
set(FISHNET_TEST_LIB OFF)
FetchContent_Declare(fishnet
    GIT_REPOSITORY https://gitlab2.informatik.uni-wuerzburg.de/descartes/sos/fishnet
    GIT_TAG main
)
FetchContent_MakeAvailable(fishnet)
