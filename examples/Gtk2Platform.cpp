/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

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

#include "Common.h"
#include <GLES2/gl2.h>
#include <gtk/gtk.h>
#include <gtkgl/gdkgl.h>
#include <gtkgl/gtkglarea.h>

#include "sys/time.h"
#include <vector>
#include <dirent.h>

//performance measure
double updateTime = 0;
double accumUpdateTime = 0;
double accumRasterTime = 0;
double accumTotalTime = 0;
uint32_t cnt = 0;

static GdkPixbuf * pixbuf;
static tvg::Canvas* canvas;
static bool updated = false;

struct _transit {
    bool valid;
    bool auto_reverse;
    double dur;
    double step;
    int repeat;
    int count;
    double cur;
    AnimatCb cb;
    void* data;
};

static std::vector<struct _transit> transitions;

void plat_init(int argc, char* argv[])
{
    gtk_init(&argc, &argv);
}

void plat_run(void)
{
    gtk_main();
}

void plat_shutdown(void)
{
}

tvg::Canvas * getCanvas(void)
{
    return canvas;
}

bool getUpdate(void)
{
    return updated;
}

void setUpdate(bool b)
{
    updated = b;
}

static void win_del(GtkWidget* o, gpointer evd)
{
    g_object_unref(pixbuf);
    gtk_main_quit();
}

static unique_ptr<tvg::SwCanvas> swCanvas;

static void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ABGR8888);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    canvas = swCanvas.get();
    tvgDrawCmds(swCanvas.get());
}

static void drawSwView(void* data, void* obj)
{
    auto before = system_time_get();
    swCanvas->update();

    auto after = system_time_get();

    updateTime += (after - before);

    before = system_time_get();

    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
        updated = false;
    }

    after = system_time_get();

    auto rasterTime = after - before;

    ++cnt;

    accumUpdateTime += updateTime;
    accumRasterTime += rasterTime;
    accumTotalTime += (updateTime + rasterTime);

    printf("[%5d]: update = %fs,   raster = %fs,  total = %fs\n", cnt, accumUpdateTime / cnt, accumRasterTime / cnt, accumTotalTime / cnt);

    updateTime = 0;
}

static gboolean expose(GtkWidget *widget, GdkEventExpose *event)
{
    gdk_pixbuf_fill(pixbuf, 0x000000FF);

    drawSwView(NULL, NULL);

    gdk_draw_pixbuf(widget->window, widget->style->white_gc, pixbuf, 0, 0, 0, 0, WIDTH, HEIGHT, GDK_RGB_DITHER_NONE,0,0);
    return FALSE;
}

void* createSwView(uint32_t w, uint32_t h)
{
    cout << "tvg engine: software" << endl;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), w, h);
    gtk_window_set_title(GTK_WINDOW(window), "ThorVG Test");
    g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK (win_del), NULL);

    GtkWidget *drawarea = gtk_drawing_area_new();
    g_signal_connect (G_OBJECT(drawarea), "expose_event", G_CALLBACK (expose), NULL);

    gtk_container_add (GTK_CONTAINER (window), drawarea);

    gtk_widget_show(drawarea);
    gtk_widget_show(window);

    uint32_t* buffer = (uint32_t*)gdk_pixbuf_get_pixels(pixbuf);
    tvgSwTest(buffer);

    return (void*)drawarea;
}

void updateSwView(void* data)
{
    GtkWidget * widget = (GtkWidget*)data;
    gtk_widget_queue_draw(GTK_WIDGET(widget));
}

static gboolean animatorSwCb(gpointer data)
{
    for (auto& tr : transitions) {
        if (!tr.valid) {
            continue;
        }

        double progress = tr.cur / tr.dur;
        tr.cb(tr.data, NULL, progress);
        tr.cur += tr.step; // 30 fps
        if (tr.cur >= tr.dur || tr.cur <= 0) {
            if (tr.auto_reverse) {
                tr.step *= -1.0;
            } else {
                tr.cur = 0;
            }
            tr.count++;
        }

        if (tr.repeat > 0 && tr.count == tr.repeat) {
            tr.valid = false;
        }
    }

    GtkWidget * widget = (GtkWidget*)data;
    gtk_widget_queue_draw(GTK_WIDGET(widget));
    return TRUE;
}

void setAnimatorSw(void * obj)
{
    GtkWidget * widget = (GtkWidget*)obj;
    g_timeout_add(33, animatorSwCb, widget);
}

void* addAnimatorTransit(double duration, int repeat, AnimatCb cb, void * data)
{
    struct _transit tr = {
        .valid = true,
        .auto_reverse = false,
        .dur = duration * 1000.0,
        .step = 33.3,
        .repeat = repeat,
        .count = 0,
        .cur = 0.0,
        .cb = cb,
        .data = data
    };

    transitions.push_back(tr);
    return &(transitions[transitions.size()-1]);
}

void delAnimatorTransit(void* tl)
{
    struct _transit * t = (struct _transit*)tl;
    t->valid = false;
}

void setAnimatorTransitAutoReverse(void* tl, bool b)
{
    struct _transit * t = (struct _transit*)tl;
    t->auto_reverse = b;
}

static unique_ptr<tvg::GlCanvas> glCanvas;

static void realize(GtkWidget *widget)
{
    gtk_gl_area_make_current (GTK_GL_AREA (widget));

    glCanvas = tvg::GlCanvas::gen();

    //Get the drawing target id
    int32_t targetId;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &targetId);

    glCanvas->target(targetId, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    canvas = glCanvas.get();
    tvgDrawCmds(glCanvas.get());
}

static gboolean reshape (GtkWidget *widget, GdkEventConfigure *event)
{
    if (gtk_gl_area_make_current (GTK_GL_AREA(widget)))
        glViewport(0,0, widget->allocation.width, widget->allocation.height);

    return TRUE;
}

static gboolean expose_gl(GtkWidget *widget, GdkEventExpose *event)
{
    if (gtk_gl_area_make_current (GTK_GL_AREA(widget))) {

        auto before = system_time_get();
        glClearColor (0.0f, 0.0f, 0.0f, 1.0);
        glClear (GL_COLOR_BUFFER_BIT);

        glCanvas->update();

        if (glCanvas->draw() == tvg::Result::Success) {
            glCanvas->sync();
            updated = false;
        }
        auto after = system_time_get();

        /* Flush the contents of the pipeline */
        gtk_gl_area_swap_buffers(GTK_GL_AREA(widget));
        auto rasterTime = after - before;

        ++cnt;

        accumUpdateTime += updateTime;
        accumRasterTime += rasterTime;
        accumTotalTime += (updateTime + rasterTime);

        printf("[%5d]: update = %fs,   raster = %fs,  total = %fs\n", cnt, accumUpdateTime / cnt, accumRasterTime / cnt, accumTotalTime / cnt);

        updateTime = 0;
    }
  return TRUE;
}

void setAnimatorGl(void * obj)
{
    setAnimatorSw(obj);
}

void* createGlView(uint32_t w, uint32_t h)
{
    cout << "tvg engine: opengl" << endl;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), w, h);
    gtk_window_set_title(GTK_WINDOW(window), "ThorVG Test");
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(gtk_main_quit), NULL);

    static int attrlist[] = {
        GDK_GL_RGBA,
        GDK_GL_BUFFER_SIZE,1,
        GDK_GL_RED_SIZE,1,
        GDK_GL_GREEN_SIZE,1,
        GDK_GL_BLUE_SIZE,1,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_NONE
    };
    GtkWidget *glarea = gtk_gl_area_new(attrlist);
    g_signal_connect (glarea, "realize", G_CALLBACK (realize), NULL);
    g_signal_connect (glarea, "configure_event", G_CALLBACK(reshape), NULL);
    g_signal_connect (glarea, "expose_event", G_CALLBACK (expose_gl), NULL);

    gtk_container_add(GTK_CONTAINER(window), glarea);

    gtk_widget_show(glarea);
    gtk_widget_show(window);

    return (void*)glarea;
}

void updateGlView(void* data)
{
    GtkWidget * widget = (GtkWidget*)data;
    gtk_widget_queue_draw(GTK_WIDGET(widget));
}

bool file_dir_list(const char* path, bool recursive, DIR_LIST_CB cb, void * data)
{
    if (!cb || !path) {
        return false;
    }

    DIR *dir = NULL;
    struct dirent * entry;

    if ((dir = opendir(path)) == NULL) {
        return false;
    } else {
        while ((entry = readdir(dir)) != NULL) {
            if (recursive && entry->d_type == DT_DIR) {
                char buf[PATH_MAX];
                snprintf(buf, sizeof(buf), "/%s/%s", path, entry->d_name);
                file_dir_list(buf, recursive, cb, data);
            } else {
                cb(entry->d_name, path, data);
            }
        }
        closedir(dir);
    }
    return true;
}

double system_time_get(void)
{
    struct timeval t;
    gettimeofday(&t, 0);
    return (double)(t.tv_sec + t.tv_usec/1000000);
}

void* system_timer_add(double s, TimerCb cb, void* data)
{
    return (void*)((intptr_t)g_timeout_add(s * 1000, cb, data));
}

void system_timer_del(void* timer)
{
    g_source_remove((intptr_t)timer);
}
