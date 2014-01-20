/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
#include <gtest/gtest.h>

#include "litestore_test_common.h"
#include "litestore_test_iterators.h"


namespace ls
{
namespace
{

struct KVData
{
    KVData(sqlite3_int64 id_,
           const std::string& key_,
           const std::string& value_)
        : id(id_),
          key(key_),
          value(value_)
    {}
    sqlite3_int64 id;
    std::string key;
    std::string value;
};
typedef std::vector<KVData> KVDatas;


struct LitestoreKVTest : public LitestoreTest
{
    LitestoreKVTest()
        : LitestoreTest(),
          key("key"),
          data(),
          dataIter(data)
    {
        data["foo"] = "bar";
        data["bar"] = "1";
    }
    virtual void SetUp()
    {
        ASSERT_LS_OK(litestore_begin_tx(ctx));
    }
    virtual void TearDown()
    {
        ASSERT_LS_OK(litestore_rollback_tx(ctx));
    }
    KVDatas readKVDatas()
    {
        KVDatas results;
        const char* s = "SELECT * FROM kv_data;";
        sqlite3_stmt* stmt = NULL;
        sqlite3_prepare_v2(db, s, -1, &stmt, NULL);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char* n =
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmt, 1));
            int size = sqlite3_column_bytes(stmt, 1);
            const std::string key(n, size);

            n = reinterpret_cast<const char*>(
                sqlite3_column_blob(stmt, 2));
            size = sqlite3_column_bytes(stmt, 2);
            const std::string value(n, size);

            results.push_back(
                KVData(sqlite3_column_int64(stmt, 0), key, value));
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);

        return results;
    }
    iter::StrMap kvDataToMap(const KVDatas& datas)
    {
        iter::StrMap m;
        for (KVDatas::const_iterator kv = datas.begin();
             kv != datas.end();
             ++kv)
        {
            m.insert(std::make_pair(kv->key, kv->value));
        }
        return m;
    }

    std::string key;
    iter::StrMap data;
    iter::MapIter dataIter;
};

int buildMap(litestore_slice_t /* key */,
             litestore_blob_t kv_key, litestore_blob_t kv_value,
             void* user_data)
{
    if (kv_key.data && kv_key.size)
    {
        iter::StrMap* m = static_cast<iter::StrMap*>(user_data);
        if (kv_value.data && kv_value.size)
        {
            m->insert(std::make_pair(
                          std::string(static_cast<const char*>(kv_key.data),
                                      kv_key.size),
                          std::string(static_cast<const char*>(kv_value.data),
                                      kv_value.size)));
        }
        else
        {
            m->insert(std::make_pair(
                          std::string(static_cast<const char*>(kv_key.data),
                                      kv_key.size),
                          std::string()));
        }
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

}  // namespace


TEST_F(LitestoreKVTest, save_kv_saves)
{
    ASSERT_LS_OK(litestore_save_kv(ctx, slice(key), dataIter.getIter()));

    const Objects res = readObjects();
    ASSERT_EQ(1u, res.size());
    EXPECT_EQ(key, res[0].name);
    EXPECT_EQ(3, res[0].type);  // LS_KV

    EXPECT_EQ(data, kvDataToMap(readKVDatas()));
}

TEST_F(LitestoreKVTest, delete_deletes_kvs)
{
    ASSERT_LS_OK(litestore_save_kv(ctx, slice(key), dataIter.getIter()));
    ASSERT_LS_OK(litestore_delete(ctx, slice(key)));

    EXPECT_TRUE(readObjects().empty());
    EXPECT_TRUE(readKVDatas().empty());
}

TEST_F(LitestoreKVTest, get_kv_retrieves_data)
{
    ASSERT_LS_OK(litestore_save_kv(ctx, slice(key), dataIter.getIter()));
    iter::StrMap result;
    ASSERT_LS_OK(litestore_get_kv(ctx, slice(key), &buildMap, &result));
    EXPECT_EQ(data, result);
}

TEST_F(LitestoreKVTest, get_kv_returns_error_for_null)
{
    ASSERT_LS_OK(litestore_save_null(ctx, slice(key)));
    iter::StrMap result;
    EXPECT_LS_ERR(litestore_get_kv(ctx, slice(key), &buildMap, &result));
}

TEST_F(LitestoreKVTest, kv_to_null_to_kv_to_raw_to_kv)
{
    ASSERT_LS_OK(litestore_save_kv(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(3, readObjects()[0].type);  // LS_KV

    ASSERT_LS_OK(litestore_update_null(ctx, slice(key)));
    EXPECT_EQ(0, readObjects()[0].type);  // LS_NULL
    ASSERT_TRUE(readKVDatas().empty());

    ASSERT_LS_OK(litestore_update_kv(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(3, readObjects()[0].type);  // LS_KV
    ASSERT_FALSE(readKVDatas().empty());

    const std::string raw("raw_data");
    ASSERT_LS_OK(litestore_update_raw(ctx, slice(key), blob(raw)));
    EXPECT_EQ(1, readObjects()[0].type);  // LS_RAW
    ASSERT_TRUE(readKVDatas().empty());

    ASSERT_LS_OK(litestore_update_kv(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(3, readObjects()[0].type);  // LS_KV
    ASSERT_FALSE(readKVDatas().empty());
}

TEST_F(LitestoreKVTest, update_existing_contents)
{
    ASSERT_LS_OK(litestore_save_kv(ctx, slice(key), dataIter.getIter()));

    data["foo"] = "foo";
    data["bar"] = "bar";
    ASSERT_LS_OK(litestore_update_kv(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(data, kvDataToMap(readKVDatas()));
}

TEST_F(LitestoreKVTest, add_new_keys)
{
    ASSERT_LS_OK(litestore_save_kv(ctx, slice(key), dataIter.getIter()));

    iter::StrMap m;
    m["test1"] = "1";
    m["test2"] = "2";
    iter::MapIter iter(m);
    ASSERT_LS_OK(litestore_update_kv(ctx, slice(key), iter.getIter()));
    m.insert(data.begin(), data.end());
    EXPECT_EQ(m, kvDataToMap(readKVDatas()));
}

}  // namespace ls
