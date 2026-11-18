#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

/// Stand-in for winws/nfqws used by integration tests and manual end-to-end runs.
/// Prints every argument it receives, then idles until the supervisor terminates it.
int main(int argc, char** argv)
{
    std::printf("fake zapret online\n");
    for (int index = 1; index < argc; ++index)
    {
        std::printf("arg%d=%s\n", index, argv[index]);
    }
    std::printf("stdout flush complete\n");
    std::fflush(stdout);

    std::fprintf(stderr, "fake zapret stderr line\n");
    std::fflush(stderr);

    std::this_thread::sleep_for(std::chrono::seconds(120));
    return 0;
}
