#pragma once
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector> // Added for reading compute shader file
#include "glm.hpp" // Added for uniform setters
#include "gtc/type_ptr.hpp" // Added for uniform setters

class Shader
{
private:
    // --- NEW --- Added COMPUTE shader type
    enum class ShaderType
    {
        NONE = -1, VERTEX = 0, FRAGMENT = 1, COMPUTE = 2
    };

    struct ShaderSource
    {
        std::string VertexShader;
        std::string FragementShader;
        // --- NEW --- Added a string for the compute shader source
        std::string ComputeShader;
    };

    // This function remains the same, parsing vertex/fragment pairs
    static ShaderSource parseShader(const std::string& filepath)
    {
        std::ifstream stream(filepath);
        if (!stream.is_open()) {
            std::cerr << "ERROR: Could not open shader file: " << filepath << std::endl;
            return { "", "" };
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

    // --- NEW --- A simpler function to read an entire file into a string for compute shaders
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
            return(contents);
        }
        std::cerr << "ERROR: Could not open shader file: " << filepath << std::endl;
        return "";
    }


    // --- MODIFIED --- Updated the error message to be more generic
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
            char* message = (char*)malloc(sizeof(char) * length);
            glGetShaderInfoLog(shader_id, length, &length, message);

            // --- MODIFIED --- More descriptive error message
            std::string shaderTypeName = "Unknown";
            if (type == GL_VERTEX_SHADER) shaderTypeName = "Vertex";
            else if (type == GL_FRAGMENT_SHADER) shaderTypeName = "Fragment";
            else if (type == GL_COMPUTE_SHADER) shaderTypeName = "Compute";

            std::cout << "Failed to compile " << shaderTypeName << " shader" << std::endl;
            std::cout << message << std::endl;

            glDeleteShader(shader_id);
            return 0;
        }

        return shader_id;
    }

    // This function remains the same for creating standard render programs
    static unsigned int CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
    {
        unsigned int program = glCreateProgram();
        unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
        unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glValidateProgram(program);

        glDeleteShader(vs);
        glDeleteShader(fs);

        return program;
    }

    // --- NEW --- Function to create a compute shader program
    static unsigned int CreateComputeProgram(const std::string& computeShaderSource)
    {
        unsigned int program = glCreateProgram();
        unsigned int cs = CompileShader(GL_COMPUTE_SHADER, computeShaderSource);

        glAttachShader(program, cs);
        glLinkProgram(program);
        glValidateProgram(program);

        glDeleteShader(cs);

        return program;
    }


public:
    unsigned int shader_obj;

    // --- MODIFIED --- The constructor now decides which path to take
    Shader(const std::string& filepath)
    {
        // Check file extension to determine shader type
        if (filepath.size() > 5 && filepath.substr(filepath.size() - 5) == ".comp")
        {
            // It's a compute shader
            std::cout << "Loading Compute Shader: " << filepath << std::endl;
            std::string computeSource = ReadFile(filepath);
            if (!computeSource.empty()) {
                shader_obj = CreateComputeProgram(computeSource);
            }
            else {
                shader_obj = 0; // Failed to load
            }
        }
        else
        {
            // It's a standard vertex/fragment shader
            std::cout << "Loading Render Shader: " << filepath << std::endl;
            ShaderSource shader = parseShader(filepath);
            shader_obj = CreateShader(shader.VertexShader, shader.FragementShader);
        }
    }

    // --- Destructor to clean up the shader program ---
    ~Shader()
    {
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
