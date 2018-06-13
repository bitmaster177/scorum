#pragma once

#include <scorum/snapshot/get_types_by_id.hpp>

#include <scorum/account_statistics/schema/objects.hpp>

using namespace scorum::account_statistics;

// clang-format off
SCORUM_OBJECT_TYPES_FOR_SNAPSHOT_SECTION(account_statistic_section, account_statistics_plugin_object_types,
                              (bucket)
                              )
// clang-format on
