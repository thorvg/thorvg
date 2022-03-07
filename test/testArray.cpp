/*
 * Copyright (c) 2021 - 2022 Samsung Electronics Co., Ltd. All rights reserved.

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

#include <thorvg.h>
#include "catch.hpp"

#include "../src/lib/tvgArray.h"

using namespace tvg;

class TestType {
public:
    int test;
};

TEST_CASE("Push and Pop from array", "[tvgArray]")
{
    Array<TestType*> array;
    REQUIRE(array.count == 0);
    
    // Test push
    TestType* ptrs[5];
    for (int i = 0; i < 5; i++) {
        TestType* object = new TestType();
        REQUIRE(object);
        array.push(object);
        ptrs[i] = object;
    }
    REQUIRE(array.count == 5);

    // Test pop
    int remove_count = 0;
    while (array.count) {
        array.pop();
        remove_count++;
    }
    REQUIRE(array.count == 0);
    REQUIRE(remove_count == 5);

    // Cleanup
    for (int i = 0; i < 5; i++) delete ptrs[i];
}

TEST_CASE("Clear array", "[tvgArray]")
{
    Array<TestType*> array;
    REQUIRE(array.count == 0);

    TestType* ptrs[5];
    for (int i = 0; i < 5; i++) {
        TestType* object = new TestType();
        REQUIRE(object);
        array.push(object);
        ptrs[i] = object;
    }
    REQUIRE(array.count == 5);

    // Test clear
    array.clear();
    REQUIRE(array.count == 0);

    // Cleanup
    for (int i = 0; i < 5; i++) delete ptrs[i];
}

TEST_CASE("Remove from array", "[tvgArray]")
{
    Array<TestType*> array;
    REQUIRE(array.count == 0);

    TestType* ptrs[7];
    for (int i = 0; i < 7; i++) {
        TestType* object = new TestType();
        REQUIRE(object);
        array.push(object);
        ptrs[i] = object;
    }
    REQUIRE(array.count == 7);

    // Remove first one and check elements have shuffled down
    REQUIRE(array.remove(ptrs[0]) == true);
    REQUIRE(array.count == 6);
    REQUIRE(array.data[0] == ptrs[1]);
    REQUIRE(array.data[1] == ptrs[2]);
    REQUIRE(array.data[2] == ptrs[3]);
    REQUIRE(array.data[3] == ptrs[4]);
    REQUIRE(array.data[4] == ptrs[5]);
    REQUIRE(array.data[5] == ptrs[6]);

    // Remove one near the middle and check elements have shuffled
    REQUIRE(array.remove(ptrs[3]) == true);
    REQUIRE(array.count == 5);
    REQUIRE(array.data[0] == ptrs[1]);
    REQUIRE(array.data[1] == ptrs[2]);
    REQUIRE(array.data[2] == ptrs[4]);
    REQUIRE(array.data[3] == ptrs[5]);
    REQUIRE(array.data[4] == ptrs[6]);

    // Remove last one and check elements are ok
    REQUIRE(array.remove(ptrs[6]) == true);
    REQUIRE(array.count == 4);
    REQUIRE(array.data[0] == ptrs[1]);
    REQUIRE(array.data[1] == ptrs[2]);
    REQUIRE(array.data[2] == ptrs[4]);
    REQUIRE(array.data[3] == ptrs[5]);

    // Remove non-existant elemnt
    TestType* object2 = new TestType();
    REQUIRE(array.remove(object2) == false);
    delete object2;

    // Cleanup
    for (int i = 0; i < 7; i++) delete ptrs[i];
}