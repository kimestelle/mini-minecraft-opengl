#pragma once

#include "drawable.h"
#include <glm_includes.h>

class Quad : public Drawable {
public:
    Quad(OpenGLContext* context);

    void create();
    void createVBOdata() override;
    GLenum drawMode() override { return GL_TRIANGLES; }

private:
    void generateUVBuffer();
};
