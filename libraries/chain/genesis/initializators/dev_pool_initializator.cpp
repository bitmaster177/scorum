#include <scorum/chain/genesis/initializators/dev_pool_initializator.hpp>
#include <scorum/chain/data_service_factory.hpp>

#include <fc/exception/exception.hpp>

#include <scorum/chain/services/account.hpp>
#include <scorum/chain/services/dynamic_global_property.hpp>
#include <scorum/chain/services/dev_pool.hpp>

#include <scorum/chain/schema/account_objects.hpp>
#include <scorum/chain/schema/dynamic_global_property_object.hpp>
#include <scorum/chain/schema/dev_committee_object.hpp>

#include <scorum/chain/genesis/genesis_state.hpp>

#include <limits>

namespace scorum {
namespace chain {
namespace genesis {

void dev_pool_initializator_impl::on_apply(initializator_context& ctx)
{
    if (!is_dev_pool_exists(ctx))
    {
        dlog("There is no development supply.");
        return;
    }

    asset dev_supply = ctx.genesis_state.dev_supply;

    FC_ASSERT(dev_supply.symbol() == VESTS_SYMBOL);

    increase_total_supply(ctx, dev_supply);
    create_locked_account(ctx, dev_supply);
    create_dev_pool(ctx);

    // TODO
}

bool dev_pool_initializator_impl::is_dev_pool_exists(initializator_context& ctx)
{
    return ctx.genesis_state.dev_supply.amount.value;
}

void dev_pool_initializator_impl::increase_total_supply(initializator_context& ctx, const asset& sp)
{
    dynamic_global_property_service_i& dgp_service = ctx.services.dynamic_global_property_service();

    dgp_service.update(
        [&](dynamic_global_property_object& props) { props.total_supply += asset(sp.amount, SCORUM_SYMBOL); });
}

void dev_pool_initializator_impl::create_locked_account(initializator_context& ctx, const asset& sp)
{
    account_service_i& account_service = ctx.services.account_service();
    dynamic_global_property_service_i& dgp_service = ctx.services.dynamic_global_property_service();

    const account_object& account = account_service.create_initial_account(
        SCORUM_DEV_POOL_SP_LOCKED_ACCOUNT, public_key_type(), asset(), "",
        R"({"created_at": "GENESIS", "for": "DEVELOPMENT POOL", "locked": true})");
    account_service.increase_vesting_shares(account, sp);
    dgp_service.update([&](dynamic_global_property_object& props) { props.total_vesting_shares += sp; });
}

void dev_pool_initializator_impl::create_dev_pool(initializator_context& ctx)
{
    dev_pool_service_i& dev_pool_service = ctx.services.dev_pool_service();

    FC_ASSERT(!dev_pool_service.is_exists());

    dev_pool_service.create(asset(0, SCORUM_SYMBOL));
}
}
}
}
