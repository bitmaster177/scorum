#pragma once

#if 0
#include <scorum/snapshot/saver.hpp>
#include <scorum/snapshot/loader.hpp>

#define SCORUM_SNAPSHOT_PLUGIN(DB, SECTION)                                                                            \
    {                                                                                                                  \
        using scorum::snapshot::db_state;                                                                              \
        using scorum::snapshot::SECTION;                                                                               \
        DB.save_snapshot.connect([&](std::ofstream& fs) {                                                              \
            scorum::snapshot::save_index_section(fs, static_cast<db_state&>(DB), SECTION());                           \
        });                                                                                                            \
        DB.load_snapshot.connect([&](std::ifstream& fs, scorum::snapshot::index_ids_type& loaded_idxs) {               \
            scorum::snapshot::load_index_section(fs, static_cast<db_state&>(DB), loaded_idxs, SECTION());              \
        });                                                                                                            \
    }
#endif
