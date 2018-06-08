#pragma once

#include <fstream>
#include <chainbase/db_state.hpp>

#include <scorum/snapshot/data_struct_hash.hpp>
#include <scorum/snapshot/get_types_by_id.hpp>

#ifdef DEBUG_SNAPSHOTTED_OBJECT
#include <fc/io/json.hpp>
#endif

namespace scorum {
namespace snapshot {

using db_state = chainbase::db_state;

template <class IterationTag, class Section> class save_index_visitor
{
public:
    using result_type = void;

    save_index_visitor(std::ofstream& fstream, db_state& state)
        : _fstream(fstream)
        , _state(state)
    {
    }

    template <class T> void operator()(const T&) const
    {
        using object_type = typename T::type;

#ifdef DEBUG_SNAPSHOTTED_OBJECT
        std::string ctx("save");
        auto object_name = boost::core::demangle(typeid(object_type).name());
        int debug_id = -1;
        if (object_name.rfind(BOOST_PP_STRINGIZE(DEBUG_SNAPSHOTTED_OBJECT)) != std::string::npos)
        {
            debug_id = (int)object_type::type_id;
        }
        snapshot_log(ctx, "saving ${name}:${id}", ("name", object_name)("id", object_type::type_id));
#endif // DEBUG_SNAPSHOTTED_OBJECT

        const auto& index = _state.template get_index<typename chainbase::get_index_type<object_type>::type>()
                                .indices()
                                .template get<IterationTag>();
        size_t sz = index.size();

#ifdef DEBUG_SNAPSHOTTED_OBJECT
        if (debug_id == object_type::type_id)
        {
            snapshot_log(ctx, "index size=${sz}", ("sz", sz));
            snapshot_log(ctx, "debugging ${name}:${id}", ("name", object_name)("id", object_type::type_id));
        }
#endif // DEBUG_SNAPSHOTTED_OBJECT

        fc::raw::pack(_fstream, sz);
        auto itr = index.begin();
        if (sz > 0)
        {
            fc::raw::pack(_fstream, get_data_struct_hash(*itr));

            for (auto itr = index.begin(); itr != index.end(); ++itr)
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
    }

private:
    std::ofstream& _fstream;
    db_state& _state;
};

template <class Section>
void save_index_section(std::ofstream& fstream, chainbase::db_state& state, const Section& section)
{
    fc::ripemd160::encoder check_enc;
    fc::raw::pack(check_enc, section.name);
    fc::raw::pack(fstream, check_enc.result());

    state.for_each_index_key([&](uint16_t index_id) {
        bool initialized = false;
        auto v = section.get_object_type_variant(index_id, initialized);
        // checking because static variant interpret uninitialized state like first type
        if (initialized)
            v.visit(save_index_visitor<by_id, Section>(fstream, state));
    });
}
}
}
