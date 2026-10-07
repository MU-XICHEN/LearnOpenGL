#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

struct Basis
{
    glm::vec3 t, b, n;
};
inline float orthogonalityError(const Basis& f)
{
    return std::max({std::abs(glm::dot(f.t,f.n)), std::abs(glm::dot(f.b,f.n)),
                     std::abs(glm::dot(f.t,f.b))});
}
inline Basis orthogonalize(Basis f)
{
    f.n = glm::normalize(f.n);
    float sign = glm::dot(glm::cross(f.n,f.t),f.b) < 0 ? -1.0f : 1.0f;
    f.t = glm::normalize(f.t - f.n * glm::dot(f.n,f.t));
    f.b = sign * glm::normalize(glm::cross(f.n,f.t));
    return f;
}
struct Vertex
{
    glm::vec3 position, normal;
    glm::vec2 uv;
    glm::vec3 triangleT, triangleB, averageT, averageB;
};
struct Mesh
{
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;
    float faceError = 0, averageError = 0, fixedError = 0, seamAngle = 0;
    glm::vec3 probePosition{};
    Basis probeBasis{};
    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    ~Mesh() { glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo); }

    void create(int columns, int rows)
    {
        struct Point { glm::vec3 p, n{0}, t{0}, b{0}; glm::vec2 uv; };
        struct Triangle { std::array<int,3> indices; Basis frame; };
        std::vector<Point> points;
        std::vector<Triangle> triangles;
        constexpr float pi = 3.14159265358979323846f;
        // 光滑的旋转曲面。非均匀采样及交替对角线使平均误差更容易观察。
        // 连续曲面的 u/v 方向正交；没有人为向 T 或 B 注入错误分量。
        for (int y=0; y<=rows; ++y)
            for (int x=0; x<=columns; ++x)
            {
                float u = float(x)/columns, v = float(y)/rows;
                u += 0.09f * std::sin(2*pi*u);
                v += 0.08f * std::sin(2*pi*v);
                float theta = -1.3f + 2.6f*u;
                float height = -1.6f + 3.2f*v;
                float radius = 1.3f + 0.27f*std::cos(2.4f*height) + 0.12f*std::sin(3.3f*height);
                Point point;
                point.p = glm::vec3(radius*std::sin(theta),height,radius*std::cos(theta));
                point.uv = glm::vec2(u,v);
                points.push_back(point);
            }
        auto addTriangle = [&](int a,int b,int c) {
            glm::vec3 e1 = points[b].p-points[a].p, e2 = points[c].p-points[a].p;
            glm::vec2 d1 = points[b].uv-points[a].uv, d2 = points[c].uv-points[a].uv;
            float determinant = d1.x*d2.y-d1.y*d2.x;
            if (std::abs(determinant) < 1e-8f) throw std::runtime_error("Degenerate UV triangle");
            Basis f;
            f.n = glm::normalize(glm::cross(e1,e2));
            f.t = glm::normalize((e1*d2.y-e2*d1.y)/determinant);
            f.b = glm::normalize((e2*d1.x-e1*d2.x)/determinant);
            // 从正交的面级坐标系出发，单独展示“平均不保持正交”这一问题。
            // 原始 UV 导数也可能互不垂直，不把那种误差混入本示例。
            f = orthogonalize(f);
            faceError = std::max(faceError,orthogonalityError(f));
            triangles.push_back({{a,b,c},f});
            const int ids[] = {a,b,c};
            for (int k=0; k<3; ++k)
            {
                Point& point = points[ids[k]];
                glm::vec3 edgeA = glm::normalize(points[ids[(k+1)%3]].p-point.p);
                glm::vec3 edgeB = glm::normalize(points[ids[(k+2)%3]].p-point.p);
                float weight = std::acos(glm::clamp(glm::dot(edgeA,edgeB),-1.0f,1.0f));
                // 在共享顶点累加邻面的切线、位切线和法线，使用角度权重。
                point.t += weight*f.t; point.b += weight*f.b; point.n += weight*f.n;
            }
        };
        for (int y=0; y<rows; ++y)
            for (int x=0; x<columns; ++x)
            {
                int a=y*(columns+1)+x, b=a+1, c=a+columns+1, d=c+1;
                if ((x+y)%2) { addTriangle(a,b,c); addTriangle(b,d,c); }
                else { addTriangle(a,b,d); addTriangle(a,d,c); }
            }
        float probeError = -1.0f;
        for (Point& point : points)
        {
            point.n = glm::normalize(point.n);
            point.t = glm::normalize(point.t); point.b = glm::normalize(point.b);
            Basis f{point.t,point.b,point.n};
            float error = orthogonalityError(f);
            averageError = std::max(averageError,error);
            fixedError = std::max(fixedError,orthogonalityError(orthogonalize(f)));
            // 选择内部误差较大的点，供界面显示 T/B/N 三根坐标轴。
            if (point.uv.x > .15f && point.uv.x < .85f && point.uv.y > .15f && point.uv.y < .85f
                && error > probeError)
            { probePosition = point.p; probeBasis = f; probeError = error; }
        }
        // GPU 展开三角形，模式 1 保留面级 T/B；平均属性来自同一共享顶点。
        std::vector<Vertex> vertices;
        std::vector<std::vector<glm::vec3>> incidentTangents(points.size());
        for (const Triangle& tri : triangles)
            for (int id : tri.indices)
            {
                const Point& p = points[id];
                for (const glm::vec3& t : incidentTangents[id])
                    seamAngle = std::max(seamAngle,glm::degrees(std::acos(glm::clamp(glm::dot(t,tri.frame.t),-1.0f,1.0f))));
                incidentTangents[id].push_back(tri.frame.t);
                vertices.push_back({p.p,p.n,p.uv,tri.frame.t,tri.frame.b,p.t,p.b});
            }
        count = static_cast<GLsizei>(vertices.size());
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
        const size_t offsets[] = {offsetof(Vertex,position),offsetof(Vertex,normal),offsetof(Vertex,uv),
            offsetof(Vertex,triangleT),offsetof(Vertex,triangleB),offsetof(Vertex,averageT),offsetof(Vertex,averageB)};
        for (GLuint i=0;i<7;++i)
        {
            glVertexAttribPointer(i,i==2 ? 2 : 3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsets[i]));
            glEnableVertexAttribArray(i);
        }
    }
    void draw() const { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,count); }
};
