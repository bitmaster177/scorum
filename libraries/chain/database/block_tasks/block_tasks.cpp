#include <scorum/chain/database/block_tasks/block_tasks.hpp>

#include <scorum/chain/data_service_factory.hpp>
#include <scorum/protocol/block.hpp>

namespace scorum {
namespace chain {
namespace database_ns {

block_task_context::block_task_context(data_service_factory_i& services,
                                       database_virtual_operations_emmiter_i& vops,
                                       uint32_t block_num,
                                       block_info& block_info)
    : _services(services)
    , _vops(vops)
    , _block_num(block_num)
    , _block_info(block_info)
{
    FC_ASSERT(_block_num > 0u);
}

void block_task_context::push_virtual_operation(const operation& op)
{
    _vops.push_virtual_operation(op);
}

void block_task_context::notify_save_snapshot(std::ofstream& fs)
{
    _vops.notify_save_snapshot(fs);
}
void block_task_context::notify_load_snapshot(std::ifstream& fs, scorum::snapshot::index_ids_type& ids)
{
    _vops.notify_load_snapshot(fs, ids);
}

} // database_ns
}
}
