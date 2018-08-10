#include <scorum/chain/database/scheduled_snapshot.hpp>

#include <scorum/chain/database/database.hpp>
#include <chainbase/db_state.hpp>

#include <scorum/chain/services/snapshot.hpp>
#include <scorum/chain/services/blocks_story.hpp>
#include <scorum/chain/services/account_registration_bonus.hpp>

#include <string>
#include <sstream>
#include <fstream>

#include <fc/io/raw.hpp>
#include <fc/string.hpp>

#include <scorum/snapshot/config.hpp>
#include <scorum/snapshot/loader.hpp>
#include <scorum/snapshot/saver.hpp>

#include <scorum/chain/database/snapshot_finder_for_types.hpp>

namespace scorum {

static find_object_type object_types;

namespace chain {
namespace database_ns {

using db_state = chainbase::db_state;

class scheduled_snapshot_impl
{
public:
    scheduled_snapshot_impl(database& db, db_state& state)
        : dprops_service(db.dynamic_global_property_service())
        , blocks_story_service(db.blocks_story_service())
        , _state(state)
    {
    }

    void save_index_section(std::ofstream& fstream, chainbase::db_state& state)
    {
#ifdef DEBUG_SNAPSHOTTED_OBJECT
        snapshot_log(DEBUG_SNAPSHOT_SAVE_CONTEXT, "saving index");
#endif // DEBUG_SNAPSHOTTED_OBJECT

        state.for_each_index_key([&](uint16_t index_id) {
            FC_ASSERT(object_types.apply(scorum::snapshot::save_index_visitor<by_id>(fstream, state), index_id),
                      "Can't save index");
        });
    }

    void save(const fc::path& snapshot_path, database_virtual_operations_emmiter_i& vops)
    {
        fc::remove_all(snapshot_path);

        std::ofstream snapshot_stream;
        snapshot_stream.open(snapshot_path.generic_string(), std::ios::binary);

        optional<signed_block> b = blocks_story_service.fetch_block_by_id(dprops_service.get().head_block_id);

        FC_ASSERT(b.valid(), "Invalid block header");

        snapshot_header header;
        header.version = SCORUM_SNAPSHOT_SERIALIZER_VER;
        header.head_block_number = b->block_num();
        header.head_block_digest = b->digest();
        header.chainbase_flags = chainbase::database::read_write;
        fc::raw::pack(snapshot_stream, header);

        save_index_section(snapshot_stream, _state);

        vops.notify_save_snapshot(snapshot_stream);
        snapshot_stream.close();
    }

    snapshot_header load_header(const fc::path& snapshot_path)
    {
        std::ifstream snapshot_stream;
        snapshot_stream.open(snapshot_path.generic_string(), std::ios::binary);

        snapshot_header header = load_header(snapshot_stream);
        FC_ASSERT(header.version == SCORUM_SNAPSHOT_SERIALIZER_VER);

        snapshot_stream.close();

        return header;
    }

    snapshot_header load_header(std::ifstream& fs)
    {
        snapshot_header header;
        fc::raw::unpack(fs, header);

        return header;
    }

    void load_index_section(std::ifstream& fstream,
                            chainbase::db_state& state,
                            scorum::snapshot::index_ids_type& loaded_idxs)
    {
#ifdef DEBUG_SNAPSHOTTED_OBJECT
        snapshot_log(DEBUG_SNAPSHOT_LOAD_CONTEXT, "loading index");
#endif // DEBUG_SNAPSHOTTED_OBJECT

        state.for_each_index_key([&](uint16_t index_id) {
            if (object_types.apply(scorum::snapshot::load_index_visitor<by_id>(fstream, state), index_id))
            {
                loaded_idxs.insert(index_id);
            }
        });
    }

    void load(const fc::path& snapshot_path, database_virtual_operations_emmiter_i& vops)
    {
        std::ifstream snapshot_stream;
        snapshot_stream.open(snapshot_path.generic_string(), std::ios::binary);
        FC_ASSERT(snapshot_stream, "Can't open ${p}", ("p", snapshot_path.generic_string()));

        uint64_t sz = fc::file_size(snapshot_path);

        snapshot_header header = load_header(snapshot_stream);

        ilog("Loading snapshot for block ${n} from file ${f}.",
             ("n", header.head_block_number)("f", snapshot_path.generic_string()));

        scorum::snapshot::index_ids_type loaded_idxs;

        loaded_idxs.reserve(_state.get_indexes_size());

        load_index_section(snapshot_stream, _state, loaded_idxs);

        vops.notify_load_snapshot(snapshot_stream, loaded_idxs);

#ifdef DEBUG_SNAPSHOTTED_OBJECT
        snapshot_log(DEBUG_SNAPSHOT_LOAD_CONTEXT, "state index size=${sz}, loaded=${lsz}",
                     ("sz", _state.get_indexes_size())("lsz", loaded_idxs.size()));
#endif // DEBUG_SNAPSHOTTED_OBJECT

        if ((uint64_t)snapshot_stream.tellg() != sz)
        {
            wlog("Not all indexes from snapshot are loaded. Node configuration does not match snapshot.");
        }

        snapshot_stream.close();
    }

private:
    dynamic_global_property_service_i& dprops_service;
    blocks_story_service_i& blocks_story_service;

    db_state& _state;
};

save_scheduled_snapshot::save_scheduled_snapshot(database& db)
    : _impl(new scheduled_snapshot_impl(db, static_cast<db_state&>(db)))
{
}
save_scheduled_snapshot::~save_scheduled_snapshot()
{
}

fc::path save_scheduled_snapshot::get_snapshot_path(dynamic_global_property_service_i& dprops_service,
                                                    const fc::path& snapshot_dir)
{
    std::stringstream snapshot_name;
    snapshot_name << dprops_service.get().time.to_iso_string();
    snapshot_name << "-";
    snapshot_name << dprops_service.get().head_block_number;
    snapshot_name << ".bin";

    return snapshot_dir / snapshot_name.str();
}

void save_scheduled_snapshot::on_apply(block_task_context& ctx)
{
    snapshot_service_i& snapshot_service = ctx.services().snapshot_service();
    dynamic_global_property_service_i& dprops_service = ctx.services().dynamic_global_property_service();

    if (!snapshot_service.is_snapshot_scheduled())
    {
        if (!check_snapshot_task(ctx))
            return;
    }

    auto number = snapshot_service.get_snapshot_scheduled_number();
    if (number && number != ctx.block_num())
        return;

    snapshot_service.clear_snapshot_schedule();

    fc::path snapshot_path = get_snapshot_path(dprops_service, snapshot_service.get_snapshot_dir());
    try
    {
        ilog("Making snapshot for block ${n} to file ${f}.",
             ("n", ctx.block_num())("f", snapshot_path.generic_string()));
        _impl->save(snapshot_path, static_cast<database_virtual_operations_emmiter_i&>(ctx));
        return;
    }
    FC_CAPTURE_AND_LOG((ctx.block_num())(snapshot_path))
    fc::remove_all(snapshot_path);
}

bool save_scheduled_snapshot::check_snapshot_task(block_task_context& ctx)
{
    snapshot_service_i& snapshot_service = ctx.services().snapshot_service();

    fc::path task_file_path = snapshot_service.get_snapshot_dir() / fc::to_string(ctx.block_num());
    if (fc::exists(task_file_path))
    {
        snapshot_service.schedule_snapshot_task(ctx.block_num());
    }

    return snapshot_service.is_snapshot_scheduled();
}

load_scheduled_snapshot::load_scheduled_snapshot(database& db)
    : _impl(new scheduled_snapshot_impl(db, static_cast<db_state&>(db)))
    , _db(db)
{
}
load_scheduled_snapshot::~load_scheduled_snapshot()
{
}

snapshot_header load_scheduled_snapshot::load_header(const fc::path& snapshot_path)
{
    try
    {
        return _impl->load_header(snapshot_path);
    }
    FC_CAPTURE_AND_RETHROW((snapshot_path))
}

void load_scheduled_snapshot::load(const fc::path& snapshot_path)
{
    try
    {
        _impl->load(snapshot_path, static_cast<database_virtual_operations_emmiter_i&>(_db));
        _db.validate_invariants();
    }
    FC_CAPTURE_AND_RETHROW((snapshot_path))
}
}
}
}
