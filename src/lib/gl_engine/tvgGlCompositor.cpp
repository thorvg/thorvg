
#include "tvgGlCompositor.h"
#include "tvgGlCommon.h"

GlCompositor::GlCompositor(uint32_t width, uint32_t height) : tvg::Compositor(), fboId{0, 0}, texId{0, 0}
{
    init(width, height);
}

GlCompositor::~GlCompositor()
{
    if (fboId[0] && fboId[1]) glDeleteFramebuffers(2, fboId);
    if (texId[0] && texId[1]) glDeleteTextures(2, texId);
}

GLuint GlCompositor::targetFbo() const
{
    return fboId[0];
}

GLuint GlCompositor::sourceFbo() const
{
    return fboId[1];
}

GLuint GlCompositor::targetTex() const
{
    return texId[0];
}

GLuint GlCompositor::sourceTex() const
{
    return texId[1];
}

void GlCompositor::init(uint32_t width, uint32_t height)
{
    GL_CHECK(glClearColor(0, 0, 0, 0));

    GL_CHECK(glGenFramebuffers(2, fboId));

    GL_CHECK(glGenTextures(2, texId));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId[0]));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId[1]));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId[0]));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId[0], 0));

    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId[1]));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId[1], 0));

    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}