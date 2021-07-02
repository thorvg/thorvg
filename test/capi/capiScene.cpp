/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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

#include <thorvg_capi.h>
#include "../catch.hpp"

TEST_CASE("Scene Creation", "[capiScene]")
{
    Tvg_Paint* scene = tvg_scene_new();
    REQUIRE(scene);
    

}

TEST_CASE("Pushing Paints Into Scene", "[capiScene]")
{
    Tvg_Paint* scene = tvg_scene_new();
    REQUIRE(scene);
    Tvg_Paint* paint = tvg_picture_new();
    REQUIRE(paint);
    
    REQUIRE(tvg_scene_push(scene, paint) == TVG_RESULT_SUCCESS);
    
    
}

TEST_CASE("Scene Memory Reservation", "[capiScene]")
{
    Tvg_Paint* scene = tvg_scene_new();
    REQUIRE(scene);
    
    REQUIRE(tvg_scene_reserve(scene,100) == TVG_RESULT_SUCCESS):
    
}

TEST_CASE("Scene Clear", "[capiScene]")
{
     Tvg_Paint* scene = tvg_scene_new();
     REQUIRE(scene);
     
     REQUIRE(tvg_scene_clear(scene,1) == TVG_RESULT_SUCCESS) ;
    
}


