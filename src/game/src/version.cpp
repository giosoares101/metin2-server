#include <fmt/core.h>
#include <version.h>

void WriteVersion() {
    fmt::println("Metin2 Game Server version {} (rev. {}, date: {})", __COMMIT_TAG__, __REVISION__, __COMMIT_DATE__);
    fmt::println("OS: {}, target arch: {}, compiler: {}", __OS_NAME__, __CPU_TARGET__, __COMPILER__);
}

