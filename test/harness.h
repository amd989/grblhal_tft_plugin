/*
 * harness.h - A tiny test harness for the plugin's pure logic
 *
 * Part of grblHAL TFT Plugin
 *
 * Each test file defines TEST(name) functions and lists them in a suite
 * that test_main.c runs. A failed CHECK prints file:line and keeps going.
 */

#ifndef _TFT_TEST_HARNESS_H_
#define _TFT_TEST_HARNESS_H_

#include <math.h>
#include <stdio.h>
#include <string.h>

extern int test_checks, test_failures;
extern const char *test_current;

#define TEST(name) static void name (void)

#define CHECK(cond) do { \
    test_checks++; \
    if(!(cond)) { \
        test_failures++; \
        printf("  FAIL %s:%d in %s: %s\n", __FILE__, __LINE__, test_current, #cond); \
    } \
} while(0)

#define CHECK_STR(actual, expected) do { \
    const char *a_ = (actual), *e_ = (expected); \
    test_checks++; \
    if(a_ == NULL || e_ == NULL ? a_ != e_ : strcmp(a_, e_) != 0) { \
        test_failures++; \
        printf("  FAIL %s:%d in %s: %s\n    got      \"%s\"\n    expected \"%s\"\n", \
               __FILE__, __LINE__, test_current, #actual, a_ ? a_ : "(null)", e_ ? e_ : "(null)"); \
    } \
} while(0)

#define CHECK_INT(actual, expected) do { \
    long long a_ = (long long)(actual), e_ = (long long)(expected); \
    test_checks++; \
    if(a_ != e_) { \
        test_failures++; \
        printf("  FAIL %s:%d in %s: %s\n    got %lld, expected %lld\n", \
               __FILE__, __LINE__, test_current, #actual, a_, e_); \
    } \
} while(0)

#define CHECK_FLOAT(actual, expected) do { \
    double a_ = (double)(actual), e_ = (double)(expected); \
    test_checks++; \
    if(fabs(a_ - e_) > 0.0005) { \
        test_failures++; \
        printf("  FAIL %s:%d in %s: %s\n    got %.4f, expected %.4f\n", \
               __FILE__, __LINE__, test_current, #actual, a_, e_); \
    } \
} while(0)

typedef struct {
    const char *name;
    void (*fn)(void);
} test_case_t;

#define TEST_CASE(fn) { #fn, fn }

// Run a list of tests, see test_main.c
void test_run_suite(const char *suite, const test_case_t *tests, size_t count);

#define RUN_SUITE(name, list) test_run_suite(name, list, sizeof(list) / sizeof(list[0]))

#endif // _TFT_TEST_HARNESS_H_
