#pragma once
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "glm.hpp"
#include "gtc/type_ptr.hpp"

class Shader
{
private:
    enum class ShaderType
    {
        NONE = -1, VERTEX = 0, FRAGMENT = 1, COMPUTE = 2
    };

    struct ShaderSource
    {
        std::string VertexShader;
        std::string FragmentShader;
        std::string ComputeShader;
    };

    static ShaderSource parseShader(const std::string& filepath)
    {
        std::ifstream stream(filepath);
        if (!stream.is_open()) {
            std::cerr << "ERROR: Could not open shader file: " << filepath << std::endl;
            return { "", "", "" };
        }

        std::string line;
        std::stringstream ss[2];
        ShaderType type = ShaderType::NONE;

        while (std::getline(stream, line)) {
            if (line.find("#shader") != std::string::npos) {
                if (line.find("vertex") != std::string::npos)
                    type = ShaderType::VERTEX;
                else if (line.find("fragment") != std::string::npos)
                    type = ShaderType::FRAGMENT;
            }
            else {
                if (type != ShaderType::NONE && (int)type < 2) {
                    ss[(int)type] << line << '\n';
                }
            }
        }

        return { ss[0].str(), ss[1].str(), "" };
    }

    static std::string ReadFile(const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::in | std::ios::binary);
        if (file)
        {
            std::string contents;
            file.seekg(0, std::ios::end);
            contents.resize(file.tellg());
            file.seekg(0, std::ios::beg);
            file.read(&contents[0], contents.size());
            file.close();
            return contents;
        }
        std::cerr << "ERROR: Could not open shader file: " << filepath << std::endl;
        return "";
    }

    static unsigned int CompileShader(unsigned int type, const std::string& source)
    {
        unsigned int shader_id = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader_id, 1, &src, nullptr);
        glCompileShader(shader_id);

        int result;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);

        if (result == GL_FALSE)
        {
            int length;
            glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> message(length);
            glGetShaderInfoLog(shader_id, length, &length, &message[0]);

            std::string shaderTypeName = "Unknown";
            if (type == GL_VERTEX_SHADER) shaderTypeName = "Vertex";
            else if (type == GL_FRAGMENT_SHADER) shaderTypeName = "Fragment";
            else if (type == GL_COMPUTE_SHADER) shaderTypeName = "Compute";

            std::cerr << "ERROR: Failed to compile " << shaderTypeName << " shader!" << std::endl;
            std::cerr << &message[0] << std::endl;

            glDeleteShader(shader_id);
            return 0;
        }

        return shader_id;
    }

    static bool CheckProgramStatus(unsigned int program, GLenum statusType, const std::string& statusName) {
        int success;
        glGetProgramiv(program, statusType, &success);
        if (!success) {
            int length;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> infoLog(length);
            glGetProgramInfoLog(program, length, NULL, &infoLog[0]);
            std::cerr << "ERROR: Shader program " << statusName << " failed!\n" << &infoLog[0] << std::endl;
            return false;
        }
        return true;
    }

    static unsigned int CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
    {
        unsigned int program = glCreateProgram();
        unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
        unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

        if (vs == 0 || fs == 0) {
            glDeleteProgram(program);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return 0;
        }

        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        
        if (!CheckProgramStatus(program, GL_LINK_STATUS, "linking")) {
            glDeleteProgram(program);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return 0;
        }

        glValidateProgram(program);

        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        return program;
    }

    static unsigned int CreateComputeProgram(const std::string& computeShaderSource)
    {
        unsigned int program = glCreateProgram();
        unsigned int cs = CompileShader(GL_COMPUTE_SHADER, computeShaderSource);

        if (cs == 0) {
            glDeleteProgram(program);
            return 0;
        }

        glAttachShader(program, cs);
        glLinkProgram(program);

        if (!CheckProgramStatus(program, GL_LINK_STATUS, "linking")) {
            glDeleteProgram(program);
            glDeleteShader(cs);
            return 0;
        }

        glValidateProgram(program);

        glDetachShader(program, cs);
        glDeleteShader(cs);

        return program;
    }

public:
    unsigned int shader_obj;

    Shader(const std::string& filepath) : shader_obj(0)
    {
        if (filepath.size() > 5 && filepath.substr(filepath.size() - 5) == ".comp")
        {
            std::cout << "Loading Compute Shader: " << filepath << std::endl;
            std::string computeSource = ReadFile(filepath);
            if (!computeSource.empty()) {
                shader_obj = CreateComputeProgram(computeSource);
            }
        }
        else
        {
            std::cout << "Loading Render Shader: " << filepath << std::endl;
            ShaderSource shader = parseShader(filepath);

            if (shader.VertexShader.empty() || shader.FragmentShader.empty()) {
                std::cerr << "ERROR: Shader source code for vertex or fragment is missing in file: " << filepath << std::endl;
            }
            else {
                shader_obj = CreateShader(shader.VertexShader, shader.FragmentShader);
            }
        }

        if (shader_obj == 0) {
            std::cerr << "FATAL: Shader program creation failed for file: " << filepath << std::endl;
        }
    }

    ~Shader()
    {
        if (shader_obj != 0)
            glDeleteProgram(shader_obj);
    }

    void setMat4(const char* name, glm::mat4 var)
    {
        glUniformMatrix4fv(glGetUniformLocation(shader_obj, name), 1, GL_FALSE, glm::value_ptr(var));
    }
    void setVec3(const char* name, glm::vec3 var)
    {
        glUniform3f(glGetUniformLocation(shader_obj, name), var.x, var.y, var.z);
    }
    void setFloat(const char* name, float var)
    {
        glUniform1f(glGetUniformLocation(shader_obj, name), var);
    }
    void setTexture(const char* name, int var)
    {
        int textureLoc = glGetUniformLocation(shader_obj, name);
        glUniform1i(textureLoc, var);
    }
};
