/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include <iostream>
#include <thread>
#include <thorvg.h>
#include <string.h>

#include <config.h>
using namespace std;

static uint32_t WIDTH = 800;
static uint32_t HEIGHT = 800;

extern double updateTime;
extern double accumUpdateTime;
extern double accumRasterTime;
extern double accumTotalTime;
extern uint32_t cnt;

/************************************************************************/
/* Common Infrastructure Code                                           */
/************************************************************************/

void plat_init(int argc, char* argv[]);
void plat_run(void);
void plat_shutdown(void);

double system_time_get(void);

typedef void (*DIR_LIST_CB)(const char* name, const char* path, void* data);
bool file_dir_list(const char* path, bool recursive, DIR_LIST_CB cb, void * data);

void* createSwView(uint32_t w = WIDTH, uint32_t h = HEIGHT);
void setAnimatorSw(void * obj);
void updateSwView(void* obj);

typedef void (*AnimatCb)(void * data, void * obj, double progress);
void* addAnimatorTransit(double duration, int repeat, AnimatCb cb, void * data);
void setAnimatorTransitAutoReverse(void* tl, bool b);
void delAnimatorTransit(void* tl);

typedef int (*TimerCb)(void * data);
void* system_timer_add(double s, TimerCb cb, void* data);
void system_timer_del(void* timer);

tvg::Canvas* getCanvas(void);

bool getUpdate(void);
void setUpdate(bool b);

void tvgDrawCmds(tvg::Canvas* canvas);

#ifndef NO_GL_EXAMPLE
void* createGlView(uint32_t w = WIDTH, uint32_t h = HEIGHT);
void setAnimatorGl(void * obj);
void updateGlView(void* obj);
#endif //NO_GL_EXAMPLE
