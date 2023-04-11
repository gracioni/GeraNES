#ifndef INCLUDE_GLShaderProgram_h
#define INCLUDE_GLShaderProgram_h

#include "Loader.h"

#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

class GLShaderProgram
{
private:

    std::vector<GLuint> m_programs;
    GLuint m_shaderProgram = 0;
    bool m_linked = false;

public:

    enum ShaderType {Vertex, Fragment};

    void create() {
        if(m_shaderProgram == 0)
            m_shaderProgram = glCreateProgram();
    }

    ~GLShaderProgram() {

        for(int i = 0; i < m_programs.size(); i++)
            glDeleteShader(m_programs[i]);

        if(m_shaderProgram)
            glDeleteProgram(m_shaderProgram);
    }

    bool addShaderFromSourceCode(ShaderType type, const char* text) {

        switch(type) {

            case Vertex: {
                GLint vertexShader = glCreateShader(GL_VERTEX_SHADER);
                glShaderSource(vertexShader, 1, &text, NULL);
                glCompileShader(vertexShader);
                // check for shader compile errors
                GLint success;
                
                glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
                if (success) {
                    m_programs.push_back(vertexShader);               
                }
                else {       
                    GLchar infoLog[512];
                    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
                    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
                    return false;
                }

                break;
            }
            
            case Fragment: {

                int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
                glShaderSource(fragmentShader, 1, &text, NULL);
                glCompileShader(fragmentShader);

                GLint success;                

                glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
                if (success) {
                    m_programs.push_back(fragmentShader);
                }
                else {
                    GLchar infoLog[512];
                    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
                    std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
                    return false;
                }               
                break;
            }
            
        }

        return true;
    
    }

    bool link() {

        GLint success;

        create();

        for(int i = 0; i < m_programs.size(); i++)
            glAttachShader(m_shaderProgram, m_programs[i]);

        glLinkProgram(m_shaderProgram);

        glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            // The program failed to link
            GLint infoLogLength;
            glGetProgramiv(m_shaderProgram, GL_INFO_LOG_LENGTH, &infoLogLength);
            char* infoLog = new char[infoLogLength + 1];
            glGetProgramInfoLog(m_shaderProgram, infoLogLength, NULL, infoLog);

            std::cout << infoLog;

            // Print or log the infoLog to investigate the cause of the link failure
            delete[] infoLog;
            return false;
        }

        m_linked = true;

        return true;
    }

    void bindAttributeLocation(const char* name, int location) {

        create();

        glBindAttribLocation(m_shaderProgram, location, name);

        // When this function is called after the program has been linked,
        // the program will need to be relinked for the change to take effect.
        if(m_linked) link();
    }

    bool bind() {

        if(m_shaderProgram == 0) return false;

        glUseProgram(m_shaderProgram);

        // Check if the shader program was successfully activated
        GLint currentProgram;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

        return m_shaderProgram == currentProgram;
    }

    void release() {
        glUseProgram(0);
    }

    void setUniformValue(const char* name, GLint value) {
        GLint uniformLocation = glGetUniformLocation(m_shaderProgram, name);
        glUniform1i(uniformLocation, value);
    }

    void setUniformValue(const char* name, GLuint value) {
        GLint uniformLocation = glGetUniformLocation(m_shaderProgram, name);
        glUniform1ui(uniformLocation, value);
    }

    void setUniformValue(const char* name, GLfloat value) {
        GLint uniformLocation = glGetUniformLocation(m_shaderProgram, name);
        glUniform1f(uniformLocation, value);
    }

    void setUniformValue(const char* name, const glm::mat4x4 value) {
        GLint uniformLocation = glGetUniformLocation(m_shaderProgram, name);
        glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, glm::value_ptr(value));
    }
};

#endif