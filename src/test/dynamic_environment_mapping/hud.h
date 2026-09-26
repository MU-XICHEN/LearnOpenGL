#pragma once
#include <array>
#include <string>
#include <vector>

// Small built-in bitmap font: no extra library or font asset is required.
inline std::array<unsigned char, 7> glyph(char c)
{
    switch (c)
    {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,14}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {31,4,4,4,4,4,31}; case 'J': return {7,2,2,2,2,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,25,21,19,19,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case '+': return {0,4,4,31,4,4,0}; case '-': return {0,0,0,31,0,0,0};
    case '/': return {1,1,2,4,8,16,16}; case ':': return {0,4,0,0,4,0,0};
    case '.': return {0,0,0,0,0,0,4}; case '|': return {4,4,4,4,4,4,4};
    default: return {};
    }
}

struct Hud
{
    GLuint vao = 0, vbo = 0;
    std::vector<glm::vec2> pixels;
    Hud()
    {
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
        glEnableVertexAttribArray(0);
    }
    ~Hud() { glDeleteVertexArrays(1, &vao); glDeleteBuffers(1, &vbo); }
    void text(const std::string& str, float x, float y, float scale = 2)
    {
        for (char c : str)
        {
            auto rows = glyph(c);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (rows[row] & (1 << (4-col)))
                    {
                        float a = x + col*scale, b = y + (6-row)*scale;
                        pixels.insert(pixels.end(), {{a,b},{a+scale,b},{a+scale,b+scale},
                                                     {a,b},{a+scale,b+scale},{a,b+scale}});
                    }
            x += 6*scale;
        }
    }
    void draw(const Shader& shader, int width, int height)
    {
        shader.use(); shader.setVec2("screenSize", float(width), float(height));
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, pixels.size()*sizeof(glm::vec2), pixels.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(pixels.size()));
        pixels.clear();
    }
};
