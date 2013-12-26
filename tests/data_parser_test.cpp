#include <gtest/gtest.h>

#include <cstring>

#include <map>
#include <vector>

#include "litestore.h"
#include "litestore_data_parser.h"


namespace ls
{
namespace
{
using namespace ::testing;

struct ParsingResults
{
    ParsingResults()
        : array(),
          obj()
    {}
    std::vector<std::string> array;
    std::map<std::string, std::string> obj;
};

void arrayDataCB(const char* d, const size_t len, void* userData)
{
    reinterpret_cast<ParsingResults*>(userData)
        ->array.push_back(std::string(d, len));
}

void objDataCB(const char* k, const size_t kLen,
               const char* d, const size_t dLen,
               void* userData)
{
    reinterpret_cast<ParsingResults*>(userData)
        ->obj[std::string(k, kLen)] = std::string(d, dLen);
}

struct DataParserTest : Test
{
    DataParserTest()
        : Test(),
          data(),
          ctx(),
          nullCtx()
    {
        ctx.array_cb = &arrayDataCB;
        ctx.obj_cb = &objDataCB;
        ctx.user_data = &data;
        std::memset(&nullCtx, 0, sizeof(nullCtx));
    }
    ParsingResults data;
    litestore_parser_ctx ctx;
    litestore_parser_ctx nullCtx;
};

}  // namespace

#define ASSERT_LS_OK(pred) ASSERT_EQ(LITESTORE_OK, pred)
#define EXPECT_LS_ERR(pred) EXPECT_EQ(LITESTORE_ERR, pred)

TEST_F(DataParserTest, parse_type_works_on_good_input)
{
    int type = -1;

    std::string str("{\"foo\" : \"bar\", \"bar\": 1, \"a\":\"\"}");
    ASSERT_LS_OK(litestore_data_parse_get_type(str.c_str(), str.length(),
                                               &type));
    EXPECT_EQ(LS_DP_OBJ, type);

    str = "[{\"foo\" : \"bar\"}, \"bar\" , 1 \n  ]";
    ASSERT_LS_OK(litestore_data_parse_get_type(str.c_str(), str.length(),
                                               &type));
    EXPECT_EQ(LS_DP_ARRAY, type);

    str = "{  }";
    ASSERT_LS_OK(litestore_data_parse_get_type(str.c_str(), str.length(),
                                               &type));
    EXPECT_EQ(LS_DP_EMPTY_OBJ, type);

    str = "[  ]";
    ASSERT_LS_OK(litestore_data_parse_get_type(str.c_str(), str.length(),
                                               &type));
    EXPECT_EQ(LS_DP_EMPTY_ARRAY, type);
}

TEST_F(DataParserTest, get_type_returns_error)
{
    std::string str("foo");
    EXPECT_LS_ERR(litestore_data_parse_get_type(str.c_str(), str.length(),
                                                NULL));
    str = "";
    EXPECT_LS_ERR(litestore_data_parse_get_type(str.c_str(), str.length(),
                                                NULL));
}

TEST_F(DataParserTest, parses_objects)
{
    const std::string str("{\"foo\" : \"bar\", \"bar\": 1, \"a\":\"\"}");
    ASSERT_LS_OK(litestore_data_parse(str.c_str(), str.length(), ctx));

    ASSERT_EQ(3u, data.obj.size());
    EXPECT_EQ("\"bar\"", data.obj["\"foo\""]);
    EXPECT_EQ("1", data.obj["\"bar\""]);
    EXPECT_EQ("\"\"", data.obj["\"a\""]);
}

TEST_F(DataParserTest, parses_array)
{
    const std::string str("[{\"foo\" : \"bar\"}, \"bar\" , 1 \n  ]");
    ASSERT_LS_OK(litestore_data_parse(str.c_str(), str.length(), ctx));

    ASSERT_EQ(3u, data.array.size());
    EXPECT_EQ("{\"foo\" : \"bar\"}", data.array[0]);
    EXPECT_EQ("\"bar\"", data.array[1]);
    EXPECT_EQ("1", data.array[2]);
}

TEST_F(DataParserTest, pase_fails_for_invalid)
{
    EXPECT_LS_ERR(litestore_data_parse("", 0, nullCtx));
    const std::string str1("foo");
    EXPECT_LS_ERR(litestore_data_parse(str1.c_str(), str1.length(),
                                       nullCtx));
    const std::string str2("[\"foo\"");
    EXPECT_LS_ERR(litestore_data_parse(str2.c_str(), str2.length(),
                                       nullCtx));
    const std::string str3("[\"foo\":1}");
    EXPECT_LS_ERR(litestore_data_parse(str3.c_str(), str3.length(),
                                       nullCtx));
    const std::string str4("{\"foo\":}");
    EXPECT_LS_ERR(litestore_data_parse(str4.c_str(), str4.length(),
                                       nullCtx));
}

}  // namespace litestore
