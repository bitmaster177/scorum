#pragma once

#include <scorum/snapshot/get_types_by_id.hpp>

#include <scorum/blockchain_history/schema/operation_objects.hpp>
#include <scorum/blockchain_history/schema/account_history_object.hpp>

using namespace scorum::blockchain_history;

// clang-format off
SCORUM_OBJECT_TYPES_FOR_SNAPSHOT_SECTION(blockchain_history_section, blockchain_history_object_type,
                                         (operations_history)
                                         (account_all_operations_history)
                                         (account_scr_to_scr_transfers_history)
                                         (account_scr_to_sp_transfers_history)
                                         (filtered_not_virt_operations_history)
                                         (filtered_virt_operations_history)
                                         (filtered_market_operations_history)
                              )
// clang-format on
