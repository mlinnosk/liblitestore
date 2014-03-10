/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

#include "litestore.h"
#include "litestore_test_common.h"


namespace ls
{
using namespace ::testing;

namespace
{

struct RawData
{
    RawData(sqlite3_int64 id_,
            const std::string& rawValue_)
        : id(id_),
          rawValue(rawValue_)
    {}
    sqlite3_int64 id;
    std::string rawValue;
};
typedef std::vector<RawData> RawDatas;

struct RawDataIdIs
{
    RawDataIdIs(sqlite3_int64 id_)
        : id(id_)
    {}
    bool operator()(const RawData& obj) const
    {
        return id == obj.id;
    }
    sqlite3_int64 id;
};

struct LitestoreRawTest : LitestoreTest
{
    LitestoreRawTest()
        : LitestoreTest(),
          key("key"),
          rawData("raw_data")
    {}
    RawDatas readRawDatas()
    {
        RawDatas results;
        const char* s = "SELECT * FROM raw_data;";
        sqlite3_stmt* stmt = NULL;
        sqlite3_prepare_v2(db, s, -1, &stmt, NULL);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char* n =
                reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmt, 1));
            const int size = sqlite3_column_bytes(stmt, 1);
            const std::string name(n, size);

            results.push_back(
                RawData(sqlite3_column_int64(stmt, 0), name));
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);

        return results;
    }

    std::string key;
    std::string rawData;
};

struct LitestoreRawTx : public LitestoreRawTest
{
    LitestoreRawTx()
        : LitestoreRawTest()
    {}
    virtual void SetUp()
    {
        ASSERT_EQ(LITESTORE_OK, litestore_begin_tx(ctx));
    }
    virtual void TearDown()
    {
        ASSERT_EQ(LITESTORE_OK, litestore_rollback_tx(ctx));
    }
};

int void2str(litestore_blob_t value, void* user_data)
{
    std::string* str = static_cast<std::string*>(user_data);
    if (value.data && value.size > 0)
    {
        *str = std::string(static_cast<const char*>(value.data), value.size);
    }
    return LITESTORE_OK;
}

int failCb(litestore_blob_t, void*)
{
    return 100;
}

}  // namespace

TEST_F(LitestoreRawTest, check_version)
{
    sqlite3* db = (sqlite3*)litestore_native_ctx(ctx);
    sqlite3_stmt* s = NULL;
    const std::string stmt("SELECT schema_version FROM meta;");
    ASSERT_EQ(
        SQLITE_OK,
        sqlite3_prepare_v2(db, stmt.c_str(), stmt.length(), &s, NULL));
    if (sqlite3_step(s) == SQLITE_ROW)
    {
        EXPECT_EQ(1, sqlite3_column_int(s, 0));
        sqlite3_finalize(s);
    }
    else
    {
        sqlite3_finalize(s);
        FAIL();
    }
}

TEST_F(LitestoreRawTest, transactions_rollback)
{
    EXPECT_LS_OK(litestore_begin_tx(ctx));
    EXPECT_EQ(SQLITE_OK,
              sqlite3_exec(
                  db,
                  "INSERT INTO objects (name, type) VALUES (\"foo\", 0);",
                  NULL, NULL, NULL));
    EXPECT_EQ(1u, readObjects().size());
    EXPECT_LS_OK(litestore_rollback_tx(ctx));

    EXPECT_TRUE(readObjects().empty());
}

TEST_F(LitestoreRawTest, transactions_commit)
{
    EXPECT_LS_OK(litestore_begin_tx(ctx));
    EXPECT_EQ(SQLITE_OK,
              sqlite3_exec(
                  db,
                  "INSERT INTO objects (name, type) VALUES (\"foo\", 0);",
                  NULL, NULL, NULL));
    EXPECT_EQ(1u, readObjects().size());
    EXPECT_LS_OK(litestore_commit_tx(ctx));

    EXPECT_EQ(1u, readObjects().size());
}

TEST_F(LitestoreRawTx, create_null_creates)
{
    EXPECT_LS_OK(litestore_create_null(ctx, slice(key)));

    const Objects res = readObjects();
    ASSERT_EQ(1u, res.size());
    EXPECT_EQ(key, res[0].name);
    EXPECT_EQ(0, res[0].type);  // LS_NULL
}

TEST_F(LitestoreRawTx, create_null_fails_for_dupliates)
{
    EXPECT_LS_OK(litestore_create_null(ctx, slice(key)));
    EXPECT_LS_ERR(litestore_create_null(ctx, slice(key)));
    EXPECT_EQ(1u, errors.size());
}

TEST_F(LitestoreRawTx, create_raw_creates)
{
    EXPECT_LS_OK(litestore_create_raw(ctx, slice(key), blob(rawData)));

    const Objects res = readObjects();
    ASSERT_EQ(1u, res.size());
    EXPECT_EQ(key, res[0].name);
    EXPECT_EQ(1, res[0].type);  // LS_RAW
    const RawDatas res2 = readRawDatas();
    ASSERT_EQ(1u, res2.size());
    EXPECT_EQ(res[0].id, res2[0].id);
}

TEST_F(LitestoreRawTx, delete_nulls)
{
    litestore_create_null(ctx, slice(key));
    EXPECT_LS_OK(litestore_delete(ctx, slice(key)));
    EXPECT_TRUE(readObjects().empty());
}

TEST_F(LitestoreRawTx, delete_raws)
{
    litestore_create_raw(ctx, slice(key), blob(rawData));
    EXPECT_LS_OK(litestore_delete(ctx, slice(key)));
    EXPECT_TRUE(readObjects().empty());
    EXPECT_TRUE(readRawDatas().empty());
}

TEST_F(LitestoreRawTx, delete_returns_unknown)
{
    litestore_create_null(ctx, slice(key));
    const std::string foo("foo");
    EXPECT_EQ(LITESTORE_UNKNOWN_ENTITY, litestore_delete(ctx, slice(foo)));
}

TEST_F(LitestoreRawTx, read_null_gives_null)
{
    litestore_create_null(ctx, slice(key));
    EXPECT_LS_OK(litestore_read_null(ctx, slice(key)));
}

TEST_F(LitestoreRawTx, read_null_returns_unknown_for_wrong_type)
{
    EXPECT_LS_OK(litestore_create_raw(ctx, slice(key), blob(rawData)));
    EXPECT_LS_ERR(litestore_read_null(ctx, slice(key)));
}

TEST_F(LitestoreRawTx, read_raw_gives_data)
{
    litestore_create_raw(ctx, slice(key), blob(rawData));
    std::string data;
    EXPECT_LS_OK(litestore_read_raw(ctx, slice(key), &void2str, &data));
    EXPECT_EQ(rawData, data);
}

TEST_F(LitestoreRawTx, read_null_returns_unknown)
{
    EXPECT_EQ(LITESTORE_UNKNOWN_ENTITY,
              litestore_read_null(ctx, slice(key)));
}

TEST_F(LitestoreRawTx, read_null_with_bad_args)
{
    EXPECT_LS_ERR(litestore_read_null(ctx, litestore_slice(NULL, 0, 0)));
}

TEST_F(LitestoreRawTx, update_null_to_raw_to_null)
{
    litestore_create_null(ctx, slice(key));
    EXPECT_LS_OK(litestore_update_raw(ctx, slice(key), blob(rawData)));
    Objects objs = readObjects();
    ASSERT_EQ(1u, objs.size());
    EXPECT_EQ(1, objs[0].type);
    ASSERT_EQ(1u, readRawDatas().size());

    EXPECT_LS_OK(litestore_update_null(ctx, slice(key)));
    objs = readObjects();
    ASSERT_EQ(1u, objs.size());
    EXPECT_EQ(0, objs[0].type);
    ASSERT_TRUE(readRawDatas().empty());
}

TEST_F(LitestoreRawTx, update_existing_raw_value)
{
    litestore_create_raw(ctx, slice(key), blob(rawData));

    const std::string newData("new_data");
    ASSERT_LS_OK(litestore_update_raw(ctx, slice(key), blob(newData)));
    const RawDatas rawDatas = readRawDatas();
    ASSERT_EQ(1u, rawDatas.size());
    EXPECT_EQ(newData, rawDatas[0].rawValue);
}

TEST_F(LitestoreRawTx, read_raw_returns_unknown_for_wrong_type)
{
    EXPECT_LS_OK(litestore_create_null(ctx, slice(key)));

    std::string data;
    EXPECT_LS_ERR(litestore_read_raw(ctx, slice(key), &void2str, &data));
}

TEST_F(LitestoreRawTx, read_raw_returns_callback_error)
{
    litestore_create_raw(ctx, slice(key), blob(rawData));
    EXPECT_EQ(100, litestore_read_raw(ctx, slice(key), &failCb, NULL));
}

}  // namespace ls
