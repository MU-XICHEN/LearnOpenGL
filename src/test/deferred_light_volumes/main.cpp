#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/camera.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/model.h>
#include <learnopengl/shader.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "geometry.h"

namespace
{
constexpr float linearAttenuation = 0.7f, quadraticAttenuation = 1.8f;
constexpr float nearPlane = 0.1f;
Camera camera(glm::vec3(0, 3, 12), glm::vec3(0, 1, 0), -90, -12);
bool captureMouse = true, firstMouse = true;
double mouseX = 0, mouseY = 0;

std::string asset(const char* name)
{
    std::string local = std::string("deferred_light_volumes/") + name;
    if (std::ifstream(local).good()) return local;
    return FileSystem::getPath(std::string("src/test/deferred_light_volumes/assets/") + name);
}

Shader loadShader(const char* vs, const char* fs)
{
    Shader shader(asset(vs).c_str(), asset(fs).c_str());
    GLint linked = 0;
    glGetProgramiv(shader.ID, GL_LINK_STATUS, &linked);
    if (!linked) throw std::runtime_error("Shader link failed");
    return shader;
}

struct Light
{
    glm::vec3 position, color;
    float radius;
};

float radiusFor(glm::vec3 color)
{
    // 沿用 8.2 的阈值：max(color)/(1 + linear*r + quadratic*r*r) = 5/256。
    // 解二次方程的正根。无限衰减被近似为有限半径，并不是光强真的变成 0。
    float brightness = std::max({color.r, color.g, color.b});
    return (-linearAttenuation + std::sqrt(linearAttenuation * linearAttenuation -
            4 * quadraticAttenuation * (1 - (256.0f / 5.0f) * brightness))) /
           (2 * quadraticAttenuation);
}

struct Sphere
{
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;
    float conservativeScale = 1;

    Sphere()
    {
        constexpr int rings = 16, segments = 32;
        constexpr float pi = 3.14159265358979323846f;
        std::vector<glm::vec3> triangles;
        float minimumPlaneDistance = 1;
        auto point = [&](int ring, int segment) {
            if (ring == 0) return glm::vec3(0, 1, 0);
            if (ring == rings) return glm::vec3(0, -1, 0);
            float theta = pi * ring / rings, phi = 2 * pi * segment / segments;
            return glm::vec3(std::sin(theta) * std::cos(phi), std::cos(theta),
                             std::sin(theta) * std::sin(phi));
        };
        auto triangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            glm::vec3 normal = glm::cross(b - a, c - a);
            if (glm::length(normal) < 0.000001f) return; // 跳过极点退化三角形。
            if (glm::dot(normal, a) < 0) { std::swap(b, c); normal = -normal; }
            // 确保从球外看为 CCW；这样剔除 GL_FRONT 才会留下球的背面。
            minimumPlaneDistance = std::min(minimumPlaneDistance,
                                            glm::dot(glm::normalize(normal), a));
            triangles.insert(triangles.end(), {a, b, c});
        };
        for (int r = 0; r < rings; ++r)
            for (int s = 0; s < segments; ++s)
            {
                triangle(point(r, s), point(r + 1, s), point(r + 1, s + 1));
                triangle(point(r, s), point(r + 1, s + 1), point(r, s + 1));
            }
        // 顶点在单位球上时，三角形平面位于球内，可能漏掉球边缘的有效像素。
        // 用最小面平面距离算出外扩比例，使整个多面体包住数学球，稍留数值余量。
        conservativeScale = 1.001f / minimumPlaneDistance;
        vertexCount = static_cast<GLsizei>(triangles.size());
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, triangles.size() * sizeof(glm::vec3), triangles.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
        glBindVertexArray(0);
    }
    void draw() const
    {
        glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES, 0, vertexCount); glBindVertexArray(0);
    }
    ~Sphere() { glDeleteBuffers(1, &vbo); glDeleteVertexArrays(1, &vao); }
};

struct Targets
{
    GLuint gBuffer = 0, position = 0, normal = 0, material = 0, depth = 0;
    GLuint lightingFbo = 0, lighting = 0;
    int width = 0, height = 0;

    void destroy()
    {
        GLuint textures[] = {position, normal, material, lighting};
        glDeleteTextures(4, textures);
        glDeleteRenderbuffers(1, &depth);
        glDeleteFramebuffers(1, &gBuffer); glDeleteFramebuffers(1, &lightingFbo);
        gBuffer = lightingFbo = position = normal = material = lighting = depth = 0;
    }
    GLuint texture(GLint format)
    {
        GLuint id;
        glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return id;
    }
    void check()
    {
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Framebuffer incomplete");
    }
    void resize(int w, int h)
    {
        if (w == width && h == height) return;
        destroy(); width = w; height = h;
        glGenFramebuffers(1, &gBuffer); glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        position = texture(GL_RGBA16F); normal = texture(GL_RGBA16F); material = texture(GL_RGBA8);
        GLuint attachments[] = {position, normal, material};
        for (int i = 0; i < 3; ++i)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, attachments[i], 0);
        GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, buffers);
        glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        check();
        glGenFramebuffers(1, &lightingFbo); glBindFramebuffer(GL_FRAMEBUFFER, lightingFbo);
        lighting = texture(GL_RGBA16F);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lighting, 0);
        // 与 G-buffer 共享深度附件；用于最后的灯泡/线框遮挡，无需向默认 FBO 拷贝不同格式的深度。
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        check(); glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    ~Targets() { destroy(); }
};

struct Renderer
{
    Shader geometry = loadShader("geometry.vs", "geometry.fs");
    Shader volume = loadShader("volume.vs", "light.fs");
    Shader fullscreenLight = loadShader("fullscreen.vs", "light.fs");
    Shader ambient = loadShader("fullscreen.vs", "ambient.fs");
    Shader solid = loadShader("solid.vs", "solid.fs");
    Shader present = loadShader("fullscreen.vs", "present.fs");
    Model backpack{FileSystem::getPath("resources/objects/backpack/backpack.obj")};
    Sphere sphere;
    Targets targets;
    std::vector<Light> lights;
    GLuint timer = 0;
    bool timerPending = false;
    double lightingMs = 0;
    int fallbackCount = 0;

    Renderer()
    {
        if (backpack.meshes.empty() || backpack.textures_loaded.empty())
            throw std::runtime_error("Backpack model/textures missing");
        std::srand(13);
        for (int i = 0; i < 512; ++i)
        {
            auto random = [] { return (std::rand() % 100) / 100.0f; };
            float extent = i < 32 ? 3.0f : 9.0f;
            glm::vec3 pos;
            pos.x = random() * 2 * extent - extent;
            pos.y = random() * 6 - 4;
            pos.z = random() * 2 * extent - extent;
            glm::vec3 color;
            color.r = random() * 0.5f + 0.5f;
            color.g = random() * 0.5f + 0.5f;
            color.b = random() * 0.5f + 0.5f;
            // 第一盏灯作为进入体积实验的位置，其余灯沿用 8.2 的随机分布思想。
            if (i == 0) { pos = glm::vec3(0, 1.2f, 2); color = glm::vec3(1, 0.65f, 0.3f); }
            lights.push_back({pos, color, radiusFor(color)});
        }
        for (Shader* shader : {&volume, &fullscreenLight, &ambient})
        {
            shader->use(); shader->setInt("gPosition", 0); shader->setInt("gNormal", 1);
            shader->setInt("gAlbedoSpec", 2);
        }
        present.use(); present.setInt("hdrLighting", 0);
        glGenQueries(1, &timer);
    }
    ~Renderer()
    {
        glDeleteQueries(1, &timer);
        for (Shader* shader : {&geometry, &volume, &fullscreenLight, &ambient, &solid, &present})
            glDeleteProgram(shader->ID);
        glDeleteVertexArrays(1, &cubeVAO); glDeleteBuffers(1, &cubeVBO);
        glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    }
    glm::mat4 lightModel(const Light& light, bool conservative) const
    {
        float radius = light.radius * (conservative ? sphere.conservativeScale : 1.0f);
        return glm::scale(glm::translate(glm::mat4(1), light.position), glm::vec3(radius));
    }
    void geometryPass(const glm::mat4& projection, const glm::mat4& view)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, targets.gBuffer);
        glViewport(0, 0, targets.width, targets.height);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        geometry.use(); geometry.setMat4("projection", projection); geometry.setMat4("view", view);
        geometry.setBool("textured", true);
        // 8.2 的九个背包：几何阶段每个物体只画一次，与光源数量无关。
        for (int x = -3; x <= 3; x += 3)
            for (int z = -3; z <= 3; z += 3)
            {
                glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(x, -0.5f, z));
                geometry.setMat4("model", glm::scale(model, glm::vec3(0.25f)));
                backpack.Draw(geometry);
            }
        // 地板增加可观察的受光表面，仍写入同一套 G-buffer。
        geometry.setBool("textured", false);
        geometry.setMat4("model", glm::scale(glm::translate(glm::mat4(1), glm::vec3(0, -2.5f, 0)),
                                              glm::vec3(12, 0.1f, 12)));
        renderCube();
    }
    void lightingPass(const glm::mat4& projection, const glm::mat4& view,
                      int count, bool useVolumes, float farPlane, bool measure = false)
    {
        bool record = false;
        if (measure)
        {
            if (timerPending)
            {
                GLint available = 0;
                glGetQueryObjectiv(timer, GL_QUERY_RESULT_AVAILABLE, &available);
                if (available)
                {
                    GLuint64 ns = 0; glGetQueryObjectui64v(timer, GL_QUERY_RESULT, &ns);
                    lightingMs = ns / 1000000.0; timerPending = false;
                }
            }
            record = !timerPending;
            if (record) glBeginQuery(GL_TIME_ELAPSED, timer);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, targets.lightingFbo);
        // 背面球的深度在场景表面之后，用普通 GL_LESS 会错误拒绝有效光照。
        // 此示范关闭深度测试，依靠 G-buffer 的位置 + 半径判断保证正确。
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, targets.position);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, targets.normal);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, targets.material);
        ambient.use(); renderQuad(); // 全屏覆盖 HDR 颜色；保留共享的场景深度。

        // 新颜色 = 本盏灯的贡献 * 1 + 已累积颜色 * 1。
        glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ONE, GL_ONE);
        glFrontFace(GL_CCW); glCullFace(GL_FRONT);
        for (Shader* shader : {&volume, &fullscreenLight})
        {
            shader->use(); shader->setVec3("viewPos", camera.Position);
            shader->setFloat("linear", linearAttenuation);
            shader->setFloat("quadratic", quadraticAttenuation);
        }
        volume.use(); volume.setMat4("projection", projection); volume.setMat4("view", view);
        fallbackCount = 0;
        for (int i = 0; i < count; ++i)
        {
            const Light& light = lights[i];
            float viewDepth = -(view * glm::vec4(light.position, 1)).z;
            // 球的背面被远平面裁掉时会漏光。这里保守退回单灯全屏绘制。
            // 相机进入球体通常不需要退回：剔除正面后，从球内仍看得到背面。
            bool clippedAtFar = viewDepth + light.radius * sphere.conservativeScale >= farPlane;
            bool drawSphere = useVolumes && !clippedAtFar;
            Shader& shader = drawSphere ? volume : fullscreenLight;
            shader.use(); shader.setVec3("lightPosition", light.position);
            shader.setVec3("lightColor", light.color); shader.setFloat("lightRadius", light.radius);
            if (drawSphere)
            {
                // 只渲染背面，避免前/后两层把同一盏灯加两次；球内外统一使用此规则。
                glEnable(GL_CULL_FACE); shader.setMat4("model", lightModel(light, true)); sphere.draw();
            }
            else
            {
                glDisable(GL_CULL_FACE); renderQuad();
                if (useVolumes) ++fallbackCount;
            }
        }
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glCullFace(GL_BACK);
        glDepthMask(GL_TRUE);
        if (record) { glEndQuery(GL_TIME_ELAPSED); timerPending = true; }
    }
    void overlays(const glm::mat4& projection, const glm::mat4& view, int count, bool wire)
    {
        // 在同一个 HDR FBO 中画灯泡，使用共享的场景深度产生正确遮挡。
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        solid.use(); solid.setMat4("projection", projection); solid.setMat4("view", view);
        for (int i = 0; i < count; ++i)
        {
            solid.setVec3("lightColor", lights[i].color * 2.0f);
            solid.setMat4("model", glm::scale(glm::translate(glm::mat4(1), lights[i].position), glm::vec3(0.06f)));
            renderCube();
        }
        if (wire)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            // 只显示前八盏灯的数学半径，避免 512 个线框遮住整个场景。
            for (int i = 0; i < std::min(count, 8); ++i)
            {
                solid.setVec3("lightColor", lights[i].color);
                solid.setMat4("model", lightModel(lights[i], false)); sphere.draw();
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        glDepthMask(GL_TRUE);
    }
    void display(float exposure)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0); glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, targets.lighting);
        present.use(); present.setFloat("exposure", exposure); renderQuad();
    }
    std::vector<float> readLighting()
    {
        std::vector<float> pixels(static_cast<size_t>(targets.width) * targets.height * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, targets.lightingFbo);
        glReadPixels(0, 0, targets.width, targets.height, GL_RGBA, GL_FLOAT, pixels.data());
        return pixels;
    }
};

void mouseCallback(GLFWwindow*, double x, double y)
{
    if (!captureMouse) return;
    if (firstMouse) { mouseX = x; mouseY = y; firstMouse = false; }
    camera.ProcessMouseMovement(static_cast<float>(x - mouseX), static_cast<float>(mouseY - y));
    mouseX = x; mouseY = y;
}

void scrollCallback(GLFWwindow*, double, double y)
{
    if (captureMouse) camera.ProcessMouseScroll(static_cast<float>(y));
}

// 实际创建 OpenGL 上下文、加载模型并比较 HDR 像素，不是只检查程序能启动。
void verify(Renderer& renderer)
{
    struct Case { const char* name; glm::vec3 eye; int count, width, height; float farPlane; };
    const auto& light = renderer.lights[0];
    std::vector<Case> cases = {
        {"outside_32", {0,3,12}, 32, 320,240,100},
        {"outside_128", {0,3,12}, 128,320,240,100},
        {"outside_512", {0,3,12}, 512,320,240,100},
        {"inside_one_light", light.position, 1,320,240,100},
        {"inside_128", light.position, 128,320,240,100},
        {"near_boundary_outside", light.position + glm::vec3(0,0,light.radius + 0.02f), 1,320,240,100},
        {"near_boundary_inside", light.position + glm::vec3(0,0,light.radius - 0.02f), 1,320,240,100},
        {"far_plane_fallback", {0,3,12}, 128,320,240,10},
        {"resized_512", {0,3,12}, 512,640,360,100}
    };
    for (const auto& test : cases)
    {
        camera = Camera(test.eye, glm::vec3(0,1,0), -90, -12);
        renderer.targets.resize(test.width, test.height);
        auto projection = glm::perspective(glm::radians(camera.Zoom),
                           float(test.width) / test.height, nearPlane, test.farPlane);
        auto view = camera.GetViewMatrix();
        renderer.geometryPass(projection, view);
        renderer.lightingPass(projection, view, 0, false, test.farPlane);
        auto ambientOnly = renderer.readLighting();
        renderer.lightingPass(projection, view, test.count, false, test.farPlane);
        auto reference = renderer.readLighting();
        renderer.lightingPass(projection, view, test.count, true, test.farPlane);
        auto actual = renderer.readLighting();
        float maximumError = 0;
        double lightEnergy = 0;
        size_t different = 0, illuminatedChannels = 0;
        for (size_t i = 0; i < actual.size(); ++i)
        {
            if (i % 4 == 3) continue;
            if (!std::isfinite(actual[i]) || !std::isfinite(reference[i]))
                throw std::runtime_error("Non-finite lighting result");
            float error = std::abs(actual[i] - reference[i]);
            maximumError = std::max(maximumError, error);
            if (error > 0.002f) ++different;
            float contribution = reference[i] - ambientOnly[i];
            lightEnergy += contribution;
            if (contribution > 0.0001f) ++illuminatedChannels;
        }
        // 非空、确有受光像素，避免两个模式都只画背景却假通过。
        if (illuminatedChannels < 100 || lightEnergy <= 0.1)
            throw std::runtime_error("No illuminated scene in verification case");
        std::cout << test.name << ": max HDR error=" << maximumError << ", mismatched channels="
                  << different << ", lit channels=" << illuminatedChannels
                  << ", far fallbacks=" << renderer.fallbackCount << '\n';
        if (different != 0) throw std::runtime_error("Volume/fullscreen comparison failed");
        renderer.overlays(projection, view, test.count, true);
        renderer.display(0.7f); // 也验证灯泡、线框和最终色调映射通路。
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error during verification");
    }
    std::cout << "VERIFY PASSED: shaders, FBOs, 32/128/512 lights, inside/outside, near/far planes, resize.\n";
}
} // namespace

int main(int argc, char** argv)
{
    bool verification = argc > 1 && std::string(argv[1]) == "--verify";
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    if (verification) glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Deferred light volumes", nullptr, nullptr);
    if (!window) { glfwTerminate(); std::cerr << "Window creation failed\n"; return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    { glfwDestroyWindow(window); glfwTerminate(); std::cerr << "GLAD init failed\n"; return 1; }
    glfwSwapInterval(0); // 不用垂直同步限制帧率，便于观察 GPU 光照阶段耗时。
    int result = 0;
    try
    {
        std::cout << "GPU: " << glGetString(GL_RENDERER) << '\n';
        stbi_set_flip_vertically_on_load(true);
        Renderer renderer;
        if (verification) verify(renderer);
        else
        {
            glfwSetCursorPosCallback(window, mouseCallback); glfwSetScrollCallback(window, scrollCallback);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            bool volumes = true, wire = false;
            int count = 128;
            float exposure = 0.7f;
            bool previous[GLFW_KEY_LAST + 1] = {};
            auto pressed = [&](int key) {
                bool down = glfwGetKey(window, key) == GLFW_PRESS;
                bool edge = down && !previous[key]; previous[key] = down; return edge;
            };
            double last = glfwGetTime(), titleTime = last;
            std::cout << "F1: spheres, F2: fullscreen per light; 1/2/3: 32/128/512 lights; V: first 8 wire volumes\n"
                      << "WASD/mouse/wheel: camera; C: enter first light; R: reset; Tab: release mouse; Q/E: exposure; Esc: exit\n";
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
                double now = glfwGetTime(); float dt = std::min(float(now - last), 0.1f); last = now;
                if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, dt);
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, dt);
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, dt);
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, dt);
                if (pressed(GLFW_KEY_F1)) volumes = true;
                if (pressed(GLFW_KEY_F2)) volumes = false;
                if (pressed(GLFW_KEY_1)) count = 32;
                if (pressed(GLFW_KEY_2)) count = 128;
                if (pressed(GLFW_KEY_3)) count = 512;
                if (pressed(GLFW_KEY_V)) wire = !wire;
                if (pressed(GLFW_KEY_C)) camera = Camera(renderer.lights[0].position, glm::vec3(0,1,0), -90, -12);
                if (pressed(GLFW_KEY_R)) camera = Camera(glm::vec3(0,3,12), glm::vec3(0,1,0), -90, -12);
                if (pressed(GLFW_KEY_TAB))
                {
                    captureMouse = !captureMouse; firstMouse = true;
                    glfwSetInputMode(window, GLFW_CURSOR, captureMouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                }
                if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) exposure *= std::exp(-dt);
                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) exposure *= std::exp(dt);
                exposure = std::clamp(exposure, 0.03f, 5.0f);
                int width, height; glfwGetFramebufferSize(window, &width, &height);
                // 最小化时等待事件；恢复后重建所有附件，确保 viewport/G-buffer 坐标始终一致。
                if (width <= 0 || height <= 0) { glfwWaitEvents(); continue; }
                renderer.targets.resize(width, height);
                auto projection = glm::perspective(glm::radians(camera.Zoom), float(width) / height, nearPlane, 100.0f);
                auto view = camera.GetViewMatrix();
                renderer.geometryPass(projection, view);
                renderer.lightingPass(projection, view, count, volumes, 100.0f, true);
                renderer.overlays(projection, view, count, wire); renderer.display(exposure);
                glfwSwapBuffers(window);
                if (now - titleTime > 0.5)
                {
                    int inside = 0;
                    for (int i = 0; i < count; ++i)
                        if (glm::length(camera.Position - renderer.lights[i].position) < renderer.lights[i].radius) ++inside;
                    std::ostringstream title;
                    title << (volumes ? "Sphere volumes" : "Fullscreen per light") << " | lights=" << count
                          << " | lighting GPU=" << std::fixed << std::setprecision(2) << renderer.lightingMs
                          << " ms | inside=" << inside << " | far fallback=" << renderer.fallbackCount
                          << " | exposure=" << exposure << " | F1/F2 mode, 1/2/3 lights, V wire, C enter, Tab mouse";
                    glfwSetWindowTitle(window, title.str().c_str()); titleTime = now;
                }
            }
        }
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
    glfwDestroyWindow(window); glfwTerminate();
    return result;
}
