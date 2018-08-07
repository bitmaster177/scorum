#include <boost/test/unit_test.hpp>

#include <scorum/chain/betting/betting_matcher.hpp>

#include "betting_common.hpp"

namespace betting_matcher_tests {

using namespace scorum::chain;
using namespace scorum::chain::betting;
using namespace scorum::protocol;
using namespace scorum::protocol::betting;

using namespace service_wrappers;

struct betting_matcher_fixture : public betting_common::betting_service_fixture_impl
{
    betting_matcher_fixture()
        : service(*dbs_services)
    {
    }

    betting_matcher service;

    template <typename... Args> asset total_stake(Args... args)
    {
        std::array<bet_service_i::object_cref_type, sizeof...(args)> list = { args... };
        asset result = ASSET_NULL_SCR;
        for (const bet_service_i::object_type& b : list)
        {
            result += b.stake;
        }
        return result;
    }

    template <typename... Args> asset total_stake_rest(Args... args)
    {
        std::array<bet_service_i::object_cref_type, sizeof...(args)> list = { args... };
        asset result = ASSET_NULL_SCR;
        for (const bet_service_i::object_type& b : list)
        {
            result += b.rest_stake;
        }
        return result;
    }

    template <typename... Args> asset total_gain(Args... args)
    {
        std::array<bet_service_i::object_cref_type, sizeof...(args)> list = { args... };
        asset result = ASSET_NULL_SCR;
        for (const bet_service_i::object_type& b : list)
        {
            result += b.gain;
        }
        return result;
    }

    template <typename... Args> asset total_matched(Args... args)
    {
        std::array<matched_bet_service_i::object_cref_type, sizeof...(args)> list = { args... };
        asset result = ASSET_NULL_SCR;
        for (const matched_bet_service_i::object_type& mb : list)
        {
            result += mb.matched_bet1_stake;
            result += mb.matched_bet2_stake;
        }
        return result;
    }

    template <typename... Args> void validate_invariants_if_any_wincase_worked(Args... args)
    {
        asset total_start = total_stake(args...);

        asset total_result = total_gain(args...);
        total_result += total_stake_rest(args...);

        BOOST_REQUIRE_EQUAL(total_result, total_start);
    }
};

BOOST_FIXTURE_TEST_SUITE(betting_matcher_tests, betting_matcher_fixture)

SCORUM_TEST_CASE(matching_not_found_and_created_pending_check)
{
    const auto& new_bet = create_bet();

    service.match(new_bet);

    BOOST_CHECK(pending_bet.is_exists());
    BOOST_CHECK(!matched_bet.is_exists());
}

SCORUM_TEST_CASE(matched_for_full_stake_check)
{
    const auto& bet1 = create_bet("alice", test_bet_game, goal_home_yes(), "10/1", ASSET_SCR(1e+9));

    service.match(bet1); // call this one bacause every bet creation followed by 'match' in evaluators

    const auto& bet2 = create_bet("bob", test_bet_game, goal_home_no(), "10/9", ASSET_SCR(9e+9));

    service.match(bet2);

    BOOST_CHECK(!pending_bet.is_exists());
    BOOST_CHECK(matched_bet.is_exists());

    BOOST_CHECK_EQUAL(matched_bet.get().bet1._id, bet2.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get().bet2._id, bet1.id._id);

    BOOST_CHECK_EQUAL(matched_bet.get().matched_bet1_stake, bet2.stake);
    BOOST_CHECK_EQUAL(matched_bet.get().matched_bet2_stake, bet1.stake);

    BOOST_CHECK_EQUAL(bet1.rest_stake, ASSET_NULL_SCR);
    BOOST_CHECK_EQUAL(bet2.rest_stake, ASSET_NULL_SCR);

    validate_invariants_if_any_wincase_worked(bet1, bet2);

    // validate_invariants_if_no_wincase_worked
    asset total_start = total_stake(bet1, bet2);
    asset total_result = total_matched(matched_bet.get(1));
    total_result += total_stake_rest(bet1, bet2);
    BOOST_REQUIRE_EQUAL(total_start, total_result);
}

SCORUM_TEST_CASE(matched_for_part_stake_check)
{
    const auto& bet1 = create_bet("alice", test_bet_game, goal_home_yes(), "10/1", ASSET_SCR(1e+9)); // 1 SCR

    service.match(bet1);

    // set not enough stake to pay gain 'alice'
    const auto& bet2 = create_bet("bob", test_bet_game, goal_home_no(), "10/9", ASSET_SCR(8e+9));

    service.match(bet2);

    BOOST_CHECK(pending_bet.is_exists());
    BOOST_CHECK(matched_bet.is_exists());

    BOOST_CHECK_EQUAL(matched_bet.get().bet1._id, bet2.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get().bet2._id, bet1.id._id);

    BOOST_CHECK_EQUAL(bet1.rest_stake, ASSET_SCR(111'111'112));
    // <- 0.(1) SCR period give accuracy lag!
    BOOST_CHECK_EQUAL(bet2.rest_stake, ASSET_NULL_SCR);

    BOOST_CHECK_EQUAL(matched_bet.get().matched_bet1_stake, bet2.stake);
    BOOST_CHECK_EQUAL(matched_bet.get().matched_bet2_stake, bet1.stake - bet1.rest_stake);

    BOOST_CHECK_EQUAL(pending_bet.get().bet._id, bet1.id._id);

    validate_invariants_if_any_wincase_worked(bet1, bet2);

    // validate_invariants_if_no_wincase_worked
    asset total_start = total_stake(bet1, bet2);
    asset total_result = total_matched(matched_bet.get(1));
    total_result += total_stake_rest(bet1, bet2);
    BOOST_REQUIRE_EQUAL(total_start, total_result);
}

SCORUM_TEST_CASE(matched_for_full_stake_with_more_than_one_matching_check)
{
    const auto& bet1 = create_bet("alice", test_bet_game, goal_home_yes(), "10/1", ASSET_SCR(1e+9)); // 1 SCR

    service.match(bet1);

    const auto& bet2 = create_bet("bob", test_bet_game, goal_home_no(), "10/9", ASSET_SCR(8e+9)); // 8 SCR

    service.match(bet2);

    BOOST_CHECK(pending_bet.is_exists());
    BOOST_CHECK(matched_bet.is_exists());

    BOOST_CHECK_EQUAL(matched_bet.get(1).bet1._id, bet2.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get(1).bet2._id, bet1.id._id);

    BOOST_CHECK_EQUAL(matched_bet.get(1).matched_bet1_stake, bet2.stake);
    BOOST_CHECK_EQUAL(matched_bet.get(1).matched_bet2_stake, bet1.stake - bet1.rest_stake);

    auto old_bet1_rest_stake = bet1.rest_stake;

    const auto& bet3 = create_bet("sam", test_bet_game, goal_home_no(), "10/9", ASSET_SCR(1e+9)); // 1 SCR

    service.match(bet3);
    // we have accuracy lag here (look match_found_for_part_stake_check) but
    //  bet2.stake + bet3.stake = bet1.gain
    // and bet1, bet2, bet3 should be matched

    BOOST_CHECK(!pending_bet.is_exists());
    BOOST_CHECK(matched_bet.is_exists());

    BOOST_CHECK_EQUAL(matched_bet.get(2).bet1._id, bet3.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get(2).bet2._id, bet1.id._id);

    BOOST_CHECK_EQUAL(matched_bet.get(2).matched_bet1_stake, bet3.stake);
    BOOST_CHECK_EQUAL(matched_bet.get(2).matched_bet2_stake, old_bet1_rest_stake - bet1.rest_stake);

    BOOST_CHECK_GT(bet1.gain, ASSET_NULL_SCR);
    BOOST_CHECK_GT(bet2.gain, ASSET_NULL_SCR);
    BOOST_CHECK_GT(bet3.gain, ASSET_NULL_SCR);

    validate_invariants_if_any_wincase_worked(bet1, bet2, bet3);

    // validate_invariants_if_no_wincase_worked
    asset total_start = total_stake(bet1, bet2, bet3);
    asset total_result = total_matched(matched_bet.get(1), matched_bet.get(2));
    total_result += total_stake_rest(bet1, bet2, bet3);
    BOOST_REQUIRE_EQUAL(total_start, total_result);
}

SCORUM_TEST_CASE(matched_from_larger_potential_result_check)
{
    const auto& bet1 = create_bet("bob", test_bet_game, goal_home_no(), "10/9", ASSET_SCR(8e+9)); // 8 SCR

    service.match(bet1);

    const auto& bet2 = create_bet("sam", test_bet_game, goal_home_no(), "10/9", ASSET_SCR(1e+9)); // 1 SCR

    service.match(bet2);

    const auto& bet3 = create_bet("alice", test_bet_game, goal_home_yes(), "10/1", ASSET_SCR(1e+9)); // 1 SCR

    service.match(bet3);

    BOOST_CHECK(!pending_bet.is_exists());
    BOOST_CHECK(matched_bet.is_exists());

    BOOST_CHECK_EQUAL(matched_bet.get(1).bet1._id, bet3.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get(1).bet2._id, bet1.id._id);

    BOOST_CHECK_EQUAL(matched_bet.get(1).matched_bet1_stake, bet1.potential_gain);
    BOOST_CHECK_EQUAL(matched_bet.get(1).matched_bet2_stake, bet1.stake - bet1.rest_stake);

    BOOST_CHECK_EQUAL(matched_bet.get(2).bet1._id, bet3.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get(2).bet2._id, bet2.id._id);

    BOOST_CHECK_EQUAL(matched_bet.get(2).matched_bet1_stake, bet2.potential_gain);
    BOOST_CHECK_EQUAL(matched_bet.get(2).matched_bet2_stake, bet3.stake);

    BOOST_CHECK_GT(bet1.gain, ASSET_NULL_SCR);
    BOOST_CHECK_GT(bet2.gain, ASSET_NULL_SCR);
    BOOST_CHECK_GT(bet3.gain, ASSET_NULL_SCR);

    validate_invariants_if_any_wincase_worked(bet1, bet2, bet3);

    // validate_invariants_if_no_wincase_worked
    asset total_start = total_stake(bet1, bet2, bet3);
    asset total_result = total_matched(matched_bet.get(1), matched_bet.get(2));
    total_result += total_stake_rest(bet1, bet2, bet3);
    BOOST_REQUIRE_EQUAL(total_start, total_result);
}

SCORUM_TEST_CASE(matched_but_matched_value_vanishes_from_larger_check)
{
    const auto& bet1 = create_bet("bob", test_bet_game, goal_home_no(), "10000/9999", ASSET_SCR(8e+3)); // 0.000'008 SCR
    // it should produce minimal matched stake = 1e-9 SCR

    service.match(bet1);

    const auto& bet2 = create_bet("alice", test_bet_game, goal_home_yes(), "10000/1", ASSET_SCR(1e+9)); // 1 SCR

    service.match(bet2);

    BOOST_CHECK(pending_bet.is_exists()); // 0.000'008 SCR can't be paid for huge 'alice' gain
    BOOST_CHECK(matched_bet.is_exists());

    BOOST_CHECK_EQUAL(matched_bet.get(1).bet1._id, bet2.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get(1).bet2._id, bet1.id._id);

    BOOST_CHECK_GT(bet1.gain, ASSET_NULL_SCR);
    BOOST_CHECK_GT(bet2.gain, ASSET_NULL_SCR);
    BOOST_CHECK_EQUAL(bet2.gain, bet1.stake);

    validate_invariants_if_any_wincase_worked(bet1, bet2);

    // validate_invariants_if_no_wincase_worked
    asset total_start = total_stake(bet1, bet2);
    asset total_result = total_matched(matched_bet.get(1));
    total_result += total_stake_rest(bet1, bet2);
    BOOST_REQUIRE_EQUAL(total_start, total_result);
}

SCORUM_TEST_CASE(matched_but_matched_value_vanishes_from_less_check)
{
    const auto& bet1 = create_bet("alice", test_bet_game, goal_home_yes(), "10000/1", ASSET_SCR(1e+9)); // 1 SCR

    service.match(bet1);

    const auto& bet2 = create_bet("bob", test_bet_game, goal_home_no(), "10000/9999", ASSET_SCR(8e+3)); // 0.000'008 SCR
    // it should produce minimal matched stake = 1e-9 SCR

    service.match(bet2);

    BOOST_CHECK(pending_bet.is_exists()); // 0.000'008 SCR can't be paid for huge 'alice' gain
    BOOST_CHECK(matched_bet.is_exists());

    BOOST_CHECK_EQUAL(matched_bet.get(1).bet1._id, bet2.id._id);
    BOOST_CHECK_EQUAL(matched_bet.get(1).bet2._id, bet1.id._id);

    BOOST_CHECK_GT(bet1.gain, ASSET_NULL_SCR);
    BOOST_CHECK_GT(bet2.gain, ASSET_NULL_SCR);
    BOOST_CHECK_EQUAL(bet1.gain, bet2.stake);

    validate_invariants_if_any_wincase_worked(bet1, bet2);

    // validate_invariants_if_no_wincase_worked
    asset total_start = total_stake(bet1, bet2);
    asset total_result = total_matched(matched_bet.get(1));
    total_result += total_stake_rest(bet1, bet2);
    BOOST_REQUIRE_EQUAL(total_start, total_result);
}

BOOST_AUTO_TEST_SUITE_END()
}
