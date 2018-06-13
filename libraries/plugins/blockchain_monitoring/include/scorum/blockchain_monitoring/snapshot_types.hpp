#pragma once

#include <scorum/snapshot/get_types_by_id.hpp>

#include <scorum/blockchain_monitoring/schema/bucket_object.hpp>

using namespace scorum::blockchain_monitoring;

// clang-format off
SCORUM_OBJECT_TYPES_FOR_SNAPSHOT_SECTION(blockchain_monitoring_section, blockchain_statistics_object_type,
                              (bucket)
                              )
// clang-format on
