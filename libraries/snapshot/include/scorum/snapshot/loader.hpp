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

template <class IterationTag, class Section> class load_index_visitor
{
public:
    using result_type = void;

    load_index_visitor(std::ifstream& fstream, db_state& state)
        : _fstream(fstream)
        , _state(state)
    {
    }

    template <class T> void operator()(const T&) const
    {
        using object_type = typename T::type;

#ifdef DEBUG_SNAPSHOTTED_OBJECT
        std::string ctx("load");
        auto object_name = boost::core::demangle(typeid(object_type).name());
        int debug_id = -1;
        if (object_name.rfind(BOOST_PP_STRINGIZE(DEBUG_SNAPSHOTTED_OBJECT)) != std::string::npos)
        {
            debug_id = (int)object_type::type_id;
        }
        snapshot_log(ctx, "loading ${name}:${id}", ("name", object_name)("id", object_type::type_id));
#endif // DEBUG_SNAPSHOTTED_OBJECT

        size_t sz = 0;
        fc::raw::unpack(_fstream, sz);

        const auto& index = _state.template get_index<typename chainbase::get_index_type<object_type>::type>()
                                .indices()
                                .template get<IterationTag>();

        object_type tmp([](object_type&) {}, index.get_allocator());

        if (sz > 0)
        {
            fc::ripemd160 check, etalon;

            fc::raw::unpack(_fstream, check);

            FC_ASSERT(check == get_data_struct_hash(tmp));
        }

        using object_id_type = typename object_type::id_type;
        using objects_copy_type = fc::shared_map<object_id_type, object_type>;
        using object_ref_type = std::reference_wrapper<const object_type>;
        using objects_type = std::vector<object_ref_type>;

        objects_copy_type objs_to_modify(index.get_allocator());

#ifdef DEBUG_SNAPSHOTTED_OBJECT
        if (debug_id == object_type::type_id)
        {
            snapshot_log(ctx, "index size=${sz}", ("sz", index.size()));
            snapshot_log(ctx, "debugging ${name}:${id}", ("name", object_name)("id", object_type::type_id));
        }
#endif // DEBUG_SNAPSHOTTED_OBJECT

        if (index.size() > 0)
        {
            objects_type objs_to_remove;
            objs_to_remove.reserve(index.size());

            for (auto itr = index.begin(); itr != index.end(); ++itr)
            {
                const object_type& obj = (*itr);
                auto id = obj.id;

                if (sz > 0)
                {
                    fc::raw::unpack(_fstream, tmp);
                    auto loaded_id = obj.id;
                    objs_to_modify.emplace(std::make_pair(loaded_id, tmp));

                    --sz;
                }

                if (!sz || objs_to_modify.find(id) == objs_to_modify.end())
                {
                    objs_to_remove.emplace_back(std::cref(obj));
                }
            }

            for (const object_type& obj : objs_to_remove)
            {
#ifdef DEBUG_SNAPSHOTTED_OBJECT
                if (debug_id == object_type::type_id)
                {
                    fc::variant vo;
                    fc::to_variant(obj, vo);
                    snapshot_log(ctx, "removed ${name}: ${obj}",
                                 ("name", object_name)("obj", fc::json::to_pretty_string(vo)));
                }
#endif // DEBUG_SNAPSHOTTED_OBJECT
                _state.template remove(obj);
            }
        }

        for (auto& item : objs_to_modify)
        {
            const object_type* pobj = _state.template find<object_type>(item.first);
            if (pobj != nullptr)
            {
                _state.template modify<object_type>(*pobj, [&](object_type& obj) {
                    obj = std::move(item.second);
#ifdef DEBUG_SNAPSHOTTED_OBJECT
                    if (debug_id == object_type::type_id)
                    {
                        fc::variant vo;
                        fc::to_variant(obj, vo);
                        snapshot_log(ctx, "updated ${name}: ${obj}",
                                     ("name", object_name)("obj", fc::json::to_pretty_string(vo)));
                    }
#endif // DEBUG_SNAPSHOTTED_OBJECT
                });
            }
            else
            {
                _state.template create<object_type>([&](object_type& obj) {
                    obj = std::move(item.second);
#ifdef DEBUG_SNAPSHOTTED_OBJECT
                    if (debug_id == object_type::type_id)
                    {
                        fc::variant vo;
                        fc::to_variant(obj, vo);
                        snapshot_log(ctx, "created ${name}: ${obj}",
                                     ("name", object_name)("obj", fc::json::to_pretty_string(vo)));
                    }
#endif // DEBUG_SNAPSHOTTED_OBJECT
                });
            }
        }

        objs_to_modify.clear();

        while (sz--)
        {
            _state.template create<object_type>([&](object_type& obj) {
                fc::raw::unpack(_fstream, obj);
#ifdef DEBUG_SNAPSHOTTED_OBJECT
                if (debug_id == object_type::type_id)
                {
                    fc::variant vo;
                    fc::to_variant(obj, vo);
                    snapshot_log(ctx, "created ${name}: ${obj}",
                                 ("name", object_name)("obj", fc::json::to_pretty_string(vo)));
                }
#endif // DEBUG_SNAPSHOTTED_OBJECT
            });
        }
    }

private:
    std::ifstream& _fstream;
    db_state& _state;
};

template <class Section>
void load_index_section(std::ifstream& fstream,
                        chainbase::db_state& state,
                        scorum::snapshot::index_ids_type& loaded_idxs,
                        const Section& section)
{
    fc::ripemd160 check;

    fc::raw::unpack(fstream, check);

    fc::ripemd160::encoder check_enc;
    fc::raw::pack(check_enc, section.name);

    FC_ASSERT(check_enc.result() == check);

    state.for_each_index_key([&](uint16_t index_id) {
        bool initialized = false;
        auto v = section.get_object_type_variant(index_id, initialized);
        // checking because static variant interpret uninitialized state like first type
        if (initialized)
        {
            v.visit(load_index_visitor<by_id, Section>(fstream, state));
            loaded_idxs.insert(index_id);
        }
    });
}
}
}
