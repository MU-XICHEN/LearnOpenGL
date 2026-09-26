#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>

struct Vertex { glm::vec3 position, normal; };

struct Mesh
{
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;
    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    ~Mesh() { glDeleteVertexArrays(1, &vao); glDeleteBuffers(1, &vbo); }
    void upload(const std::vector<Vertex>& vertices)
    {
        count = static_cast<GLsizei>(vertices.size());
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)sizeof(glm::vec3));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
    }
    void draw() const { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES, 0, count); }
};

inline void createCube(Mesh& mesh)
{
    std::vector<Vertex> vertices;
    const glm::vec3 normals[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (glm::vec3 n : normals)
    {
        glm::vec3 u = std::abs(n.y) > 0.5f ? glm::vec3(1,0,0) : glm::normalize(glm::cross(glm::vec3(0,1,0), n));
        glm::vec3 v = glm::cross(n, u);
        glm::vec3 p[] = {(n-u-v)*0.5f, (n+u-v)*0.5f, (n+u+v)*0.5f, (n-u+v)*0.5f};
        for (int i : {0,1,2,0,2,3}) vertices.push_back({p[i], n});
    }
    mesh.upload(vertices);
}

inline void createSphere(Mesh& mesh)
{
    constexpr int rings = 48, sectors = 96;
    constexpr float pi = 3.14159265358979323846f;
    auto vertex = [](float theta, float phi) {
        glm::vec3 p(std::sin(theta)*std::cos(phi), std::cos(theta), std::sin(theta)*std::sin(phi));
        return Vertex{p, p};
    };
    std::vector<Vertex> vertices;
    for (int y = 0; y < rings; ++y)
        for (int x = 0; x < sectors; ++x)
        {
            float t = pi * y / rings, t1 = pi * (y+1) / rings;
            float p = 2*pi*x / sectors, p1 = 2*pi*(x+1) / sectors;
            Vertex corners[] = {vertex(t,p), vertex(t1,p), vertex(t1,p1), vertex(t,p1)};
            for (int i : {0,1,2,0,2,3}) vertices.push_back(corners[i]);
        }
    mesh.upload(vertices);
}
