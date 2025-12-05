#pragma once

#include "GLHeaders.h"

class GLVertexBufferObject
{
    private:

        GLuint m_vboID = 0;
        GLenum m_usage = GL_STATIC_DRAW;

    public:

        void create() {
            glGenBuffers(1, &m_vboID);
        }

        bool isCreated() {
            return m_vboID != 0;
        }

        void bind() {
            glBindBuffer(GL_ARRAY_BUFFER, m_vboID);
        }

        void release() {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        void allocate(const void *data, int size) {
            bind();
            glBufferData(GL_ARRAY_BUFFER, size, data, m_usage);
            release();
        }

        void write(int offset, const void *data, int size) {
            bind();
            glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
            release();
        }

        GLint size() {
            bind();
            GLint bufferSize = 0;
            glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);
            release();

            return bufferSize;
        }

        ~GLVertexBufferObject() {
            if(isCreated()) {
                glDeleteBuffers(1, &m_vboID);
                m_vboID = 0;
            }
        }
};
