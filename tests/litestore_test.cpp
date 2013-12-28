#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

#include "litestore.h"


namespace ls
{
using namespace ::testing;

// Structure for objects table rows.
struct Obj
{
    Obj(sqlite3_int64 id_,
        const std::string& name_,
        int type_)
        : id(id_),
          name(name_),
          type(type_)
    {}
    sqlite3_int64 id;
    std::string name;
    int type;
};
typedef std::vector<Obj> Objects;

struct ObjNameIs
{
    ObjNameIs(const std::string& name_)
        : name(name_)
    {}
    bool operator()(const Obj& obj) const
    {
        return name == obj.name;
    }
    std::string name;
};

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

struct JsonObjectData
{
    JsonObjectData(sqlite3_int64 id_,
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
typedef std::vector<JsonObjectData> JsonObjectDatas;


struct LiteStore : Test
{
    LiteStore()
        : ctx(NULL),
          db(NULL),
          key("key"),
          rawData("raw_data"),
          jsonObject("{\"foo\":\"bar\",\"bar\":1}"),
          jsonArray("[\"foo\",1,{\"foo\":\"bar\"}]")
    {
        if (litestore_open("/tmp/ls_test.db", &ctx) != LITESTORE_OK)
        {
            throw std::runtime_error("Faild to open DB!");
        }
        db = reinterpret_cast<sqlite3*>(litestore_native_ctx(ctx));
    }
    virtual ~LiteStore()
    {
        dropDB();
        litestore_close(ctx);
    }

    void dropDB()
    {
        sqlite3_exec(db, "DELETE FROM objects;", NULL, NULL, NULL);
    }

    bool contains(const std::string& key)
    {
        const Objects res = readObjects();
        return (std::find_if(res.begin(), res.end(), ObjNameIs(key))
                != res.end());
    }

    Objects readObjects()
    {
        Objects results;
        const char* s = "SELECT * FROM objects;";
        sqlite3_stmt* stmt = NULL;
        sqlite3_prepare_v2(db, s, -1, &stmt, NULL);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char* n =
                reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, 1));
            const int size = sqlite3_column_bytes(stmt, 1);
            const std::string name(n, size);

            results.push_back(
                Obj(sqlite3_column_int64(stmt, 0),
                    name,
                    sqlite3_column_int(stmt, 2)));
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);

        return results;
    }

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
    JsonObjectDatas readJsonObjectDatas()
    {
        JsonObjectDatas results;
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
                JsonObjectData(sqlite3_column_int64(stmt, 0), key, value));
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);

        return results;
    }


    litestore* ctx;
    sqlite3* db;
    std::string key;
    std::string rawData;
    std::string jsonObject;
    std::string jsonArray;
};

struct LiteStoreTx : public LiteStore
{
    LiteStoreTx()
        : LiteStore()
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

#define EXPECT_LS_OK(pred) EXPECT_EQ(LITESTORE_OK, pred)
#define EXPECT_LS_ERR(pred) EXPECT_EQ(LITESTORE_ERR, pred)


TEST_F(LiteStore, open_and_close)
{
    SUCCEED();
}

TEST_F(LiteStore, transactions_rollback)
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

TEST_F(LiteStore, transactions_commit)
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

TEST_F(LiteStoreTx, save_null_saves)
{
    EXPECT_LS_OK(litestore_save_null(ctx, key.c_str(), key.length()));

    const Objects res = readObjects();
    ASSERT_EQ(1u, res.size());
    EXPECT_EQ(key, res[0].name);
    EXPECT_EQ(0, res[0].type);  // LS_NULL
}

TEST_F(LiteStoreTx, save_null_fails_for_dupliates)
{
    EXPECT_LS_OK(litestore_save_null(ctx, key.c_str(), key.length()));
    EXPECT_LS_ERR(litestore_save_null(ctx, key.c_str(), key.length()));
}

TEST_F(LiteStoreTx, save_raw_saves)
{
    EXPECT_LS_OK(litestore_save_raw(ctx, key.c_str(), key.length(),
                                    rawData.c_str(), rawData.length()));

    const Objects res = readObjects();
    ASSERT_EQ(1u, res.size());
    EXPECT_EQ(key, res[0].name);
    EXPECT_EQ(1, res[0].type);  // LS_RAW
    const RawDatas res2 = readRawDatas();
    ASSERT_EQ(1u, res2.size());
    EXPECT_EQ(res[0].id, res2[0].id);
}

TEST_F(LiteStoreTx, delete_nulls)
{
    litestore_save_null(ctx, key.c_str(), key.length());
    EXPECT_LS_OK(litestore_delete(ctx, key.c_str(), key.length()));
    EXPECT_TRUE(readObjects().empty());
}

TEST_F(LiteStoreTx, delete_raws)
{
    litestore_save_raw(ctx, key.c_str(), key.length(),
                       rawData.c_str(), rawData.length());
    EXPECT_LS_OK(litestore_delete(ctx, key.c_str(), key.length()));
    EXPECT_TRUE(readObjects().empty());
    EXPECT_TRUE(readRawDatas().empty());
}

TEST_F(LiteStoreTx, delete_returns_unknown)
{
    litestore_save_null(ctx, key.c_str(), key.length());
    const std::string foo("foo");
    EXPECT_EQ(LITESTORE_UNKNOWN_ENTITY,
              litestore_delete(ctx, foo.c_str(), foo.length()));
}

TEST_F(LiteStoreTx, get_null_gives_null)
{
    litestore_save_null(ctx, key.c_str(), key.length());
    EXPECT_LS_OK(litestore_get_null(ctx, key.c_str(), key.length()));
}

TEST_F(LiteStoreTx, get_null_returns_unknown_for_wrong_type)
{
    EXPECT_LS_OK(litestore_save_raw(ctx, key.c_str(), key.length(),
                                    rawData.c_str(), rawData.length()));
    EXPECT_LS_ERR(litestore_get_null(ctx, key.c_str(), key.length()));
}

TEST_F(LiteStoreTx, get_raw_gives_data)
{
    litestore_save_raw(ctx, key.c_str(), key.length(),
                       rawData.c_str(), rawData.length());
    char* data = NULL;
    std::size_t len = 0;
    EXPECT_LS_OK(litestore_get_raw(ctx, key.c_str(), key.length(),
                                   &data, &len));
    ASSERT_TRUE(data);
    ASSERT_TRUE(len > 0);
    const std::string res(data, len);
    free(data);

    EXPECT_EQ(rawData, res);
}

TEST_F(LiteStoreTx, get_null_returns_unknown)
{
    EXPECT_EQ(LITESTORE_UNKNOWN_ENTITY,
              litestore_get_null(ctx, key.c_str(), key.length()));
}

TEST_F(LiteStoreTx, get_null_with_bad_args)
{
    EXPECT_LS_ERR(litestore_get_null(ctx, NULL, 0));
}

TEST_F(LiteStoreTx, update_null_to_raw_to_null)
{
    litestore_save_null(ctx, key.c_str(), key.length());
    EXPECT_LS_OK(litestore_update_raw(ctx, key.c_str(), key.length(),
                                      rawData.c_str(), rawData.length()));
    Objects objs = readObjects();
    ASSERT_EQ(1u, objs.size());
    EXPECT_EQ(1, objs[0].type);
    ASSERT_EQ(1u, readRawDatas().size());

    EXPECT_LS_OK(litestore_update_null(ctx, key.c_str(), key.length()));
    objs = readObjects();
    ASSERT_EQ(1u, objs.size());
    EXPECT_EQ(0, objs[0].type);
    ASSERT_TRUE(readRawDatas().empty());
}

TEST_F(LiteStoreTx, get_raw_returns_unknown_for_wrong_type)
{
    EXPECT_LS_OK(litestore_save_null(ctx, key.c_str(), key.length()));

    char* data = NULL;
    std::size_t len = 0;
    EXPECT_LS_ERR(litestore_get_raw(ctx, key.c_str(), key.length(),
                                    &data, &len));
    free(data);
}

// TEST_F(LiteStoreTx, save_saves_json_object)
// {
//     EXPECT_LS_OK(litestore_save_kv(ctx, key.c_str(), key.length(),
//                                   jsonObject.c_str(),
//                                   jsonObject.length()));

//     const Objects res = readObjects();
//     ASSERT_EQ(1u, res.size());
//     EXPECT_EQ(key, res[0].name);
//     EXPECT_EQ(5, res[0].type);  // LS_OBJECT

//     const JsonObjectDatas datas = readJsonObjectDatas();
//     ASSERT_EQ(2u, datas.size());
//     EXPECT_EQ("\"foo\"", datas[0].key);
//     EXPECT_EQ("\"bar\"", datas[0].value);
//     EXPECT_EQ("\"bar\"", datas[1].key);
//     EXPECT_EQ("1", datas[1].value);
// }

}  // namespace litestore
