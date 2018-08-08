#include <boost/test/unit_test.hpp>

#include "database_trx_integration.hpp"
#include "database_blog_integration.hpp"

#include <scorum/chain/database/scheduled_snapshot.hpp>
#include <scorum/chain/services/dynamic_global_property.hpp>
#include <scorum/chain/services/snapshot.hpp>
#include <scorum/chain/services/comment.hpp>

#include <graphene/utilities/tempdir.hpp>
#include <fc/filesystem.hpp>

#include <scorum/protocol/block.hpp>

namespace snapshot_tests {

using namespace scorum;
using namespace scorum::chain;
using namespace scorum::protocol;

struct snapshot_fixture : public database_fixture::database_trx_integration_fixture
{
    snapshot_fixture()
        : dprops_service(db.dynamic_global_property_service())
        , snapshot_service(db.snapshot_service())
        , alice("alice")
        , sam("sam")
    {
        open_database();

        const int feed_amount = 99000;

        actor(initdelegate).create_account(alice);
        actor(initdelegate).give_scr(alice, feed_amount);
        actor(initdelegate).give_sp(alice, feed_amount);

        actor(initdelegate).create_account(sam);
        actor(initdelegate).give_scr(sam, feed_amount / 2);
        actor(initdelegate).give_sp(sam, feed_amount / 2);

        db.save_snapshot.connect([&](std::ofstream& fs) { save_plugin_snapshot(fs); });

        generate_blocks(2);
    }

    void save_plugin_snapshot(std::ofstream&)
    {
        snapshot_saved_for_plugin = true;
    }

    bool snapshot_saved_for_plugin = false;
    dynamic_global_property_service_i& dprops_service;
    snapshot_service_i& snapshot_service;

    Actor alice;
    Actor sam;
};

BOOST_FIXTURE_TEST_SUITE(snapshot_tests, snapshot_fixture)

BOOST_AUTO_TEST_CASE(dont_make_snapshot_with_no_snapshot_dir)
{
    db.schedule_snapshot_task();

    BOOST_CHECK_EQUAL(snapshot_service.is_snapshot_scheduled(), false);

    generate_block();

    BOOST_CHECK_EQUAL(snapshot_saved_for_plugin, false);
}

BOOST_AUTO_TEST_CASE(save_and_load_snapshot_for_genesis_state)
{
    using namespace scorum::chain::database_ns;

    fc::path dir = fc::temp_directory(graphene::utilities::temp_directory_path()).path();
    db.set_snapshot_dir(dir);
    db.schedule_snapshot_task();

    BOOST_CHECK_EQUAL(snapshot_service.is_snapshot_scheduled(), true);

    fc::path snapshot_file
        = save_scheduled_snapshot::get_snapshot_path(dprops_service, snapshot_service.get_snapshot_dir());

    fc::remove_all(snapshot_file);

    save_scheduled_snapshot saver(db);

    block_info empty_info;
    block_task_context ctx(static_cast<data_service_factory&>(db),
                           static_cast<database_virtual_operations_emmiter_i&>(db), db.head_block_num(), empty_info);

    BOOST_REQUIRE_NO_THROW(saver.apply(ctx));

    BOOST_CHECK_EQUAL(snapshot_saved_for_plugin, true);

    BOOST_CHECK_EQUAL(snapshot_service.is_snapshot_scheduled(), false);

    BOOST_REQUIRE(fc::exists(snapshot_file));

    load_scheduled_snapshot loader(db);

    snapshot_header header = loader.load_header(snapshot_file);

    BOOST_REQUIRE_EQUAL(header.head_block_number, dprops_service.get().head_block_number);
    BOOST_REQUIRE_EQUAL(header.chainbase_flags, chainbase::database::read_write);

    BOOST_REQUIRE_NO_THROW(loader.load(snapshot_file));
}

BOOST_AUTO_TEST_SUITE_END()

struct snapshot_reopen_fixture_impl : public database_fixture::database_blog_integration_fixture
{
    snapshot_reopen_fixture_impl()
        : alice("alice")
        , sam("sam")
        , comment_service(db.comment_service())
        , dprops_service(db.dynamic_global_property_service())
        , snapshot_service(db.snapshot_service())
    {
        open_database();

        const int feed_amount = 99000;

        actor(initdelegate).create_account(alice);
        actor(initdelegate).give_scr(alice, feed_amount);
        actor(initdelegate).give_sp(alice, feed_amount);

        actor(initdelegate).create_account(sam);
        actor(initdelegate).give_scr(sam, feed_amount / 2);
        actor(initdelegate).give_sp(sam, feed_amount / 2);

        generate_blocks(2);
    }

    Actor alice;
    Actor sam;

    comment_service_i& comment_service;
    dynamic_global_property_service_i& dprops_service;
    snapshot_service_i& snapshot_service;
};

struct snapshot_reopen_fixture
{
    snapshot_reopen_fixture()
    {
        reset();
    }

    void reset()
    {
        fixture.reset(new snapshot_reopen_fixture_impl());
    }

    std::unique_ptr<snapshot_reopen_fixture_impl> fixture;
};

BOOST_FIXTURE_TEST_SUITE(snapshot_reopen_tests, snapshot_reopen_fixture)

BOOST_AUTO_TEST_CASE(save_and_load_snapshot_with_removing_items)
{
    using namespace scorum::chain::database_ns;

    auto alice_post = fixture->create_post(fixture->alice).push();

    fixture->generate_block();

    auto sam_post = fixture->create_post(fixture->sam).push();

    sam_post.create_comment(fixture->alice);

    alice_post.remove();

    fixture->generate_block();

    fc::path dir = fc::temp_directory(graphene::utilities::temp_directory_path()).path();
    fixture->db.set_snapshot_dir(dir);
    fixture->db.schedule_snapshot_task();

    fc::path snapshot_file = save_scheduled_snapshot::get_snapshot_path(fixture->dprops_service,
                                                                        fixture->snapshot_service.get_snapshot_dir());

    fc::remove_all(snapshot_file);

    save_scheduled_snapshot saver(fixture->db);

    block_info empty_info;
    block_task_context ctx(static_cast<data_service_factory&>(fixture->db),
                           static_cast<database_virtual_operations_emmiter_i&>(fixture->db),
                           fixture->db.head_block_num(), empty_info);

    BOOST_REQUIRE_NO_THROW(saver.apply(ctx));

    reset();

    alice_post = fixture->create_post(fixture->alice).push();

    fixture->generate_block();

    load_scheduled_snapshot loader(fixture->db);

    BOOST_REQUIRE_NO_THROW(loader.load(snapshot_file));

    // alice post should be deleted
    BOOST_CHECK(!fixture->comment_service.is_exists(alice_post.author(), alice_post.permlink()));
    BOOST_CHECK(fixture->comment_service.is_exists(sam_post.author(), sam_post.permlink()));
}

BOOST_AUTO_TEST_SUITE_END()
}
