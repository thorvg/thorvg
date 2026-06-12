/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

 #include "tvgMediaLoader.h"

 #if defined(THORVG_APPLE_MEDIA_SUPPORT)
    #include "tvgAvfMediaLoader.h"

    MediaLoader* MediaLoader::gen()
    {
        return new AvfMediaLoader;
    }
#elif defined(THORVG_LINUX_MEDIA_SUPPORT)
    #include "tvgGstMediaLoader.h"

    MediaLoader* MediaLoader::gen()
    {
        return new GstMediaLoader;
    }
#elif defined(THORVG_WEB_MEDIA_SUPPORT)
    #include "tvgWebMediaLoader.h"

    MediaLoader* MediaLoader::gen()
    {
        return new WebMediaLoader;
    }
#else
    MediaLoader* MediaLoader::gen()
    {
        TVGLOG("MEDIA", "Couldn't find a proper Media loader.");
        return nullptr;
    }
#endif
