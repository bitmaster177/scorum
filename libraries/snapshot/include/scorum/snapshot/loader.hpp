#pragma once

#include <fstream>
#include <chainbase/db_state.hpp>

#include <scorum/snapshot/data_struct_hash.hpp>
#include <scorum/snapshot/get_types_by_id.hpp>

#include <fc/io/json.hpp>

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
        static const int debug_id = 9;

        using object_type = typename T::type;

        std::cerr << "loading " << object_type::type_id << ": " << boost::core::demangle(typeid(object_type).name())
                  << std::endl;

        size_t sz = 0;
        fc::raw::unpack(_fstream, sz);

        if (sz > 0)
        {
            fc::ripemd160 check, etalon;

            fc::raw::unpack(_fstream, check);

            const object_type* petalon_obj = _state.template find<object_type>();
            if (petalon_obj == nullptr)
            {
                const object_type& fake_obj = _state.template create<object_type>(
                    [&](object_type& obj) { etalon = get_data_struct_hash(obj); });
                _state.template remove(fake_obj);
            }
            else
            {
                etalon = get_data_struct_hash(*petalon_obj);
            }

            FC_ASSERT(check == etalon);
        }

        using object_id_type = typename object_type::id_type;
        using objects_copy_type = fc::shared_map<object_id_type, object_type>;
        using object_ref_type = std::reference_wrapper<const object_type>;
        using objects_type = std::vector<object_ref_type>;

        const auto& index = _state.template get_index<typename chainbase::get_index_type<object_type>::type>()
                                .indices()
                                .template get<IterationTag>();

        objects_copy_type objs_to_modify(index.get_allocator());

        if (debug_id == object_type::type_id)
        {
            std::cerr << object_type::type_id << ": saved sz = " << sz << ", index sz = " << index.size() << std::endl;
        }

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
                    bool new_item = objs_to_modify.emplace(std::make_pair(id, obj)).second;
                    // TODO: new_item overwrited if not new!!!
                    object_type& obj = objs_to_modify.at(id);
                    fc::raw::unpack(_fstream, obj);
                    auto loaded_id = obj.id;
                    objs_to_modify.emplace(std::make_pair(loaded_id, obj));
                    if (new_item && loaded_id != id)
                    {
                        objs_to_modify.erase(id);
                    }

                    --sz;
                }

                if (!sz || objs_to_modify.find(id) == objs_to_modify.end())
                {
                    objs_to_remove.emplace_back(std::cref(obj));
                }
            }

            for (const object_type& obj : objs_to_remove)
            {
                if (debug_id == object_type::type_id)
                {
                    fc::variant vo;
                    fc::to_variant(obj, vo);
                    std::cerr << "removed " << boost::core::demangle(typeid(object_type).name()) << ":"
                              << fc::json::to_pretty_string(vo) << std::endl;
                }
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
                    if (debug_id == object_type::type_id)
                    {
                        fc::variant vo;
                        fc::to_variant(obj, vo);
                        std::cerr << "updated " << boost::core::demangle(typeid(object_type).name()) << ":"
                                  << fc::json::to_pretty_string(vo) << std::endl;
                    }
                });
            }
            else
            {
                _state.template create<object_type>([&](object_type& obj) {
                    obj = std::move(item.second);
                    if (debug_id == object_type::type_id)
                    {
                        fc::variant vo;
                        fc::to_variant(obj, vo);
                        std::cerr << "created " << boost::core::demangle(typeid(object_type).name()) << ": "
                                  << fc::json::to_pretty_string(vo) << std::endl;
                    }
                });
            }
        }

        objs_to_modify.clear();

        while (sz--)
        {
            _state.template create<object_type>([&](object_type& obj) {
                fc::raw::unpack(_fstream, obj);
                if (debug_id == object_type::type_id)
                {
                    fc::variant vo;
                    fc::to_variant(obj, vo);
                    std::cerr << "created " << boost::core::demangle(typeid(object_type).name()) << ": "
                              << fc::json::to_pretty_string(vo) << std::endl;
                }
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
