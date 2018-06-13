#pragma once

#include <scorum/snapshot/get_types_by_id.hpp>

#include <scorum/tags/tags_objects.hpp>

using namespace scorum::tags;

// clang-format off
SCORUM_OBJECT_TYPES_FOR_SNAPSHOT_SECTION(tags_section, tags_object_types,
                                         (tag)
                                         (tag_stats)
                                         (peer_stats)
                                         (author_tag_stats)
                                         (category_stats)
                              )
// clang-format on
