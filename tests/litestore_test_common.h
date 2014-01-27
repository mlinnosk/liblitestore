/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

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

inline
void errorCB(const int error, const char* errStr, void* user_data);

struct LitestoreTest : Test
{
    LitestoreTest()
        : ctx(NULL),
          db(NULL),
          errors(0)
    {
        litestore_opts opts = {&errorCB, this};
        if (litestore_open(":memory:", opts, &ctx) != LITESTORE_OK)
        {
            throw std::runtime_error("Faild to open DB!");
        }
        db = static_cast<sqlite3*>(litestore_native_ctx(ctx));
    }
    virtual ~LitestoreTest()
    {
        dropDB();
        litestore_close(ctx);
    }
    void lsError(const std::string& errStr)
    {
        errors.push_back(errStr);
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

    litestore* ctx;
    sqlite3* db;
    std::vector<std::string> errors;
};

inline
void errorCB(const int error, const char* errStr, void* user_data)
{
    LitestoreTest* t = static_cast<LitestoreTest*>(user_data);
    t->lsError(errStr);
}


#define EXPECT_LS_OK(pred) EXPECT_EQ(LITESTORE_OK, pred)
#define EXPECT_LS_ERR(pred) EXPECT_EQ(LITESTORE_ERR, pred)
#define ASSERT_LS_OK(pred) ASSERT_EQ(LITESTORE_OK, pred)
#define ASSERT_LS_ERR(pred) ASSERT_EQ(LITESTORE_ERR, pred)

inline
litestore_slice_t slice(const std::string& str)
{
    return litestore_slice(str.c_str(), 0, str.length());
}

inline
litestore_blob_t blob(const std::string& str)
{
    return litestore_make_blob(str.c_str(), str.length());
}

}  // namespace ns
