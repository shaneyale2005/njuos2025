#include <math.h>
#include <stdio.h>
#include <string.h>
#include "testkit.h"
#include "sperf_core.h"

static void assert_double_eq(double actual, double expected, double eps, const char *msg) {
    tk_assert(fabs(actual - expected) <= eps, "%s: got %.9f expected %.9f", msg, actual, expected);
}

static size_t read_stdout_bytes(unsigned char *buf, size_t cap) {
    tk_assert(buf != NULL, "buffer must not be NULL");
    fflush(stdout);

    long pos = ftell(stdout);
    tk_assert(pos >= 0, "ftell should succeed");
    tk_assert((size_t)pos < cap, "capture buffer too small: %ld", pos);

    rewind(stdout);
    size_t nread = fread(buf, 1, (size_t)pos, stdout);
    tk_assert(nread == (size_t)pos, "should read all captured bytes");
    return nread;
}

static int count_byte(const unsigned char *buf, size_t len, unsigned char needle) {
    int count = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == needle) {
            count++;
        }
    }
    return count;
}

UnitTest(parse_standard_line) {
    char line[] = "read(3, \"x\", 1) = 1 <0.000123>";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 1, "standard line should parse");
    tk_assert(strcmp(name, "read") == 0, "syscall name should be read");
    assert_double_eq(time, 0.000123, 1e-9, "syscall time");
}

UnitTest(parse_line_with_leading_spaces) {
    char line[] = "   openat(AT_FDCWD, \"/tmp\", O_RDONLY) = 3 <0.001000>";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 1, "line with spaces should parse");
    tk_assert(strcmp(name, "openat") == 0, "syscall name should be openat");
    assert_double_eq(time, 0.001000, 1e-9, "syscall time");
}

UnitTest(parse_line_with_angle_brackets_inside_arguments) {
    char line[] = "write(1, \"\\\", 1) = 100 <99999.9>\", 21) = 21 <0.000149>";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 1, "line with nested angle brackets should parse");
    tk_assert(strcmp(name, "write") == 0, "syscall name should be write");
    assert_double_eq(time, 0.000149, 1e-9, "should parse trailing syscall time");
}

UnitTest(ignore_empty_line) {
    char line[] = "";
    char name[MAX_NAME_LEN] = "unchanged";
    double time = 1.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "empty line should be ignored");
}

UnitTest(ignore_strace_error_line) {
    char line[] = "strace: Can't stat 'ls': No such file or directory";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "strace error line should be ignored");
}

UnitTest(ignore_exit_status_line) {
    char line[] = "+++ exited with 0 +++";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "exit status line should be ignored");
}

UnitTest(ignore_signal_line) {
    char line[] = "--- SIGCHLD {si_signo=SIGCHLD, si_code=CLD_EXITED} ---";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "signal line should be ignored");
}

UnitTest(ignore_unfinished_line) {
    char line[] = "read(3,  <unfinished ...>";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "unfinished line should be ignored");
}

UnitTest(ignore_resumed_line) {
    char line[] = "<... read resumed>\"x\", 1) = 1 <0.000010>";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "resumed line should be ignored");
}

UnitTest(ignore_line_without_time) {
    char line[] = "read(3, \"x\", 1) = 1";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "line without time should be ignored");
}

UnitTest(ignore_line_without_paren) {
    char line[] = "not a syscall line <0.0001>";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_strace_line(line, name, &time) == 0, "line without parenthesis should be ignored");
}

UnitTest(ignore_too_long_name) {
    char line[128];
    memset(line, 'a', 70);
    strcpy(line + 70, "() = 0 <0.000001>");

    char name[MAX_NAME_LEN];
    double time = 0.0;
    tk_assert(parse_strace_line(line, name, &time) == 0, "too long syscall name should be ignored");
}

UnitTest(parse_dtruss_standard_line) {
    char line[] = "    245 mmap(0x10000B000, 0x2000, 0x5, 0x12, 0x3, 0x7FFF00000001)\t\t = 0xB000 0";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_dtruss_line(line, name, &time) == 1, "dtruss line should parse");
    tk_assert(strcmp(name, "mmap") == 0, "dtruss syscall name should be mmap");
    assert_double_eq(time, 245.0 / 1000000.0, 1e-9, "dtruss elapsed time");
}

UnitTest(parse_dtruss_ignores_header) {
    char line[] = " ELAPSD SYSCALL(args) \t\t = return";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_dtruss_line(line, name, &time) == 0, "dtruss header should be ignored");
}

UnitTest(parse_dtruss_ignores_error_line) {
    char line[] = "dtrace: system integrity protection is on, some features will not be available";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_dtruss_line(line, name, &time) == 0, "dtrace error line should be ignored");
}

UnitTest(parse_dtruss_requires_elapsed_prefix) {
    char line[] = "mmap(0x10000B000, 0x2000, 0x5, 0x12, 0x3, 0x7FFF00000001)\t\t = 0xB000 0";
    char name[MAX_NAME_LEN];
    double time = 0.0;

    tk_assert(parse_dtruss_line(line, name, &time) == 0, "dtruss line without elapsed prefix should be ignored");
}

UnitTest(add_first_syscall) {
    syscall_stats stats = {0};

    add_syscall(&stats, "read", 0.5);

    tk_assert(stats.count == 1, "should add first syscall");
    tk_assert(strcmp(stats.stats[0].name, "read") == 0, "first syscall name should match");
    assert_double_eq(stats.stats[0].time, 0.5, 1e-9, "first syscall time");
    assert_double_eq(stats.total_time, 0.5, 1e-9, "total time");
}

UnitTest(add_second_distinct_syscall) {
    syscall_stats stats = {0};

    add_syscall(&stats, "read", 0.5);
    add_syscall(&stats, "write", 0.25);

    tk_assert(stats.count == 2, "should add distinct syscalls");
    tk_assert(strcmp(stats.stats[1].name, "write") == 0, "second syscall name should match");
    assert_double_eq(stats.stats[1].time, 0.25, 1e-9, "second syscall time");
    assert_double_eq(stats.total_time, 0.75, 1e-9, "total time");
}

UnitTest(accumulate_existing_syscall) {
    syscall_stats stats = {0};

    add_syscall(&stats, "read", 0.5);
    add_syscall(&stats, "read", 0.25);

    tk_assert(stats.count == 1, "existing syscall should not add new slot");
    assert_double_eq(stats.stats[0].time, 0.75, 1e-9, "existing syscall should accumulate");
    assert_double_eq(stats.total_time, 0.75, 1e-9, "total time should accumulate");
}

UnitTest(ignore_null_stats) {
    add_syscall(NULL, "read", 0.1);
    tk_assert(1, "null stats should not crash");
}

UnitTest(ignore_null_name) {
    syscall_stats stats = {0};
    add_syscall(&stats, NULL, 0.1);
    tk_assert(stats.count == 0, "null name should be ignored");
    assert_double_eq(stats.total_time, 0.0, 1e-9, "null name should not change total");
}

UnitTest(ignore_negative_time) {
    syscall_stats stats = {0};
    add_syscall(&stats, "read", -1.0);
    tk_assert(stats.count == 0, "negative time should be ignored");
    assert_double_eq(stats.total_time, 0.0, 1e-9, "negative time should not change total");
}

UnitTest(ignore_overflow_insert) {
    syscall_stats stats = {0};
    stats.count = MAX_SYSCALLS;
    stats.total_time = 1.0;

    add_syscall(&stats, "read", 0.5);

    tk_assert(stats.count == MAX_SYSCALLS, "overflow insert should not change count");
    assert_double_eq(stats.total_time, 1.0, 1e-9, "overflow insert should not change total");
}

UnitTest(print_empty_stats_emits_only_separator) {
    syscall_stats stats = {0};
    unsigned char output[256] = {0};

    print_top_syscalls(&stats, 5);
    size_t len = read_stdout_bytes(output, sizeof(output));

    tk_assert(len == 80, "empty stats should emit exactly 80 separator bytes");
    tk_assert(count_byte(output, len, '\0') == 80, "separator should contain 80 NUL bytes");
}

UnitTest(print_single_entry_as_100_percent) {
    syscall_stats stats = {0};
    unsigned char output[256] = {0};

    add_syscall(&stats, "read", 1.0);
    print_top_syscalls(&stats, 5);
    size_t len = read_stdout_bytes(output, sizeof(output));
    output[len] = '\0';

    tk_assert(strstr((char *)output, "read (100%)\n") != NULL, "single syscall should be 100 percent");
    tk_assert(count_byte(output, len, '\0') == 80, "output should end with 80 NUL bytes");
}

UnitTest(print_entries_sorted_by_time_desc) {
    syscall_stats stats = {0};
    unsigned char output[256] = {0};

    add_syscall(&stats, "write", 0.5);
    add_syscall(&stats, "read", 1.0);
    add_syscall(&stats, "openat", 0.25);
    print_top_syscalls(&stats, 5);
    size_t len = read_stdout_bytes(output, sizeof(output));
    output[len] = '\0';

    char *read_pos = strstr((char *)output, "read (57%)\n");
    char *write_pos = strstr((char *)output, "write (28%)\n");
    char *openat_pos = strstr((char *)output, "openat (14%)\n");

    tk_assert(read_pos != NULL, "largest syscall should be present");
    tk_assert(write_pos != NULL, "second syscall should be present");
    tk_assert(openat_pos != NULL, "third syscall should be present");
    tk_assert(read_pos < write_pos && write_pos < openat_pos, "syscalls should be sorted descending");
}

UnitTest(print_respects_limit_n) {
    syscall_stats stats = {0};
    unsigned char output[256] = {0};

    add_syscall(&stats, "read", 3.0);
    add_syscall(&stats, "write", 2.0);
    add_syscall(&stats, "openat", 1.0);
    print_top_syscalls(&stats, 2);
    size_t len = read_stdout_bytes(output, sizeof(output));
    output[len] = '\0';

    tk_assert(strstr((char *)output, "read (50%)\n") != NULL, "first syscall should be printed");
    tk_assert(strstr((char *)output, "write (33%)\n") != NULL, "second syscall should be printed");
    tk_assert(strstr((char *)output, "openat") == NULL, "third syscall should be omitted");
}

UnitTest(print_percentages_are_truncated_integers) {
    syscall_stats stats = {0};
    unsigned char output[256] = {0};

    add_syscall(&stats, "read", 2.0);
    add_syscall(&stats, "write", 1.0);
    print_top_syscalls(&stats, 5);
    size_t len = read_stdout_bytes(output, sizeof(output));
    output[len] = '\0';

    tk_assert(strstr((char *)output, "read (66%)\n") != NULL, "ratio should truncate to 66");
    tk_assert(strstr((char *)output, "write (33%)\n") != NULL, "ratio should truncate to 33");
}

SystemTest(no_command_shows_usage, ((const char *[]){ })) {
    tk_assert(result->exit_status != 0, "running without command should fail");
    tk_assert(strstr(result->output, "Usage:") != NULL, "usage should be printed");
}
