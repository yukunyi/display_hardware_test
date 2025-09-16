#include <iostream>
#include "display_hardware_test.h"
#ifdef _WIN32
#include <windows.h>
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

int main() {
    auto lang = MonitorTest::detectLanguage();
    if (lang == Language::ZH) {
        std::cout << "=== 显示器硬件测试 (display_hardware_test) ===\n";
        std::cout << "- 目标：10bit色深与高刷新率压力、链路稳定性诊断\n\n";
    } else {
        std::cout << "=== Display Hardware Test (display_hardware_test) ===\n";
        std::cout << "- Goal: 10-bit + high refresh stress; link stability diagnostics\n\n";
    }

    try {
        MonitorTest test;

        if (!test.initialize()) {
            std::cerr << (lang==Language::ZH?"初始化失败":"Initialization failed") << std::endl;
            return -1;
        }

        std::cout << (lang==Language::ZH?"开始运行...":"Running...") << "\n" << std::endl;
        test.run();

        std::cout << (lang==Language::ZH?"\n结束":"\nDone") << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
