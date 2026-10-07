#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "mesh.h"
#include "hud.h"

namespace
{
struct State
{
    int selected = -1, normalStyle = 0;
    bool dense = false, errors = false, wire = false, axes = false, animate = false;
    float yaw = 0, pitch = 0, distance = 6.8f, time = 0;
    double mouseX = 0, mouseY = 0;
    bool firstMouse = true;
    glm::vec3 eye() const
    {
        float y=glm::radians(yaw), p=glm::radians(pitch);
        return glm::vec3(distance*std::sin(y)*std::cos(p),distance*std::sin(p),distance*std::cos(y));
    }
};
std::string number(float value,int decimals=4)
{
    std::ostringstream text; text << std::fixed << std::setprecision(decimals) << value; return text.str();
}
std::string asset(const char* name)
{
    std::string local=std::string("tbn_averaging/")+name;
    if (std::ifstream(local).good()) return local;
    return FileSystem::getPath(std::string("src/test/tbn_averaging/assets/")+name);
}
Shader loadShader(const char* vs,const char* fs)
{
    Shader shader(asset(vs).c_str(),asset(fs).c_str());
    GLint linked=0; glGetProgramiv(shader.ID,GL_LINK_STATUS,&linked);
    if (!linked) { glDeleteProgram(shader.ID); throw std::runtime_error("Shader link failed"); }
    return shader;
}
struct Renderer
{
    Shader surface=loadShader("surface.vs","surface.fs");
    Shader text=loadShader("hud.vs","hud.fs");
    Shader axes=loadShader("axes.vs","axes.fs");
    Mesh coarse, dense;
    Hud hud;
    GLuint normalMap=0, axisVao=0, axisVbo=0;
    Renderer()
    {
        coarse.create(10,8); dense.create(40,32);
        // 生成真实的切线空间法线纹理，使用线性浮点格式，不作 sRGB 解码。
        constexpr int size=256;
        constexpr float pi=3.14159265358979323846f;
        std::vector<float> pixels;
        for (int y=0;y<size;++y)
            for (int x=0;x<size;++x)
            {
                float u=(x+.5f)/size, v=(y+.5f)/size;
                float slopeX=.40f+.55f*std::cos(2*pi*4*u)*std::sin(2*pi*3*v);
                float slopeY=.25f+.55f*std::sin(2*pi*4*u)*std::cos(2*pi*3*v);
                glm::vec3 encoded=glm::normalize(glm::vec3(slopeX,slopeY,1))*.5f+.5f;
                pixels.insert(pixels.end(),{encoded.x,encoded.y,encoded.z});
            }
        glGenTextures(1,&normalMap); glBindTexture(GL_TEXTURE_2D,normalMap);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,size,size,0,GL_RGB,GL_FLOAT,pixels.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        surface.use(); surface.setInt("normalMap",0);
        glGenVertexArrays(1,&axisVao); glGenBuffers(1,&axisVbo);
        glBindVertexArray(axisVao); glBindBuffer(GL_ARRAY_BUFFER,axisVbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));
        glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    }
    ~Renderer()
    {
        for (GLuint id : {surface.ID,text.ID,axes.ID}) glDeleteProgram(id);
        glDeleteTextures(1,&normalMap); glDeleteVertexArrays(1,&axisVao); glDeleteBuffers(1,&axisVbo);
    }
    Mesh& mesh(const State& state) { return state.dense ? dense : coarse; }
    void begin()
    {
        glBindFramebuffer(GL_FRAMEBUFFER,0); glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
        glClearColor(.02f,.03f,.045f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    }
    void picture(const State& state,int mode,int x,int y,int width,int height)
    {
        glViewport(x,y,width,height); glEnable(GL_DEPTH_TEST);
        glm::vec3 eye=state.eye();
        glm::mat4 view=glm::lookAt(eye,glm::vec3(0),glm::vec3(0,1,0));
        glm::mat4 projection=glm::perspective(glm::radians(40.0f),float(width)/height,.1f,50.0f);
        surface.use(); surface.setMat4("view",view); surface.setMat4("projection",projection);
        surface.setVec3("cameraPos",eye);
        surface.setVec3("lightPos",glm::vec3(-2.4f*std::cos(state.time),2.6f,4.0f+std::sin(state.time)));
        surface.setInt("mode",mode); surface.setInt("normalStyle",state.normalStyle);
        surface.setBool("errorView",state.errors); surface.setBool("wire",false);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,normalMap);
        mesh(state).draw();
        if (state.wire)
        {
            surface.setBool("wire",true); glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(-1,-1);
            mesh(state).draw();
            glDisable(GL_POLYGON_OFFSET_LINE); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        }
        if (state.axes && mode != 0)
        {
            Mesh& selected=mesh(state);
            Basis f=mode==3 ? orthogonalize(selected.probeBasis) : selected.probeBasis;
            glm::vec3 origin=selected.probePosition;
            const glm::vec3 vectors[]={f.t,f.b,f.n};
            const glm::vec3 colors[]={{1,.15f,.12f},{.15f,1,.25f},{.15f,.55f,1}};
            std::vector<float> vertices;
            for (int i=0;i<3;++i)
                for (glm::vec3 p : {origin,origin+vectors[i]*.6f})
                    vertices.insert(vertices.end(),{p.x,p.y,p.z,colors[i].x,colors[i].y,colors[i].z});
            glDisable(GL_DEPTH_TEST);
            axes.use(); axes.setMat4("view",view); axes.setMat4("projection",projection);
            glBindVertexArray(axisVao); glBindBuffer(GL_ARRAY_BUFFER,axisVbo);
            glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STREAM_DRAW);
            glDrawArrays(GL_LINES,0,6);
        }
    }
    void draw(const State& state,int width,int height)
    {
        begin();
        const char* labels[]={"1 - TRIANGLE T/B","2 - AVERAGED T/B","3 - AVERAGED BASIS ERROR","4 - GRAM-SCHMIDT"};
        auto panel=[&](int mode,int x,int y,int w,int h) {
            picture(state,mode,x,y+24,w,h-51);
            hud.text(labels[mode],float(x+12),float(y+h-21),1.7f);
            Mesh& m=mesh(state);
            std::string caption;
            if (mode==0) caption="SHARED VERTEX T JUMP UP TO "+number(m.seamAngle,1)+" DEG";
            else if (mode==3) caption="VERTEX ERROR "+number(m.fixedError,6);
            else caption="AVERAGED VERTEX ERROR UP TO "+number(m.averageError,4);
            hud.text(caption,float(x+12),float(y+7),1.3f);
        };
        int available=height-133;
        if (state.selected < 0)
        {
            int w=width/2, h=available/2;
            panel(0,0,46+h,w,available-h); panel(1,w,46+h,width-w,available-h);
            panel(2,0,46,w,h); panel(3,w,46,width-w,h);
        }
        else panel(state.selected,0,46,width,available);
        glViewport(0,0,width,height); glDisable(GL_DEPTH_TEST);
        const char* styles[]={"WAVES NORMAL MAP","CONSTANT TILTED NORMAL","FLAT NORMAL - CONTROL"};
        hud.text("TBN: TRIANGLES / AVERAGING / ORTHOGONALIZATION",14,float(height-24),2);
        hud.text(std::string(styles[state.normalStyle])+(state.dense ? " | DENSE 40X32" : " | COARSE 10X8")
            +(state.errors ? " | ALL ERROR MAPS" : ""),14,float(height-44),1.5f);
        hud.text("0 COMPARE | 1/2/3/4 FOCUS | N NORMAL MAP | C MESH | E ERROR | W WIRE | A AXES",14,float(height-62),1.5f);
        hud.text("RIGHT DRAG ORBIT | WHEEL ZOOM | L MOVE LIGHT | R RESET | ESC EXIT",14,float(height-80),1.5f);
        hud.text("ERROR: MAX ABS PAIR DOT | BLUE 0 / GOLD 0.05 / RED 0.10+",14,26,1.4f);
        hud.text("AXES: RED T / GREEN B / BLUE N | SAME GEOMETRY, UV, SMOOTH N AND LIGHT",14,8,1.3f);
        hud.draw(text,width,height);
    }
};
void mouse(GLFWwindow* window,double x,double y)
{
    auto& s=*static_cast<State*>(glfwGetWindowUserPointer(window));
    if (glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)!=GLFW_PRESS) s.firstMouse=true;
    else
    {
        if (!s.firstMouse) { s.yaw+=float(x-s.mouseX)*.25f; s.pitch=glm::clamp(s.pitch+float(y-s.mouseY)*.25f,-75.0f,75.0f); }
        s.firstMouse=false;
    }
    s.mouseX=x; s.mouseY=y;
}
std::vector<unsigned char> readPixels(int width,int height)
{
    std::vector<unsigned char> pixels(width*height*4);
    glReadBuffer(GL_BACK); glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    return pixels;
}
size_t differentPixels(const std::vector<unsigned char>& a,const std::vector<unsigned char>& b)
{
    size_t count=0;
    for (size_t i=0;i<a.size();i+=4)
        if (std::abs(int(a[i])-int(b[i]))+std::abs(int(a[i+1])-int(b[i+1]))+std::abs(int(a[i+2])-int(b[i+2]))>5) ++count;
    return count;
}
void require(bool condition,const char* message)
{
    if (!condition) throw std::runtime_error(std::string("VERIFY FAILED: ")+message);
    std::cout << "PASS " << message << '\n';
}
void savePreview(int width,int height,const char* name)
{
    auto pixels=readPixels(width,height);
    std::ofstream out(std::string("tbn_averaging/")+name+".ppm",std::ios::binary);
    if (!out) throw std::runtime_error("Run --verify from executable directory");
    out << "P6\n" << width << " " << height << "\n255\n";
    for (int y=height-1;y>=0;--y)
        for (int x=0;x<width;++x) out.write(reinterpret_cast<const char*>(pixels.data()+(y*width+x)*4),3);
}
void verify(Renderer& renderer,int width,int height)
{
    Mesh& m=renderer.coarse;
    std::cout << "Face basis error " << m.faceError << ", averaged vertex error " << m.averageError
              << ", corrected error " << m.fixedError << ", neighboring T angle " << m.seamAngle << " degrees\n";
    require(m.faceError<1e-5f,"input triangle frames are orthonormal");
    require(m.averageError>.001f,"averaging normalized frames loses orthogonality");
    require(m.fixedError<1e-5f,"Gram-Schmidt restores vertex orthogonality");
    require(m.seamAngle>1.0f,"unaveraged tangent differs across shared vertices");
    State state;
    auto render=[&](int mode) {
        renderer.begin(); renderer.picture(state,mode,0,0,width,height); return readPixels(width,height);
    };
    auto triangle=render(0), average=render(1), fixed=render(3);
    require(differentPixels(triangle,average)>500,"GPU lighting changes when tangents are averaged");
    require(differentPixels(average,fixed)>50,"GPU lighting exposes non-orthogonal basis distortion");
    state.normalStyle=2;
    triangle=render(0); average=render(1); fixed=render(3);
    require(triangle==average && average==fixed,"flat normal control matches in all three lighting modes");
    state=State{}; renderer.draw(state,width,height); savePreview(width,height,"comparison");
    state.errors=true; renderer.draw(state,width,height); savePreview(width,height,"errors");
    state.errors=false; state.normalStyle=1; renderer.draw(state,width,height); savePreview(width,height,"constant_normal");
    state.dense=true; renderer.draw(state,width,height);
    require(glGetError()==GL_NO_ERROR,"OpenGL texture, dense mesh, panels and readback without errors");
}
}
int main(int argc,char** argv)
{
    bool verification=argc>1 && std::string(argv[1])=="--verify";
    glfwSetErrorCallback([](int code,const char* message) { std::cerr << "GLFW " << code << ": " << message << '\n'; });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    if (verification) glfwWindowHint(GLFW_VISIBLE,0);
    GLFWwindow* window=glfwCreateWindow(1400,1000,"TBN averaging - normal mapping",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { glfwDestroyWindow(window); glfwTerminate(); return 1; }
    glfwSwapInterval(verification ? 0 : 1);
    State state; glfwSetWindowUserPointer(window,&state); glfwSetCursorPosCallback(window,mouse);
    glfwSetScrollCallback(window,[](GLFWwindow* win,double,double y) {
        auto& s=*static_cast<State*>(glfwGetWindowUserPointer(win)); s.distance=glm::clamp(s.distance-float(y)*.35f,3.8f,12.0f);
    });
    int result=0;
    try
    {
        Renderer renderer;
        std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n"
                  << "0 compare | 1/2/3/4 focus | N normal map | C mesh | E errors | W wire | A axes\n"
                  << "Right drag orbit | Wheel zoom | L moving light | R reset | Esc exit\n";
        int width,height; glfwGetFramebufferSize(window,&width,&height);
        if (verification) verify(renderer,width,height);
        else
        {
            std::array<bool,GLFW_KEY_LAST+1> previous{};
            double last=glfwGetTime();
            while (!glfwWindowShouldClose(window))
            {
                glfwPollEvents(); double now=glfwGetTime(); float dt=std::min(float(now-last),.1f); last=now;
                auto tapped=[&](int key) {
                    bool down=glfwGetKey(window,key)==GLFW_PRESS, pressed=down&&!previous[key]; previous[key]=down; return pressed;
                };
                if (tapped(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window,true);
                if (tapped(GLFW_KEY_R)) state=State{};
                for (int i=0;i<=4;++i) if (tapped(GLFW_KEY_0+i)) state.selected=i-1;
                if (tapped(GLFW_KEY_N)) state.normalStyle=(state.normalStyle+1)%3;
                if (tapped(GLFW_KEY_C)) state.dense=!state.dense;
                if (tapped(GLFW_KEY_E)) state.errors=!state.errors;
                if (tapped(GLFW_KEY_W)) state.wire=!state.wire;
                if (tapped(GLFW_KEY_A)) state.axes=!state.axes;
                if (tapped(GLFW_KEY_L)) state.animate=!state.animate;
                if (state.animate) state.time+=dt*.7f;
                glfwGetFramebufferSize(window,&width,&height);
                if (width<240 || height<300) { glfwWaitEvents(); continue; }
                renderer.draw(state,width,height); glfwSwapBuffers(window);
            }
        }
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; result=1; }
    glfwDestroyWindow(window); glfwTerminate(); return result;
}
