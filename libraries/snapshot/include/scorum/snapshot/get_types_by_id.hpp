#pragma once

#include <scorum/typeid/get_types.hpp>

#include <boost/preprocessor.hpp>

#include <fc/static_variant.hpp>
#include <string>
#include <scorum/snapshot/serializer_extensions.hpp>

#include <chainbase/chain_object.hpp>

#include <boost/container/flat_set.hpp>

namespace scorum {
namespace snapshot {
using index_ids_type = fc::flat_set<uint16_t>;
}
}

#define DEBUG_SNAPSHOTTED_OBJECT key_lookup_object

#define snapshot_log(CTX, FORMAT, ...) fc_ctx_dlog(fc::logger::get("snapshot"), CTX, FORMAT, __VA_ARGS__)

#define SCORUM_OBJECT_TYPE_EXTRACTION_MAKE_TYPE(elem) get_object_type<BOOST_PP_CAT(elem, _object_type)>

#define SCORUM_OBJECT_TYPE_EXTRACTION_MAKE_TYPE_LIST(_1, _2, n, elem)                                                  \
    BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(n, 0)) SCORUM_OBJECT_TYPE_EXTRACTION_MAKE_TYPE(elem)

#define SCORUM_OBJECT_TYPE_EXTRACTION_RETURN_TYPE(_1, enum_type, elem)                                                 \
    case enum_type::BOOST_PP_SEQ_CAT((elem)(_object_type)):                                                            \
        return object_type_variant_type(get_object_type<BOOST_PP_CAT(elem, _object_type)>());

// use this macro to extract object type by runtime id
#define SCORUM_OBJECT_TYPES_FOR_SNAPSHOT_SECTION(SECTION_NAME, ENUM, FIELDS)                                           \
    namespace scorum {                                                                                                 \
    namespace snapshot {                                                                                               \
    struct SECTION_NAME                                                                                                \
    {                                                                                                                  \
        using object_type_variant_type                                                                                 \
            = fc::static_variant<BOOST_PP_SEQ_FOR_EACH_I(SCORUM_OBJECT_TYPE_EXTRACTION_MAKE_TYPE_LIST, _, FIELDS)>;    \
        std::string name = BOOST_PP_STRINGIZE(SECTION_NAME);                                                           \
        object_type_variant_type get_object_type_variant(int id, bool& initialized) const                              \
        {                                                                                                              \
            initialized = true;                                                                                        \
            switch (id)                                                                                                \
            {                                                                                                          \
                BOOST_PP_SEQ_FOR_EACH(SCORUM_OBJECT_TYPE_EXTRACTION_RETURN_TYPE, ENUM, FIELDS)                         \
            default:                                                                                                   \
                initialized = false;                                                                                   \
            }                                                                                                          \
            return object_type_variant_type();                                                                         \
        }                                                                                                              \
    };                                                                                                                 \
    }                                                                                                                  \
    }
