#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/camera.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "geometry.h"
#include "hud.h"

namespace
{
constexpr int captureSize = 512;
const glm::vec3 probePosition(0.0f);
const char* faceNames[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
const glm::vec3 faceDirections[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
// These up vectors match OpenGL cubemap sampling, including the two poles.
const glm::vec3 faceUps[] = {{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};

std::string asset(const char* name)
{
    std::string local = std::string("dynamic_environment_mapping/") + name;
    if (std::ifstream(local).good()) return local;
    return FileSystem::getPath(std::string("src/test/dynamic_environment_mapping/assets/") + name);
}

Shader loadShader(const char* vs, const char* fs)
{
    Shader shader(asset(vs).c_str(), asset(fs).c_str());
    GLint ok = 0;
    glGetProgramiv(shader.ID, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(shader.ID); throw std::runtime_error("Shader link failed"); }
    return shader;
}

void cubemapParameters()
{
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void loadSkybox(GLuint& texture)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    const char* names[] = {"right.jpg","left.jpg","top.jpg","bottom.jpg","front.jpg","back.jpg"};
    stbi_set_flip_vertically_on_load(false);
    int faceSize = 0;
    for (int face = 0; face < 6; ++face)
    {
        std::string path = FileSystem::getPath(std::string("resources/textures/skybox/") + names[face]);
        int w, h, channels;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (!data) throw std::runtime_error("Cannot load skybox: " + path);
        bool valid = w == h && (face == 0 || w == faceSize);
        if (valid) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8,
                               w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        if (!valid) throw std::runtime_error("Cubemap faces must be equally sized squares");
        faceSize = w;
    }
    cubemapParameters();
}

struct Renderer
{
    Shader scene = loadShader("object.vs", "scene.fs");
    Shader environment = loadShader("object.vs", "environment.fs");
    Shader sky = loadShader("skybox.vs", "skybox.fs");
    Shader preview = loadShader("preview.vs", "preview.fs");
    Shader text = loadShader("hud.vs", "hud.fs");
    Mesh cube, sphere;
    Hud hud;
    GLuint staticMap = 0, dynamicMap = 0, captureFbo = 0, depth = 0;
    int captureCount = 0;

    Renderer()
    {
        createCube(cube); createSphere(sphere);
        loadSkybox(staticMap);
        glGenTextures(1, &dynamicMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, dynamicMap);
        for (int face = 0; face < 6; ++face)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8,
                         captureSize, captureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        cubemapParameters();

        glGenFramebuffers(1, &captureFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, captureSize, captureSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
        for (int face = 0; face < 6; ++face)
        {
            attachFace(face);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error(std::string("Incomplete capture FBO: ") + faceNames[face]);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        sky.use(); sky.setInt("skybox", 0);
        preview.use(); preview.setInt("capturedMap", 0);
        environment.use(); environment.setInt("environmentMap", 0); environment.setFloat("ior", 1.52f);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }
    ~Renderer()
    {
        for (GLuint id : {scene.ID, environment.ID, sky.ID, preview.ID, text.ID}) glDeleteProgram(id);
        glDeleteTextures(1, &staticMap); glDeleteTextures(1, &dynamicMap);
        glDeleteRenderbuffers(1, &depth); glDeleteFramebuffers(1, &captureFbo);
    }
    void attachFace(int face)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, dynamicMap, 0);
    }
    void drawSky(const glm::mat4& view, const glm::mat4& projection)
    {
        glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        sky.use(); sky.setMat4("view", view); sky.setMat4("projection", projection);
        glActiveTexture(GL_TEXTURE0);
        // Always sample the static sky here, never the map currently being captured.
        glBindTexture(GL_TEXTURE_CUBE_MAP, staticMap);
        cube.draw();
        glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
    }
    void drawScene(float time, const glm::mat4& view, const glm::mat4& projection)
    {
        scene.use(); scene.setMat4("view", view); scene.setMat4("projection", projection);
        scene.setBool("floorPattern", true);
        glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(0, -1.35f, 0));
        model = glm::scale(model, glm::vec3(16, 0.2f, 16));
        scene.setMat4("model", model); scene.setVec3("baseColor", 1, 1, 1);
        cube.draw();
        scene.setBool("floorPattern", false);
        const glm::vec3 colors[] = {{1,.18f,.07f},{.05f,.9f,.95f},{1,.75f,.05f},{.65f,.13f,.9f}};
        for (int i = 0; i < 4; ++i)
        {
            float angle = time * 0.65f + i * 1.57079633f;
            glm::vec3 position(3.2f * std::cos(angle),
                               0.35f + 0.65f * std::sin(time + i), 3.2f * std::sin(angle));
            model = glm::translate(glm::mat4(1), position);
            model = glm::rotate(model, time * .55f + i, glm::normalize(glm::vec3(1,2,0.5f)));
            model = glm::scale(model, glm::vec3(.95f));
            scene.setMat4("model", model); scene.setVec3("baseColor", colors[i]); cube.draw();
        }
        // Tall stationary landmarks give the reflection a stable reference.
        for (int i = 0; i < 4; ++i)
        {
            float a = 0.78539816f + i * 1.57079633f;
            model = glm::translate(glm::mat4(1), glm::vec3(5.5f*std::cos(a), .75f, 5.5f*std::sin(a)));
            model = glm::scale(model, glm::vec3(.45f,4,.45f));
            scene.setMat4("model", model); scene.setVec3("baseColor", .85f,.88f,.93f); cube.draw();
        }
        drawSky(view, projection);
    }

    void captureEnvironment(float time)
    {
        // PASS 1: six 90-degree views, all from the same probe and simulation time.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
        glViewport(0, 0, captureSize, captureSize);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
        glDisable(GL_SCISSOR_TEST);
        glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
        for (int face = 0; face < 6; ++face)
        {
            attachFace(face);
            glClearColor(.015f,.02f,.03f,1);
            // A shared depth buffer must be cleared separately for EACH face.
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glm::mat4 view = glm::lookAt(probePosition, probePosition + faceDirections[face], faceUps[face]);
            // The central environment-mapped sphere is intentionally excluded.
            drawScene(time, view, projection);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ++captureCount;
    }

    void drawMain(int width, int height, Camera& camera, float time, int mode, bool useDynamic)
    {
        // PASS 2: restore window-sized viewport; capture viewport is not FBO-local state.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0,0,width,height);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(.015f,.02f,.03f,1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), float(width)/height, .1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        drawScene(time, view, projection);

        environment.use();
        environment.setMat4("projection", projection); environment.setMat4("view", view);
        environment.setMat4("model", glm::translate(glm::mat4(1), probePosition));
        environment.setVec3("cameraPos", camera.Position); environment.setInt("mode", mode);
        glActiveTexture(GL_TEXTURE0);
        // All six faces are finished and the capture FBO is no longer the draw target.
        glBindTexture(GL_TEXTURE_CUBE_MAP, useDynamic ? dynamicMap : staticMap);
        sphere.draw();
    }

    void drawHud(int width, int height, int mode, bool dynamic, bool frozen, bool paused, bool faces)
    {
        glDisable(GL_DEPTH_TEST);
        auto background = [](int x, int y, int w, int h) {
            glEnable(GL_SCISSOR_TEST); glScissor(x,y,w,h);
            glClearColor(.025f,.038f,.06f,1); glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
        };
        background(0,height-92,width,92);
        const char* modes[] = {"REFLECTION", "REFRACTION", "FRESNEL MIX"};
        hud.text("DYNAMIC ENVIRONMENT MAPPING", 18, float(height-24), 2);
        hud.text(std::string(modes[mode]) + " | " + (dynamic ? "DYNAMIC CUBEMAP" : "STATIC SKY ONLY")
                 + " | " + (frozen ? "CAPTURE FROZEN" : "CAPTURE LIVE")
                 + (paused ? " | PAUSED" : ""), 18, float(height-47), 1.5f);
        hud.text("1/2/3 MATERIAL | E STATIC/DYNAMIC | SPACE FREEZE MAP | P PAUSE | V FACES",
                 18, float(height-67), 1.5f);
        hud.text("WASD MOVE | RIGHT DRAG LOOK | WHEEL ZOOM | R RESET | ESC EXIT",
                 18, float(height-84), 1.5f);
        if (faces && width > 120 && height > 250)
        {
            int tile = std::min(160, (width-42)/6);
            int start = (width - (tile+6)*6) / 2;
            background(0,0,width,tile+52);
            preview.use(); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, dynamicMap);
            glBindVertexArray(cube.vao);
            for (int i = 0; i < 6; ++i)
            {
                int x = start + i*(tile+6);
                glViewport(x,12,tile,tile);
                preview.setInt("face", i); glDrawArrays(GL_TRIANGLES,0,3);
                hud.text(faceNames[i], float(x+4), float(tile+18), 2);
            }
            hud.text("CAPTURED FACES 512X512 - THUMBNAILS", float(start), float(tile+38), 1);
        }
        glViewport(0,0,width,height);
        hud.draw(text,width,height);
    }

    std::vector<unsigned char> readMap()
    {
        std::vector<unsigned char> bytes(6*captureSize*captureSize*4);
        glBindTexture(GL_TEXTURE_CUBE_MAP, dynamicMap);
        for (int i = 0; i < 6; ++i)
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,GL_RGBA,GL_UNSIGNED_BYTE,
                          bytes.data()+i*captureSize*captureSize*4);
        return bytes;
    }
};

struct Input
{
    Camera camera = Camera(glm::vec3(0,2.0f,8.5f), glm::vec3(0,1,0), -90.0f, -13.0f);
    bool first = true;
    double x = 0, y = 0;
};
void mouse(GLFWwindow* window, double x, double y)
{
    auto& input = *static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) input.first = true;
    else
    {
        if (!input.first) input.camera.ProcessMouseMovement(float(x-input.x), float(input.y-y));
        input.first = false;
    }
    input.x = x; input.y = y;
}
std::vector<unsigned char> readWindow(int width, int height)
{
    std::vector<unsigned char> pixels(width*height*4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0); glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    return pixels;
}
size_t changedPixels(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b)
{
    size_t changes = 0;
    for (size_t i = 0; i < a.size(); i += 4)
        if (std::abs(int(a[i])-b[i]) + std::abs(int(a[i+1])-b[i+1]) + std::abs(int(a[i+2])-b[i+2]) > 18)
            ++changes;
    return changes;
}
void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error("VERIFY FAILED: " + message);
    std::cout << "PASS " << message << '\n';
}
void savePreview(const std::vector<unsigned char>& rgba, int width, int height)
{
    std::ofstream out("dynamic_environment_mapping/preview.ppm", std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write preview.ppm; run from the executable directory");
    out << "P6\n" << width << " " << height << "\n255\n";
    for (int y = height-1; y >= 0; --y)
        for (int x = 0; x < width; ++x)
            out.write(reinterpret_cast<const char*>(rgba.data()+(y*width+x)*4),3);
}

void verify(Renderer& renderer, GLFWwindow* window, Input& input)
{
    renderer.captureEnvironment(0);
    auto first = renderer.readMap();
    const size_t faceBytes = captureSize*captureSize*4;
    bool distinct = true;
    for (int i = 0; i < 6; ++i)
    {
        auto begin = first.begin()+i*faceBytes;
        bool varied = false;
        for (size_t p = 4; p < faceBytes; p += 4)
            if (begin[p] != begin[0] || begin[p+1] != begin[1] || begin[p+2] != begin[2]) { varied = true; break; }
        require(varied, std::string("captured face has scene detail: ") + faceNames[i]);
        if (i && std::equal(begin, begin+faceBytes, first.begin())) distinct = false;
    }
    require(distinct, "face directions produce different images");
    renderer.captureEnvironment(2);
    auto second = renderer.readMap();
    require(changedPixels(first,second) > 500, "moving scene changes the captured cubemap");

    int w,h; glfwGetFramebufferSize(window,&w,&h);
    require(w > 0 && h > 0, "window framebuffer available");
    renderer.drawMain(w,h,input.camera,2,0,true);
    auto reflected = readWindow(w,h);
    renderer.drawMain(w,h,input.camera,2,0,false);
    require(changedPixels(reflected,readWindow(w,h)) > 100, "dynamic reflection differs from static sky");
    renderer.drawMain(w,h,input.camera,2,1,true);
    require(changedPixels(reflected,readWindow(w,h)) > 100, "refraction differs from reflection");
    renderer.drawMain(w,h,input.camera,2,2,true);
    require(changedPixels(reflected,readWindow(w,h)) > 100, "Fresnel mix differs from mirror");
    renderer.drawMain(w,h,input.camera,3,0,true);
    require(renderer.readMap() == second, "main rendering leaves frozen cubemap unchanged");
    require(glGetError() == GL_NO_ERROR, "OpenGL passes without errors");

    renderer.drawMain(w,h,input.camera,2,0,true);
    renderer.drawHud(w,h,0,true,false,false,true);
    savePreview(readWindow(w,h),w,h);
    require(glGetError() == GL_NO_ERROR, "six face previews and HUD rendered");
}
}

int main(int argc, char** argv)
{
    bool verification = argc > 1 && std::string(argv[1]) == "--verify";
    glfwSetErrorCallback([](int code, const char* message) { std::cerr << "GLFW " << code << ": " << message << '\n'; });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    if (verification) glfwWindowHint(GLFW_VISIBLE,0);
    GLFWwindow* window = glfwCreateWindow(1280,900,"Dynamic Environment Mapping",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { glfwDestroyWindow(window); glfwTerminate(); return 1; }
    glfwSwapInterval(verification ? 0 : 1);
    Input input;
    glfwSetWindowUserPointer(window,&input); glfwSetCursorPosCallback(window,mouse);
    glfwSetScrollCallback(window,[](GLFWwindow* win, double, double y) {
        static_cast<Input*>(glfwGetWindowUserPointer(win))->camera.ProcessMouseScroll(float(y));
    });
    int result = 0;
    try
    {
        Renderer renderer;
        std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n"
                  << "1 reflection | 2 refraction | 3 Fresnel | E static/dynamic\n"
                  << "Space freeze capture | P pause motion | V face previews\n"
                  << "WASD move | Right-drag look | Scroll zoom | R reset | Esc exit\n";
        if (verification) verify(renderer,window,input);
        else
        {
            std::array<bool, GLFW_KEY_LAST+1> previous{};
            bool frozen = false, paused = false, dynamic = true, faces = true;
            int mode = 0;
            float time = 0;
            double last = glfwGetTime();
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
                double now = glfwGetTime();
                float dt = std::min(float(now-last),0.1f); last = now;
                auto tapped = [&](int key) {
                    bool down = glfwGetKey(window,key) == GLFW_PRESS;
                    bool pressed = down && !previous[key]; previous[key] = down; return pressed;
                };
                if (tapped(GLFW_KEY_SPACE)) frozen = !frozen;
                if (tapped(GLFW_KEY_P)) paused = !paused;
                if (tapped(GLFW_KEY_E)) dynamic = !dynamic;
                if (tapped(GLFW_KEY_V)) faces = !faces;
                if (tapped(GLFW_KEY_R)) input = Input{};
                if (tapped(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window,true);
                if (glfwGetKey(window,GLFW_KEY_1) == GLFW_PRESS) mode = 0;
                if (glfwGetKey(window,GLFW_KEY_2) == GLFW_PRESS) mode = 1;
                if (glfwGetKey(window,GLFW_KEY_3) == GLFW_PRESS) mode = 2;
                for (const auto& key : std::array<std::pair<int,Camera_Movement>,4>{{
                         {GLFW_KEY_W,FORWARD},{GLFW_KEY_S,BACKWARD},{GLFW_KEY_A,LEFT},{GLFW_KEY_D,RIGHT}}})
                    if (glfwGetKey(window,key.first) == GLFW_PRESS) input.camera.ProcessKeyboard(key.second,dt);
                if (!paused) time += dt;
                int w,h; glfwGetFramebufferSize(window,&w,&h);
                if (w <= 0 || h <= 0) { glfwWaitEvents(); continue; }

                // Capture once even if the user freezes on the very first frame.
                if (!frozen || renderer.captureCount == 0) renderer.captureEnvironment(time);
                renderer.drawMain(w,h,input.camera,time,mode,dynamic);
                renderer.drawHud(w,h,mode,dynamic,frozen,paused,faces);
                glfwSwapBuffers(window);
            }
        }
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
    glfwDestroyWindow(window); glfwTerminate();
    return result;
}
