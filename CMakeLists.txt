project(demos)

set(SCYTHE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../scythe")
set(BINARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_subdirectory(atmospheric_scattering)
add_subdirectory(cascaded_shadows)
add_subdirectory(marble_maze)
add_subdirectory(pbr)
add_subdirectory(ray_trace)
add_subdirectory(sandbox)
add_subdirectory(shadows)