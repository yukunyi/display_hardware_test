#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <chrono>
#include <string>
#include <fstream>

class Shader;
class TextRenderer;

enum class TestMode { FIXED_FPS, JITTER_FPS, OSCILLATION_FPS };
enum class Category { STATIC_GROUP = 0, DYNAMIC_GROUP = 1, AUX_GROUP = 2 };
enum class Language { ZH = 0, EN = 1 };

struct TestConfig {
    int minFps = 30;
    int maxFps = 144;
    int targetFps = 120;
    TestMode mode = TestMode::FIXED_FPS;
    bool isPaused = false;
    int colorVariation = 0;
    Category category = Category::DYNAMIC_GROUP;
    int staticMode = 0;
    int dynamicMode = 0;
    int auxMode = 0;
    bool vsyncEnabled = false;
};

class MonitorTest {
private:
    GLFWwindow* window;
    std::unique_ptr<Shader> shader;
    GLuint VAO, VBO;
    TestConfig config;
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    std::chrono::high_resolution_clock::time_point lastFpsReportTime;
    double currentTime;
    int frameCount;
    double currentFps;
    double targetFrameTime;
    double frameTimeMs;
    int windowWidth;
    int windowHeight;
    static const std::string vertexShaderSource;
    static const std::string fragmentShaderSource;
    std::unique_ptr<TextRenderer> textRenderer;
    void renderStatusOverlay();
    std::string chooseFontPath() const;
    Language language = Language::ZH;
    bool minimalOverlay = false;
    bool useDynamicFrameRange = false;
    bool extremeMode = false;

public:
    MonitorTest();
    ~MonitorTest();
    bool initialize();
    void run();
    void cleanup();
    static Language detectLanguage();

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
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void errorCallback(int error, const char* description);
    void printControls() const;
    void printSystemInfo() const;
    const char* tr(const char* zh, const char* en) const;
    std::string onOff(bool v) const;
    void toggleLanguage();
    std::chrono::high_resolution_clock::time_point lastLoopTime;
};
