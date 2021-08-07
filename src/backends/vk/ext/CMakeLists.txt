set(SHADERC_SKIP_INSTALL ON CACHE BOOL "")
set(SHADERC_SKIP_TESTS ON CACHE BOOL "")

add_subdirectory(SPIRV-Headers)
add_subdirectory(glslang)
add_subdirectory(SPIRV-Tools)
add_subdirectory(shaderc)

add_library(luisa-compute-backend-vulkan-ext INTERFACE)
target_link_libraries(luisa-compute-backend-vulkan-ext INTERFACE shaderc)