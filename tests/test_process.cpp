#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <deimos/system/process.hpp>

using namespace deimos;

namespace
{

struct CapturedOutput
{
    mutable std::mutex mutex;
    std::string standardOutput;
    std::string errorOutput;

    Process::OutputCallback callback()
    {
        return [this](std::string_view chunk, bool isErrorStream)
        {
            std::lock_guard<std::mutex> lock(mutex);
            (isErrorStream ? errorOutput : standardOutput).append(chunk);
        };
    }

    bool contains(const std::string& needle) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return standardOutput.find(needle) != std::string::npos;
    }

    bool errorContains(const std::string& needle) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return errorOutput.find(needle) != std::string::npos;
    }
};

void waitFor(const CapturedOutput& output, const std::string& needle, int attempts = 100)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        if (output.contains(needle))
        {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
}

} // namespace

TEST_CASE("Process_CapturesOutputAndExitsOnStop", "[process]")
{
    Process process;
    CapturedOutput output;

    ProcessOptions options;
    options.executable = DEIMOS_FAKE_ZAPRET;
    options.arguments = {"--wf-tcp=443", "--dpi-desync=split2"};
    options.captureOutput = true;

    const Status status = process.start(options, output.callback());
    REQUIRE(status.isOk());
    REQUIRE(process.pid() != 0);

    waitFor(output, "fake zapret online");
    REQUIRE(output.contains("fake zapret online"));
    REQUIRE(output.contains("arg1=--wf-tcp=443"));
    REQUIRE(output.contains("arg2=--dpi-desync=split2"));
    REQUIRE(output.errorContains("fake zapret stderr line"));

    REQUIRE_FALSE(process.poll());
    REQUIRE(process.isRunning());

    process.stop(std::chrono::milliseconds{2000});
    REQUIRE(process.poll());
    REQUIRE_FALSE(process.isRunning());
}

TEST_CASE("Process_ReportsMissingExecutable", "[process]")
{
    Process process;
    ProcessOptions options;
    options.executable = "deimos-definitely-not-a-real-binary";
    options.arguments = {"--help"};

    const Status status = process.start(options, nullptr);

    REQUIRE_FALSE(status.isOk());
    REQUIRE(status.error().code() == ErrorCode::io);
}

TEST_CASE("Process_DestructorIsSafeAfterFailedStart", "[process]")
{
    // Regression: a failed start used to leave non-joinable threads behind and the
    // destructor called join() unconditionally, which threw std::system_error.
    bool threw = false;
    try
    {
        Process process;
        ProcessOptions options;
        options.executable = "deimos-definitely-not-a-real-binary";
        (void)process.start(options, nullptr);
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    REQUIRE_FALSE(threw);
}
