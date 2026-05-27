#pragma once
#include <cstdio>

struct TestContext {
    int failed = 0;
    const char* suite = nullptr;
};

inline bool test_check(TestContext& ctx, bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::fprintf(stderr, "%s: FAIL %s:%d: %s\n", ctx.suite ? ctx.suite : "test", file, line, expr);
        ++ctx.failed;
    }
    return cond;
}

#define CHECK(ctx, cond) test_check((ctx), (cond), #cond, __FILE__, __LINE__)

#define RUN_SUITE(ctx, name, fn) \
    do {                         \
        (ctx).suite = (name);    \
        std::printf("  %s\n", (name)); \
        fn(ctx);                 \
    } while (0)
