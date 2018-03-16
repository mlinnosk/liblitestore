/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
#include <gtest/gtest.h>

#include "litestore/litestore_helpers.h"

TEST(LitestoreHelpers, make_blob_returns_object)
{
    int i = 10;
    litestore_blob_t b = litestore_make_blob(&i, sizeof(i));

    EXPECT_TRUE(b.data);
    EXPECT_EQ(sizeof(i), b.size);
    int j = 0;
    memcpy(&j, b.data, b.size);
    EXPECT_EQ(i, j);
}

TEST(LitestoreHelpers, slice_returns_object)
{
    const char* orig = "foo_bar";
    litestore_slice_t slice = litestore_slice(orig, 4, strlen(orig));

    EXPECT_EQ(3u, slice.length);
    EXPECT_TRUE(slice.data);
    char* copy = strndup(slice.data, slice.length);
    EXPECT_STREQ("bar", copy);
    free(copy);
}

TEST(LitestoreHelpers, slice_returns_null_on_invalid_input)
{
    const char* orig = "foo";
    litestore_slice_t slice = litestore_slice(orig, 2, 2);
    EXPECT_FALSE(slice.data);
    EXPECT_EQ(0u, slice.length);
}

TEST(LitestoreHelpers, slice_str_returns_object)
{
    const char* orig = "foo_bar";
    litestore_slice_t slice = litestore_slice_str(orig);

    EXPECT_EQ(strlen(orig), slice.length);
    EXPECT_EQ(orig, slice.data);
}
