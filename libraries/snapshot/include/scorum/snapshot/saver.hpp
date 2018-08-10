#pragma once

#include <fstream>
#include <chainbase/db_state.hpp>

#include <scorum/snapshot/data_struct_hash.hpp>
#include <scorum/snapshot/get_types_by_id.hpp>

#ifdef DEBUG_SNAPSHOTTED_OBJECT
#include <fc/io/json.hpp>
#endif

#define DEBUG_SNAPSHOT_SAVE_CONTEXT std::string("save")

namespace scorum {
namespace snapshot {

using db_state = chainbase::db_state;

template <class IterationTag> class save_index_visitor
{
public:
    save_index_visitor(std::ofstream& fstream, db_state& state)
        : _fstream(fstream)
        , _state(state)
    {
    }

    template <class T> bool operator()(const T&) const
    {
        using object_type = typename T::type;

#ifdef DEBUG_SNAPSHOTTED_OBJECT
        std::string ctx(DEBUG_SNAPSHOT_SAVE_CONTEXT);
        auto object_name = boost::core::demangle(typeid(object_type).name());
        int debug_id = -1;
        if (object_name.rfind(BOOST_PP_STRINGIZE(DEBUG_SNAPSHOTTED_OBJECT)) != std::string::npos)
        {
            debug_id = (int)object_type::type_id;
        }
        snapshot_log(ctx, "saving index of ${name}:${id}", ("name", object_name)("id", (int)object_type::type_id));
#endif // DEBUG_SNAPSHOTTED_OBJECT

        const auto& index = _state.template get_index<typename chainbase::get_index_type<object_type>::type>()
                                .indices()
                                .template get<IterationTag>();
        size_t sz = index.size();

#ifdef DEBUG_SNAPSHOTTED_OBJECT
        if (debug_id == object_type::type_id)
        {
            snapshot_log(ctx, "debugging ${name}:${id}", ("name", object_name)("id", (int)object_type::type_id));
            snapshot_log(ctx, "index size=${sz}", ("sz", sz));
        }
#endif // DEBUG_SNAPSHOTTED_OBJECT

        fc::raw::pack(_fstream, sz);
        if (sz > 0)
        {
            auto itr = index.begin();

            fc::raw::pack(_fstream, get_data_struct_hash(*itr));

            for (; itr != index.end(); ++itr)
            {
                const object_type& obj = (*itr);
#ifdef DEBUG_SNAPSHOTTED_OBJECT
                if (debug_id == object_type::type_id)
                {
                    fc::variant vo;
                    fc::to_variant(obj, vo);
                    snapshot_log(ctx, "saved ${name}: ${obj}",
                                 ("name", object_name)("obj", fc::json::to_pretty_string(vo)));
                }
#endif // DEBUG_SNAPSHOTTED_OBJECT
                fc::raw::pack(_fstream, obj);
            }
        }

        return true;
    }

private:
    std::ofstream& _fstream;
    db_state& _state;
};
}
}
