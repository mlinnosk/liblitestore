#include <gtest/gtest.h>

#include "litestore_helpers.h"

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
    litestore_slice_t slice = litestore_slice(orig, 3);

    EXPECT_EQ(3u, slice.length);
    EXPECT_TRUE(slice.data);
    char* copy = strndup(slice.data, slice.length);
    EXPECT_STREQ("foo", copy);
    free(copy);
}
