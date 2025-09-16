#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <chrono>
#include <string>
#include <fstream>

class Shader;
class TextRenderer;

enum class TestMode {
    FIXED_FPS,          // 固定帧率模式
    JITTER_FPS,         // 帧率抖动模式
    OSCILLATION_FPS     // 平滑震荡模式
};

// 顶层分组：静态图样 / 动态压力
enum class Category { STATIC_GROUP = 0, DYNAMIC_GROUP = 1 };

struct TestConfig {
    int minFps = 30;
    int maxFps = 144;
    int targetFps = 120;
    TestMode mode = TestMode::FIXED_FPS;
    bool isPaused = false;
    int colorVariation = 0;  // 动态复杂内容的子变体（0~7）
    Category category = Category::DYNAMIC_GROUP; // 模式组
    int staticMode = 0;      // 静态图样 子模式索引
    int dynamicMode = 0;     // 动态压力 子模式索引
    bool vsyncEnabled = false; // 垂直同步
};

class MonitorTest {
private:
    GLFWwindow* window;
    std::unique_ptr<Shader> shader;
    
    // OpenGL对象
    GLuint VAO, VBO;
    
    // 测试配置
    TestConfig config;
    
    // 时间和帧率相关
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    std::chrono::high_resolution_clock::time_point lastFpsReportTime;
    
    double currentTime;
    int frameCount;
    double currentFps;
    double targetFrameTime;
    double frameTimeMs; // 最近帧时间（指数平滑）
    
    // 窗口信息
    int windowWidth;
    int windowHeight;
    
    // 着色器源码
    static const std::string vertexShaderSource;
    static const std::string fragmentShaderSource;
    
    // 文本渲染
    std::unique_ptr<TextRenderer> textRenderer;
    void renderStatusOverlay();
    std::string chooseFontPath() const;
    
    // 截图/日志/自动轮播
    bool saveScreenshot();
    bool loggingEnabled = false;
    std::ofstream logStream;
    std::string logFilePath;
    bool autoplayEnabled = false;
    double autoplayIntervalSec = 2.0;
    std::chrono::high_resolution_clock::time_point lastModeSwitchTime;
    
public:
    MonitorTest();
    ~MonitorTest();
    
    bool initialize();
    void run();
    void cleanup();
    
private:
    bool initializeWindow();
    bool initializeOpenGL();
    void setupQuad();
    void setupShaders();
    
    void update();
    void render();
    void handleInput();
    
    void updateFrameRate();
    double calculateTargetFps();
    void reportFps();
    
    // 回调函数
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void errorCallback(int error, const char* description);
    
    // 帮助函数
    void printControls() const;
    void printSystemInfo() const;
    
    // 额外的时间记录，用于帧时间显示
    std::chrono::high_resolution_clock::time_point lastLoopTime;
};
