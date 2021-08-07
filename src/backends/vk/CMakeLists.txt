find_package(Vulkan)
if(Vulkan_FOUND)
    message(STATUS "Build with Vulkan backend: ${Vulkan_INCLUDE_DIRS}")
    set(LUISA_COMPUTE_VULKAN_SOURCES
        vk_device.cpp
    )
    luisa_compute_add_backend(vulkan SOURCES ${LUISA_COMPUTE_VULKAN_SOURCES})
    target_link_libraries(luisa-compute-backend-vulkan PRIVATE Vulkan::Headers Vulkan::Vulkan)
    target_compile_definitions(luisa-compute-backend-vulkan PUBLIC LUISA_BACKEND_VULKAN_ENABLED=1)
    set_target_properties(luisa-compute-backend-vulkan PROPERTIES UNITY_BUILD OFF)
endif()