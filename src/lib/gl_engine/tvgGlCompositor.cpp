
#include "tvgGlCompositor.h"
#include "tvgGlCommon.h"

GlCompositor::GlCompositor(uint32_t width, uint32_t height)
    : tvg::Compositor(), fboId{0, 0, 0}, texId{0, 0}, msaaTexId{0, 0}, mWidth(width), mHeight(height)
{
    init(width, height);
}

GlCompositor::~GlCompositor()
{
    if (fboId[0] && fboId[1] && fboId[2]) glDeleteFramebuffers(3, fboId);
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

void GlCompositor::resolveMSAA()
{
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId[2]));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId[0], 0));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    // blit fboId[0] to fboId[2]
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId[0]));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId[2]));
    GL_CHECK(glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR));

    // blit fboId[1] to fboId[2]
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId[2]));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId[1], 0));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId[1]));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId[2]));
    GL_CHECK(glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

static const GLenum color_buffer = GL_COLOR_ATTACHMENT0;

static const float transparent[] = {0.f, 0.f, 0.f, 0.f};

void GlCompositor::init(uint32_t width, uint32_t height)
{
    GL_CHECK(glDisable(GL_SCISSOR_TEST));
    GL_CHECK(glGenFramebuffers(3, fboId));

    GL_CHECK(glGenTextures(2, texId));
    GL_CHECK(glGenRenderbuffers(2, msaaTexId));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId[0]));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId[1]));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, msaaTexId[0]));
    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA, width, height));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, msaaTexId[1]));
    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA, width, height));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId[0]));
    GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaTexId[0]));

    GL_CHECK(glDrawBuffers(1, &color_buffer));
    GL_CHECK(glClearBufferfv(GL_COLOR, 0, transparent));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId[1]));
    GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaTexId[1]));

    GL_CHECK(glDrawBuffers(1, &color_buffer));
    GL_CHECK(glClearBufferfv(GL_COLOR, 0, transparent));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CHECK(glEnable(GL_SCISSOR_TEST));
}