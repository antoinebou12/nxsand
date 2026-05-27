#include "test_harness.hpp"
#include <cstdio>
#include <cstdlib>

void run_gpu_sim_tests(TestContext& ctx);

int main() {
    TestContext ctx{};
    std::printf("gpu unit tests:\n");
    RUN_SUITE(ctx, "gpu_sim", run_gpu_sim_tests);

    if (ctx.failed > 0) {
        std::fprintf(stderr, "gpu unit tests: %d failure(s)\n", ctx.failed);
        return 1;
    }
    std::printf("gpu unit tests: OK\n");
    return 0;
}
