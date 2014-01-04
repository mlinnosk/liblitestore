#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "litestore_iterator.h"
#include "litestore_test_iterators.h"


namespace ls
{
using namespace ::testing;
using namespace ls::iter;

namespace
{

struct MapIteratorTest : Test
{
    MapIteratorTest()
        : Test(),
          strMap(),
          mapIter(strMap)
    {
        strMap["one"] = "one";
        strMap["two"] = "two";
    }

    StrMap strMap;
    MapIter mapIter;
};

struct VecIteratorTest : Test
{
    VecIteratorTest()
        : Test(),
          strVec(),
          vecIter(strVec)
    {
        strVec.push_back("one");
        strVec.push_back("two");
    }

    StrVec strVec;
    VecIter vecIter;
};

}  // namespace

TEST_F(MapIteratorTest, kv_iterator_for_map_iterates)
{
    litestore_kv_iterator iter = mapIter.getIter();
    int i = 0;

    for (iter.begin(iter.user_data);
         !iter.end(iter.user_data);
         iter.next(iter.user_data))
    {
        ++i;
    }
    EXPECT_EQ(2, i);

    for (iter.begin(iter.user_data);
         !iter.end(iter.user_data);
         iter.next(iter.user_data))
    {
        ++i;
    }
    EXPECT_EQ(4, i);
}

TEST_F(MapIteratorTest, kv_iterator_produces_values)
{
    litestore_kv_iterator iter = mapIter.getIter();
    StrMap m;
    for (iter.begin(iter.user_data);
         !iter.end(iter.user_data);
         iter.next(iter.user_data))
    {
        const char* k = NULL;
        size_t k_len = 0;
        const char* v = NULL;
        size_t v_len = 0;
        iter.value(iter.user_data,
                   &k, &k_len,
                   &v, &v_len);

        ASSERT_TRUE(k);
        ASSERT_TRUE(k_len > 0);
        ASSERT_TRUE(v);
        ASSERT_TRUE(v_len > 0);

        m[std::string(k, k_len)] = std::string(v, v_len);
    }

    EXPECT_EQ(strMap, m);
}

TEST_F(VecIteratorTest, array_iterator_for_vec_iterates)
{
    litestore_array_iterator iter = vecIter.getIter();
    int i = 0;

    for (iter.begin(iter.user_data);
         !iter.end(iter.user_data);
         iter.next(iter.user_data))
    {
        ++i;
    }
    EXPECT_EQ(2, i);

    for (iter.begin(iter.user_data);
         !iter.end(iter.user_data);
         iter.next(iter.user_data))
    {
        ++i;
    }
    EXPECT_EQ(4, i);
}

TEST_F(VecIteratorTest, array_iterator_produces_values)
{
    litestore_array_iterator iter = vecIter.getIter();
    StrVec vec;
    for (iter.begin(iter.user_data);
         !iter.end(iter.user_data);
         iter.next(iter.user_data))
    {
        const char* v = NULL;
        size_t v_len = 0;
        iter.value(iter.user_data, &v, &v_len);

        ASSERT_TRUE(v);
        ASSERT_TRUE(v_len > 0);

        vec.push_back(std::string(v, v_len));
    }

    EXPECT_EQ(strVec, vec);
}

}  // namespace ls
