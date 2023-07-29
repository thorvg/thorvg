
#include "tvgGlCompositor.h"
#include "tvgGlCommon.h"

GlCompositor::GlCompositor(uint32_t width, uint32_t height) : tvg::Compositor(), fboId{0}, texId{0}, previous{nullptr}
{
    init(width, height);
}

GlCompositor::~GlCompositor()
{
    if (fboId) glDeleteFramebuffers(1, &fboId);
    if (texId) glDeleteTextures(1, &texId);
}

void GlCompositor::init(uint32_t width, uint32_t height)
{
    GL_CHECK(glGenFramebuffers(1, &fboId));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fboId));

    GL_CHECK(glGenTextures(1, &texId));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texId));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}