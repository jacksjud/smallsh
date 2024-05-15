#include <stdio.h>
#include <stdlib.h>
#include <check.h> // Include the Check header - errors probably will stop when ran on os1


// Test using: 
// gcc -o test_smallsh test.c smallsh.c -lcheck -lm -lpthread -lrt
// ./test_smallsh

// Include the header file containing the function to test
#include "smallsh.h"

// Define test cases using the START_TEST macro
START_TEST(test_replaceToken) {
    char token[] = "TEST$$TEST";
    char *substring = strstr(token, "$$");
    if (substring != NULL) {
        replaceToken(token, substring);
        ck_assert_str_eq(token, "TEST1234TEST"); // Assert the expected result
    } else {
        ck_abort_msg("Substring not found");
    }
} END_TEST

// Create a test suite
Suite *token_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("Token");
    tc_core = tcase_create("Core");

    // Add the test case to the test suite
    tcase_add_test(tc_core, test_replaceToken);
    suite_add_tcase(s, tc_core);

    return s;
}

// Run the test suite
int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = token_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
