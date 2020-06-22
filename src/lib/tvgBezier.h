/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_BEZIER_H_
#define _TVG_BEZIER_H_

namespace tvg
{

struct Bezier
{
    Point start;
    Point ctrl1;
    Point ctrl2;
    Point end;
};

void bezSplit(const Bezier&cur, Bezier& left, Bezier& right);
float bezLength(const Bezier& cur);
void bezSplitLeft(Bezier& cur, float at, Bezier& left);
float bezAt(const Bezier& bz, float at);
void bezSplitAt(const Bezier& cur, float at, Bezier& left, Bezier& right);

}

#endif //_TVG_BEZIER_H_