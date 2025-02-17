/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tvgWgGeometry.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static WgGeometryBufferPool _pool;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

WgVertexBuffer* WgGeometryBufferPool::reqVertexBuffer(float scale)
{
    ARRAY_FOREACH(p, vbuffers) {
        if ((*p)->count == 0) {
            (*p)->scale = scale;
            return (*p);
        }
    }
    vbuffers.push(new WgVertexBuffer(scale));
    return vbuffers.last();
}

void WgGeometryBufferPool::retVertexBuffer(WgVertexBuffer* buffer)
{
    buffer->reset(1.0f);
}

WgIndexedVertexBuffer* WgGeometryBufferPool::reqIndexedVertexBuffer(float scale)
{
    ARRAY_FOREACH(p, ibuffers) {
        if ((*p)->vcount == 0) {
            (*p)->scale = scale;
            return (*p);
        }
    }
    ibuffers.push(new WgIndexedVertexBuffer(this, scale));
    return ibuffers.last();
}

void WgGeometryBufferPool::retIndexedVertexBuffer(WgIndexedVertexBuffer* buffer)
{
    buffer->reset(1.0f);
}


WgGeometryBufferPool* WgGeometryBufferPool::instance()
{
    /* TODO: These could be easily addressed per threads. i.e _pool[thread_cnt]; */
    return &_pool;
}


WgGeometryBufferPool::~WgGeometryBufferPool()
{
    ARRAY_FOREACH(p, vbuffers) delete(*p);
    ARRAY_FOREACH(p, ibuffers) delete(*p);
}