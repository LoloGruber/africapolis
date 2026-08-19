add_executable(AfricapolisGraphConstruction GraphConstruction.cpp)
target_link_libraries(AfricapolisGraphConstruction PRIVATE Fishnet::Fishnet Fishnet::Workflow::Settlement CLI11::CLI11 magic_enum::magic_enum)
install(TARGETS AfricapolisGraphConstruction RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(AfricapolisGraphComponents GraphComponents.cpp)
target_link_libraries(AfricapolisGraphComponents PRIVATE Fishnet::Fishnet Fishnet::Workflow CLI11::CLI11 spdlog::spdlog)
install(TARGETS AfricapolisGraphComponents RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(AfricapolisSpatialClustering SpatialClustering.cpp)
target_link_libraries(AfricapolisSpatialClustering PRIVATE Fishnet::Fishnet Fishnet::Workflow::Settlement CLI11::CLI11 magic_enum::magic_enum)
install(TARGETS AfricapolisSpatialClustering RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(AfricapolisPolygonOutline PolygonOutline.cpp)
target_link_libraries(AfricapolisPolygonOutline PRIVATE Fishnet::Fishnet Fishnet::Workflow CLI11::CLI11)
install(TARGETS AfricapolisPolygonOutline RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(AfricapolisEdgeVisualization EdgeVisualization.cpp)
target_link_libraries(AfricapolisEdgeVisualization PRIVATE Fishnet::Fishnet Fishnet::Workflow CLI11::CLI11)
install(TARGETS AfricapolisEdgeVisualization RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(AfricapolisMSTVisualization MSTVisualization.cpp)
target_link_libraries(AfricapolisMSTVisualization PRIVATE Fishnet::Fishnet Fishnet::Workflow CLI11::CLI11)
install(TARGETS AfricapolisMSTVisualization RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)