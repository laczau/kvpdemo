include_directories(${CMAKE_SOURCE_DIR}/inc)
add_executable(server server.c keyregistry.c)
add_executable(client client.c keyregistry.c)