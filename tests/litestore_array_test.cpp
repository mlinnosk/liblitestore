#include <gtest/gtest.h>

#include "litestore.h"
#include "litestore_test_common.h"
#include "litestore_test_iterators.h"

namespace ls
{
namespace
{

struct LitestoreArrayTest : public LitestoreTest
{
    LitestoreArrayTest()
        : LitestoreTest(),
          key("key"),
          data(),
          dataIter(data)
    {
        data.push_back("foo");
        data.push_back("bar");
        data.push_back("foo_bar");
    }
    virtual void SetUp()
    {
        ASSERT_LS_OK(litestore_begin_tx(ctx));
    }
    virtual void TearDown()
    {
        ASSERT_LS_OK(litestore_rollback_tx(ctx));
    }
    iter::StrVec readArrayDatas()
    {
        iter::StrVec results;
        const char* s = "SELECT * FROM array_data ORDER BY array_index;";
        sqlite3_stmt* stmt = NULL;
        sqlite3_prepare_v2(db, s, -1, &stmt, NULL);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char* n =
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmt, 2));
            const int size = sqlite3_column_bytes(stmt, 2);
            results.push_back(std::string(n, size));
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);

        return results;
    }

    std::string key;
    iter::StrVec data;
    iter::VecIter dataIter;
};

int toStrVec(litestore_slice_t /* key */,
             const unsigned /* index */,
             litestore_blob_t data,
             void* user_data)
{
    if (data.data && data.size > 0)
    {
        iter::StrVec* v = static_cast<iter::StrVec*>(user_data);
        v->push_back(std::string(static_cast<const char*>(data.data),
                                 data.size));
        return LITESTORE_OK;
    }
    return LITESTORE_ERR;
}

}  // namespace

TEST_F(LitestoreArrayTest, save_array_saves)
{
    ASSERT_LS_OK(litestore_save_array(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(data, readArrayDatas());
}

TEST_F(LitestoreArrayTest, get_array_gets)
{
    litestore_save_array(ctx, slice(key), dataIter.getIter());
    iter::StrVec res;
    ASSERT_LS_OK(litestore_get_array(ctx, slice(key), &toStrVec, &res));
    EXPECT_EQ(data, res);
}

TEST_F(LitestoreArrayTest, get_array_returns_error_for_non_existing)
{
    iter::StrVec res;
    ASSERT_LS_ERR(litestore_get_array(ctx, slice(key), &toStrVec, &res));
}

TEST_F(LitestoreArrayTest, delete_deletes_array_data)
{
    litestore_save_array(ctx, slice(key), dataIter.getIter());
    ASSERT_LS_OK(litestore_delete(ctx, slice(key)));

    EXPECT_TRUE(readObjects().empty());
    EXPECT_TRUE(readArrayDatas().empty());
}

TEST_F(LitestoreArrayTest, array_to_null_to_array_to_raw_to_array)
{
    litestore_save_array(ctx, slice(key), dataIter.getIter());
    EXPECT_EQ(2, readObjects()[0].type);  // LS_ARRAY

    ASSERT_LS_OK(litestore_update_null(ctx, slice(key)));
    EXPECT_EQ(0, readObjects()[0].type);  // LS_NULL
    ASSERT_TRUE(readArrayDatas().empty());

    ASSERT_LS_OK(litestore_update_array(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(2, readObjects()[0].type);  // LS_ARRAY
    ASSERT_FALSE(readArrayDatas().empty());

    const std::string raw("raw_data");
    ASSERT_LS_OK(litestore_update_raw(ctx, slice(key), blob(raw)));
    EXPECT_EQ(1, readObjects()[0].type);  // LS_RAW
    ASSERT_TRUE(readArrayDatas().empty());

    ASSERT_LS_OK(litestore_update_array(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(2, readObjects()[0].type);  // LS_ARRAY
    ASSERT_FALSE(readArrayDatas().empty());
}

TEST_F(LitestoreArrayTest, get_array_returns_error_for_null)
{
    litestore_save_null(ctx, slice(key));
    iter::StrVec result;
    EXPECT_LS_ERR(litestore_get_array(ctx, slice(key), &toStrVec, &result));
}

TEST_F(LitestoreArrayTest, update_existing_contents)
{
    litestore_save_array(ctx, slice(key), dataIter.getIter());

    data[0] = "1";
    data[1] = "2";
    data[2] = "3";
    ASSERT_LS_OK(litestore_update_array(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(data, readArrayDatas());
}

TEST_F(LitestoreArrayTest, add_new_data)
{
    litestore_save_array(ctx, slice(key), dataIter.getIter());

    data.push_back("1");
    data.push_back("2");
    ASSERT_LS_OK(litestore_update_array(ctx, slice(key), dataIter.getIter()));
    EXPECT_EQ(data, readArrayDatas());
}


}  // namespace ls
