#include <boost/test/unit_test.hpp>

#include <scorum/chain/services/account.hpp>
#include <scorum/chain/services/reward_balancer.hpp>
#include <scorum/chain/services/budgets.hpp>
#include <scorum/chain/services/dev_pool.hpp>
#include <scorum/chain/services/reward_funds.hpp>
#include <scorum/chain/services/dynamic_global_property.hpp>
#include <scorum/protocol/config.hpp>

#include <scorum/chain/schema/account_objects.hpp>
#include <scorum/chain/schema/reward_balancer_objects.hpp>
#include <scorum/chain/schema/budget_objects.hpp>
#include <scorum/chain/schema/dev_committee_object.hpp>
#include <scorum/chain/schema/scorum_objects.hpp>
#include <scorum/chain/schema/dynamic_global_property_object.hpp>

#include <scorum/chain/database/budget_management_algorithms.hpp>

#include "database_default_integration.hpp"

#include <limits>

using namespace database_fixture;

namespace reward_distribution {

class dbs_reward_fixture : public database_integration_fixture
{
public:
    dbs_reward_fixture()
        : content_reward_scr_service(db.content_reward_scr_service())
        , fund_budget_service(db.fund_budget_service())
        , advertising_budget_service(db.post_budget_service())
        , dev_service(db.dev_pool_service())
        , content_reward_fund_scr_service(db.content_reward_fund_scr_service())
        , content_reward_fund_sp_service(db.content_reward_fund_sp_service())
        , account_service(db.account_service())
        , dgp_service(db.dynamic_global_property_service())
        , voters_reward_sp_service(db.voters_reward_sp_service())
        , voters_reward_scr_service(db.voters_reward_scr_service())
    {
        open_database();
    }

    content_reward_scr_service_i& content_reward_scr_service;
    fund_budget_service_i& fund_budget_service;
    post_budget_service_i& advertising_budget_service;
    dev_pool_service_i& dev_service;
    content_reward_fund_scr_service_i& content_reward_fund_scr_service;
    content_reward_fund_sp_service_i& content_reward_fund_sp_service;
    account_service_i& account_service;
    dynamic_global_property_service_i& dgp_service;
    voters_reward_sp_service_i& voters_reward_sp_service;
    voters_reward_scr_service_i& voters_reward_scr_service;

    const asset NULL_BALANCE = asset(0, SCORUM_SYMBOL);
};
}

BOOST_FIXTURE_TEST_SUITE(reward_distribution, reward_distribution::dbs_reward_fixture)

SCORUM_TEST_CASE(check_per_block_reward_distribution_with_fund_budget_only)
{
    generate_block();

    const auto& fund_budget = fund_budget_service.get();
    asset initial_per_block_reward = fund_budget.per_block;

    auto witness_reward = initial_per_block_reward * SCORUM_WITNESS_PER_BLOCK_REWARD_PERCENT / SCORUM_100_PERCENT;
    auto active_voters_reward
        = initial_per_block_reward * SCORUM_ACTIVE_SP_HOLDERS_PER_BLOCK_REWARD_PERCENT / SCORUM_100_PERCENT;
    auto content_reward = initial_per_block_reward - witness_reward - active_voters_reward;

    const auto& account = account_service.get_account(TEST_INIT_DELEGATE_NAME);

    BOOST_REQUIRE_EQUAL(dev_service.get().scr_balance, NULL_BALANCE);
    BOOST_REQUIRE_EQUAL(content_reward_fund_sp_service.get().activity_reward_balance, content_reward);
    BOOST_REQUIRE_EQUAL(account.scorumpower, witness_reward);
    BOOST_REQUIRE_EQUAL(voters_reward_sp_service.get().balance, active_voters_reward);
}

SCORUM_TEST_CASE(check_per_block_reward_distribution_with_advertising_budget)
{
    //          | r[i] + max(1, r[i]*5/100) if B > r[i]*D(100),
    // r[i+1] = | max(1, r[i] - r[i]*5/100) if B < r[i]*D(30),                          (1)
    //          | r[i]                      if B in [r[i]*D(30), r[i]*D(100)].
    //
    // TODO: there are not tests for this formula.

    const auto& account = account_service.get_account(TEST_INIT_DELEGATE_NAME);
    auto advertising_budget = ASSET_SCR(2e+9);
    auto deadline = db.get_slot_time(1);
    content_reward_scr_service.update(
        [&](content_reward_balancer_scr_object& b) { b.current_per_block_reward = advertising_budget; });

    BOOST_CHECK_NO_THROW(post_budget_management_algorithm(advertising_budget_service, dgp_service, account_service)
                             .create_budget(account.name, advertising_budget, db.head_block_time(), deadline, ""));

    asset balance_before = account.balance;

    generate_block();

    auto dev_team_reward_scr = advertising_budget * SCORUM_DEV_TEAM_PER_BLOCK_REWARD_PERCENT / SCORUM_100_PERCENT;
    auto user_reward_scr = advertising_budget - dev_team_reward_scr;

    auto witness_reward_scr = user_reward_scr * SCORUM_WITNESS_PER_BLOCK_REWARD_PERCENT / SCORUM_100_PERCENT;
    auto active_voters_reward_scr
        = user_reward_scr * SCORUM_ACTIVE_SP_HOLDERS_PER_BLOCK_REWARD_PERCENT / SCORUM_100_PERCENT;
    auto content_reward_scr = user_reward_scr - witness_reward_scr - active_voters_reward_scr;

    BOOST_REQUIRE_EQUAL(dev_service.get().scr_balance, dev_team_reward_scr);
    BOOST_REQUIRE_EQUAL(account.balance, balance_before + witness_reward_scr);
    BOOST_REQUIRE_EQUAL(voters_reward_scr_service.get().balance, active_voters_reward_scr);
    BOOST_REQUIRE_EQUAL(content_reward_fund_scr_service.get().activity_reward_balance, content_reward_scr);
}

BOOST_AUTO_TEST_SUITE_END()
