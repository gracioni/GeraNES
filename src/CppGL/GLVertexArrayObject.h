#ifndef INCLUDE_GLVertexArrayObject_H
#define INCLUDE_GLVertexArrayObject_H

#include "Loader.h"

class GLVertexArrayObject
{
    private:

        GLuint m_vaoID = 0;

    public:

        void create() {
            glGenVertexArrays(1, &m_vaoID);
        }

        bool isCreated() {
            return m_vaoID != 0;
        }

        void bind() {
            glBindVertexArray(m_vaoID);
        }

        void release() {
            glBindVertexArray(0);
        }

        ~GLVertexArrayObject() {
            if(isCreated()) {
                glDeleteVertexArrays(1, &m_vaoID);
                m_vaoID = 0;
            }
        }
};

#endif
