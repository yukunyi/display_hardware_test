#include <iostream>
#include "monitor_test.h"

int main() {
    std::cout << "=== Display Hardware Test ===" << std::endl;
    std::cout << "功能目标：" << std::endl;
    std::cout << "- 模拟高刷新率环境下的真实显示负载" << std::endl;
    std::cout << "- 覆盖10bit色深全色域变化，测试R/G/B各通道0~1023" << std::endl;
    std::cout << "- 构造易触发黑屏的高动态内容流" << std::endl;
    std::cout << "- 复现黑屏场景与系统handshake重协商错误" << std::endl;
    std::cout << "- 测试DSC/VRR/G-Sync等机制失效点\n" << std::endl;
    
    try {
        MonitorTest test;
        
        if (!test.initialize()) {
            std::cerr << "初始化测试失败！" << std::endl;
            return -1;
        }
        
        std::cout << "测试开始运行...\n" << std::endl;
        test.run();
        
        std::cout << "\n测试结束。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "运行时错误: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
