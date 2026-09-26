#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
constexpr int viewportWidth = 800;
constexpr int viewportHeight = 600;
// Magenta marks pixels cleared but not covered by the draw call.
constexpr std::array<float, 4> unusedColor = {0.6f, 0.0f, 0.6f, 1.0f};

struct Target
{
    int width, height;
    GLuint fbo = 0, texture = 0;
};

const char* vertexSource = R"(
#version 330 core
out vec2 uv;
void main()
{
    // One oversized triangle covers the entire NDC square after clipping.
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* fragmentSource = R"(
#version 330 core
in vec2 uv;
out vec4 color;
uniform bool guides;
void main()
{
    // R and G encode viewport-relative X and Y, independently of texture size.
    vec3 c = vec3(uv, 0.2);
    if (guides)
    {
        vec2 grid = abs(fract(uv * 8.0 + 0.5) - 0.5);
        if (min(grid.x, grid.y) < 0.014) c *= 0.45;
        if (min(abs(uv.x - 0.5), abs(uv.y - 0.5)) < 0.006)
            c = vec3(1.0);
        // Correct for the viewport's 4:3 aspect ratio to draw a circle.
        float radius = length((uv - 0.5) * vec2(4.0 / 3.0, 1.0));
        if (abs(radius - 0.25) < 0.008) c = vec3(1.0);
        if (min(min(uv.x, uv.y), min(1.0 - uv.x, 1.0 - uv.y)) < 0.008)
            c = vec3(1.0);
    }
    color = vec4(c, 1.0);
}
)";

GLuint compile(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error(log);
    }
    return shader;
}

GLuint createProgram()
{
    GLuint vs = compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        throw std::runtime_error(log);
    }
    return program;
}

void createTarget(Target& t)
{
    glGenTextures(1, &t.texture);
    glBindTexture(GL_TEXTURE_2D, t.texture);
    // The attachment's allocation is controlled here, NOT by glViewport.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.width, t.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, t.texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete framebuffer");
}

void renderTarget(const Target& t, bool match)
{
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    // Binding an FBO does not change viewport state; set it for every pass.
    glViewport(0, 0, match ? t.width : viewportWidth, match ? t.height : viewportHeight);
    // With scissor disabled, glClear clears the WHOLE attachment, not just viewport.
    glClearColor(unusedColor[0], unusedColor[1], unusedColor[2], unusedColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

bool checkPixels(const Target& t, bool match)
{
    int w = match ? t.width : viewportWidth;
    int h = match ? t.height : viewportHeight;
    // Sample corners, centre and both sides of viewport boundaries.
    const std::array<int, 5> xs = {0, t.width / 2, t.width - 1,
                                  std::min(799, t.width - 1), std::min(800, t.width - 1)};
    const std::array<int, 5> ys = {0, t.height / 2, t.height - 1,
                                  std::min(599, t.height - 1), std::min(600, t.height - 1)};
    for (int y : ys)
        for (int x : xs)
        {
            unsigned char pixel[4];
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            auto expected = unusedColor;
            if (x < w && y < h)
                expected = {(x + 0.5f) / w, (y + 0.5f) / h, 0.2f, 1.0f};
            for (int c = 0; c < 4; ++c)
                if (std::abs(static_cast<int>(pixel[c]) - expected[c] * 255.0f) > 2.0f)
                {
                    std::cerr << "Unexpected pixel at " << x << "," << y << '\n';
                    return false;
                }
        }
    std::cout << "PASS attachment=" << t.width << 'x' << t.height
              << " viewport=" << w << 'x' << h << " (25 pixel checks)\n";
    return true;
}

constexpr int displayX = 20;
constexpr int displayY = 20;

void showTarget(GLFWwindow* window, const Target& t)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.035f, 0.045f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (width <= 0 || height <= 0) return;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, t.fbo);
    // Equal source/destination extents: one texel becomes one framebuffer pixel.
    // Keep the origin fixed when switching attachments. If the window is too
    // small, OpenGL clips the copy without rescaling either rectangle.
    glBlitFramebuffer(0, 0, t.width, t.height,
                      displayX, displayY, displayX + t.width, displayY + t.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool checkPresentation(GLFWwindow* window, const Target& t)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    const int visibleWidth = std::min(t.width, width - displayX);
    const int visibleHeight = std::min(t.height, height - displayY);
    if (visibleWidth <= 0 || visibleHeight <= 0) return false;
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 5; ++col)
        {
            const int x = col * (visibleWidth - 1) / 4;
            const int y = row * (visibleHeight - 1) / 4;
            unsigned char source[4], destination[4];
            glBindFramebuffer(GL_READ_FRAMEBUFFER, t.fbo);
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, source);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadBuffer(GL_BACK);
            glReadPixels(displayX + x, displayY + y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, destination);
            for (int c = 0; c < 3; ++c)
                if (std::abs(static_cast<int>(source[c]) - destination[c]) > 2)
                {
                    std::cerr << "Presentation is not pixel-for-pixel at " << x << ',' << y << '\n';
                    return false;
                }
        }
    std::cout << "PASS 1:1 presentation " << t.width << 'x' << t.height << '\n';
    return true;
}
}

int main(int argc, char** argv)
{
    const bool verify = argc > 1 && std::string(argv[1]) == "--verify";
    glfwSetErrorCallback([](int code, const char* message) {
        std::cerr << "GLFW " << code << ": " << message << '\n';
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    if (verify) glfwWindowHint(GLFW_VISIBLE, 0);
    GLFWwindow* window = glfwCreateWindow(1240, 940, "Viewport / attachment size (1:1)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "GLAD initialization failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwSwapInterval(verify ? 0 : 1);

    std::array<Target, 3> targets = {{{400, 300}, {800, 600}, {1200, 900}}};
    GLuint program = 0, vao = 0;
    int result = 0;
    try
    {
        program = createProgram();
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glUseProgram(program);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_DITHER);
        glUniform1i(glGetUniformLocation(program, "guides"), !verify);
        for (auto& t : targets) createTarget(t);
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n'
                  << "Keys 1/2/3: 400x300 / 800x600 / 1200x900 attachment at 1:1 pixels\n"
                  << "Fixed lower-left origin (20,20). Resize clips; it never scales.\n"
                  << "Space: toggle viewport 800x600 / match attachment. Esc: quit.\n"
                  << "Magenta: cleared but not drawn. White: NDC border, centre axes, circle.\n";

        if (verify)
        {
            for (bool match : {false, true})
                for (const auto& t : targets)
                {
                    renderTarget(t, match);
                    if (!checkPixels(t, match)) result = 1;
                    showTarget(window, t);
                    if (!checkPresentation(window, t)) result = 1;
                }
        }
        else
        {
            bool match = false, wasSpace = false;
            int selected = 1;
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents();
                if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                    glfwSetWindowShouldClose(window, true);
                bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
                if (space && !wasSpace) match = !match;
                wasSpace = space;
                if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) selected = 0;
                if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) selected = 1;
                if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) selected = 2;
                const auto& target = targets[selected];
                const std::string title = "1:1 | Attachment " + std::to_string(target.width) + "x"
                    + std::to_string(target.height) + (match ? " | MATCHED viewport" : " | Viewport 800x600")
                    + " | 1/2/3: select | Space: match | Esc: quit";
                glfwSetWindowTitle(window, title.c_str());
                for (const auto& t : targets) renderTarget(t, match);
                showTarget(window, target);
                glfwSwapBuffers(window);
            }
        }
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error detected");
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (auto& t : targets)
    {
        glDeleteFramebuffers(1, &t.fbo);
        glDeleteTextures(1, &t.texture);
    }
    glDeleteVertexArrays(1, &vao);
    if (program) glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
