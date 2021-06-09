/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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
#ifndef _TVG_TVG_SAVER_H_
#define _TVG_TVG_SAVER_H_

#include "tvgTaskScheduler.h"
#include "tvgSaverMgr.h"

class TvgSaver : public Saver, public Task
{
public:
    TvgSaver(Paint* paint);
    ~TvgSaver();

    bool open(const string& path) override;
    bool write() override;
    void run(unsigned tid);
    bool close() override;

    void saveMemberIndicator(TvgIndicator ind);
    void saveMemberDataSize(ByteCounter byteCnt);
    void saveMemberDataSizeAt(ByteCounter byteCnt);
    void skipMemberDataSize();
    ByteCounter saveMemberData(const void* data, ByteCounter byteCnt);
    ByteCounter saveMember(TvgIndicator ind, ByteCounter byteCnt, const void* data);
    void resizeBuffer(uint32_t newSize);
    void rewindBuffer(ByteCounter bytesNum);

//private:
    string filePath;
    char* buffer = nullptr;
    uint32_t size = 0;
    uint32_t reserved = 0;
    char* bufferPosition = nullptr;
    Paint* root;

    bool saveHeader();
};

#endif //_TVG_TVG_SAVER_H_
