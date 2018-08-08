set (TAGS_HPP "include/scorum/tags/tags_plugin.hpp"
              "include/scorum/tags/tags_api.hpp"
              "include/scorum/tags/tags_api_impl.hpp"
              "include/scorum/tags/tags_api_objects.hpp"
              "include/scorum/tags/tags_objects.hpp"
              "include/scorum/tags/tags_service.hpp"
              "include/scorum/tags/snapshot_types.hpp")

add_library(scorum_tags
            tags_plugin.cpp
            tags_api.cpp
            tags_service.cpp
            tags_api_objects.cpp
            ${TAGS_HPP})

target_link_libraries(scorum_tags
                      scorum_chain
                      scorum_protocol
                      scorum_app
                      scorum_utils)

target_include_directories(scorum_tags
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../snapshot/include")

add_custom_target(scorum_tags_manifest SOURCES plugin.json)

install(TARGETS
   scorum_tags

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
