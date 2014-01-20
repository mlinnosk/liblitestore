/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */#include <map>
#include <string>
#include <vector>

#include "litestore_iterator.h"


namespace ls
{
namespace iter
{

typedef std::map<std::string, std::string> StrMap;

// iterator for StrMap
struct MapIter
{
    MapIter(StrMap& m)
        : map(m),
          curr(map.end())
    {}
    static void begin(void* user_data)
    {
        MapIter* mi = static_cast<MapIter*>(user_data);
        mi->curr = mi->map.begin();
    }
    static int end(void* user_data)
    {
        MapIter* mi = static_cast<MapIter*>(user_data);
        return (mi->curr == mi->map.end() ? 1 : 0);
    }
    static void next(void* user_data)
    {
        MapIter* mi = static_cast<MapIter*>(user_data);
        ++(mi->curr);
    }
    static void value(void* user_data,
                      const void** key, size_t* key_len,
                      const void** value, size_t* value_len)
    {
        MapIter* mi = static_cast<MapIter*>(user_data);
        *key = mi->curr->first.c_str();
        *key_len = mi->curr->first.length();
        *value = mi->curr->second.c_str();
        *value_len = mi->curr->second.length();
    }
    litestore_kv_iterator getIter()
    {
        litestore_kv_iterator i =
        {
            this,
            &MapIter::begin,
            &MapIter::end,
            &MapIter::next,
            &MapIter::value
        };
        return i;
    }

    StrMap& map;
    StrMap::iterator curr;
};

typedef std::vector<std::string> StrVec;

// iterator for StrVec
struct VecIter
{
    VecIter(StrVec& v)
       : vec(v),
         curr(vec.end())
    {}

    static void begin(void* user_data)
    {
        VecIter* mi = static_cast<VecIter*>(user_data);
        mi->curr = mi->vec.begin();
    }
    static int end(void* user_data)
    {
        VecIter* mi = static_cast<VecIter*>(user_data);
        return (mi->curr == mi->vec.end() ? 1 : 0);
    }
    static void next(void* user_data)
    {
        VecIter* mi = static_cast<VecIter*>(user_data);
        ++(mi->curr);
    }
    static void value(void* user_data,
                      const void** value, size_t* value_len)
    {
        VecIter* mi = static_cast<VecIter*>(user_data);
        *value = mi->curr->c_str();
        *value_len = mi->curr->length();
    }
    litestore_array_iterator getIter()
    {
        litestore_array_iterator i =
        {
            this,
            &VecIter::begin,
            &VecIter::end,
            &VecIter::next,
            &VecIter::value
        };
        return i;
    }

    StrVec& vec;
    StrVec::iterator curr;
};

}  // namespace iter
}  // namespace ls
