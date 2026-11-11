/*
 * test_main.c - Runs the plugin's logic tests on a PC
 *
 * Part of grblHAL TFT Plugin
 *
 * Build and run with test/run.sh from the repository root. Exits non-zero
 * if a check fails.
 */

#include "harness.h"

int test_checks = 0, test_failures = 0;
const char *test_current = "";

void test_run_suite (const char *suite, const test_case_t *tests, size_t count)
{
    int failures = test_failures;

    for(size_t idx = 0; idx < count; idx++) {
        test_current = tests[idx].name;
        tests[idx].fn();
    }

    printf("%-8s %2u tests  %s\n", suite, (unsigned)count, test_failures == failures ? "ok" : "FAILED");
}

void test_gcode_suite(void);
void test_logic_suite(void);

int main (void)
{
    test_gcode_suite();
    test_logic_suite();

    printf("%d checks, %d failed\n", test_checks, test_failures);

    return test_failures ? 1 : 0;
}
