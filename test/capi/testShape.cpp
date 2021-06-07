/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include "catch.hpp"

TEST_CASE("Multiple shapes", "[capiShapes]")
{
    Tvg_Paint *paint = NULL;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_shape_append_rect(paint, 0, 0, 100, 100, 0, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_append_rect(paint, 0, 0, 100, 100, 50, 50) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_append_rect(paint, 0, 0, 100, 100, 100, 100) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_append_circle(paint, 100, 100, 50, 50) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_append_circle(paint, 100, 100, 0, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_append_arc(paint, 100, 100, 50, 90, 90, 0) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Shape path", "[capiShapePath]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Path_Command * cmds_get;
    Tvg_Point * pts_get;
    uint32_t cnt;
    uint32_t i;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    Tvg_Path_Command cmds[11];
    cmds[0] = TVG_PATH_COMMAND_MOVE_TO;
    cmds[1] = TVG_PATH_COMMAND_LINE_TO;
    cmds[2] = TVG_PATH_COMMAND_LINE_TO;
    cmds[3] = TVG_PATH_COMMAND_LINE_TO;
    cmds[4] = TVG_PATH_COMMAND_LINE_TO;
    cmds[5] = TVG_PATH_COMMAND_LINE_TO;
    cmds[6] = TVG_PATH_COMMAND_LINE_TO;
    cmds[7] = TVG_PATH_COMMAND_LINE_TO;
    cmds[8] = TVG_PATH_COMMAND_LINE_TO;
    cmds[9] = TVG_PATH_COMMAND_LINE_TO;
    cmds[10] = TVG_PATH_COMMAND_CLOSE;

    Tvg_Point pts[10];
    pts[0] = {199, 34};  //MoveTo
    pts[1] = {253, 143}; //LineTo
    pts[2] = {374, 160}; //LineTo
    pts[3] = {287, 244}; //LineTo
    pts[4] = {307, 365}; //LineTo
    pts[5] = {199, 309}; //LineTo
    pts[6] = {97, 365};  //LineTo
    pts[7] = {112, 245}; //LineTo
    pts[8] = {26, 161};  //LineTo
    pts[9] = {146, 143}; //LineTo

    REQUIRE(tvg_shape_append_path(paint, cmds, 11, pts, 10) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_get_path_commands(paint, (const Tvg_Path_Command**) &cmds_get, &cnt) == TVG_RESULT_SUCCESS);
    REQUIRE(cnt == 11);
    for (i = 0; i < cnt; i++) {
        REQUIRE(cmds_get[i] == cmds[i]);
    }

    REQUIRE(tvg_shape_get_path_coords(paint, (const Tvg_Point**) &pts_get, &cnt) == TVG_RESULT_SUCCESS);
    REQUIRE(cnt == 10);
    for (i = 0; i < cnt; i++) {
        REQUIRE(pts_get[i].x == pts[i].x);
        REQUIRE(pts_get[i].y == pts[i].y);
    }

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Stroke width", "[capiStrokeWidth]")
{
    Tvg_Paint *paint = NULL;
    float stroke_set, stroke_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    stroke_set = 0.0f;
    REQUIRE(tvg_shape_set_stroke_width(paint, stroke_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_width(paint, &stroke_get) == TVG_RESULT_SUCCESS);
    REQUIRE(stroke_get == stroke_get);

    stroke_set = 5.0f;
    REQUIRE(tvg_shape_set_stroke_width(paint, stroke_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_width(paint, &stroke_get) == TVG_RESULT_SUCCESS);
    REQUIRE(stroke_get == stroke_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Stroke color", "[capiStrokeColor]")
{
    Tvg_Paint *paint = NULL;
    uint8_t r = 255, g = 255, b = 255, a = 255;
    uint8_t r_get, g_get, b_get, a_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_shape_set_stroke_color(paint, r, g, b, a) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_color(paint, &r_get, &g_get, &b_get, &a_get) == TVG_RESULT_SUCCESS);
    REQUIRE(r == r_get);
    REQUIRE(g == g_get);
    REQUIRE(b == b_get);
    REQUIRE(a == a_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Stroke dash", "[capiStrokeDash]")
{
    Tvg_Paint *paint = NULL;
    float dashPattern[2] = {20, 10};
    float * dashPattern_get;
    uint32_t cnt;
    uint32_t i;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_shape_set_stroke_dash(paint, dashPattern, 2) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_dash(paint, (const float**) &dashPattern_get, &cnt) == TVG_RESULT_SUCCESS);
    REQUIRE(cnt == 2);
    for (i = 0; i < cnt; i++) {
        REQUIRE(dashPattern_get[i] == dashPattern[i]);
    }

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Stroke cap", "[capiStrokeCap]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Stroke_Cap cap;
    Tvg_Stroke_Cap cap_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    cap = TVG_STROKE_CAP_ROUND;
    REQUIRE(tvg_shape_set_stroke_cap(paint, cap) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_cap(paint, &cap_get) == TVG_RESULT_SUCCESS);
    REQUIRE(cap == cap_get);

    cap = TVG_STROKE_CAP_BUTT;
    REQUIRE(tvg_shape_set_stroke_cap(paint, cap) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_cap(paint, &cap_get) == TVG_RESULT_SUCCESS);
    REQUIRE(cap == cap_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Stroke join", "[capiStrokeJoin]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Stroke_Join join;
    Tvg_Stroke_Join join_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    join = TVG_STROKE_JOIN_BEVEL;
    REQUIRE(tvg_shape_set_stroke_join(paint, join) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_join(paint, &join_get) == TVG_RESULT_SUCCESS);
    REQUIRE(join == join_get);

    join = TVG_STROKE_JOIN_MITER;
    REQUIRE(tvg_shape_set_stroke_join(paint, join) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_stroke_join(paint, &join_get) == TVG_RESULT_SUCCESS);
    REQUIRE(join == join_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Fill color", "[capiFillColor]")
{
    Tvg_Paint *paint = NULL;
    uint8_t r = 255, g = 255, b = 255, a = 255;
    uint8_t r_get, g_get, b_get, a_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_shape_set_fill_color(paint, r, g, b, a) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_fill_color(paint, &r_get, &g_get, &b_get, &a_get) == TVG_RESULT_SUCCESS);
    REQUIRE(r == r_get);
    REQUIRE(g == g_get);
    REQUIRE(b == b_get);
    REQUIRE(a == a_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Fill rule", "[capiFillRule]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Fill_Rule rule;
    Tvg_Fill_Rule rule_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    rule = TVG_FILL_RULE_EVEN_ODD;
    REQUIRE(tvg_shape_set_fill_rule(paint, rule) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_get_fill_rule(paint, &rule_get) == TVG_RESULT_SUCCESS);
    REQUIRE(rule == rule_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

