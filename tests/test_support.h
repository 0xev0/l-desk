#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_ASSERT_TRUE(value) test_assert_true((value), #value, __FILE__, __LINE__)
#define TEST_ASSERT_FALSE(value) test_assert_false((value), #value, __FILE__, __LINE__)
#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    test_assert_equal_int((int)(expected), (int)(actual), #actual, __FILE__, __LINE__)

static inline void test_assert_true(bool value, const char *expression, const char *file, int line)
{
    if (!value) {
        fprintf(stderr, "%s:%d: expected true: %s\n", file, line, expression);
        exit(1);
    }
}

static inline void test_assert_false(bool value, const char *expression, const char *file, int line)
{
    if (value) {
        fprintf(stderr, "%s:%d: expected false: %s\n", file, line, expression);
        exit(1);
    }
}

static inline void test_assert_equal_int(int expected, int actual, const char *expression, const char *file, int line)
{
    if (expected != actual) {
        fprintf(stderr, "%s:%d: expected %s == %d, got %d\n", file, line, expression, expected, actual);
        exit(1);
    }
}
