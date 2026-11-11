/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef THORVG_TEST_FRAMEWORK_H
#define THORVG_TEST_FRAMEWORK_H

#include <cmath>
#include <limits>
#include <memory>
#include <ostream>
#include <vector>

#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#include "doctest/doctest.h"

#undef TEST_CASE
#define TEST_CASE(name, tags) DOCTEST_TEST_CASE((name) * doctest::test_suite(tags))

class Approx
{
public:
    template<typename T>
    explicit Approx(const T& value) : expected(static_cast<double>(value))
    {
    }

    template<typename T>
    Approx& margin(const T& value)
    {
        absolute = static_cast<double>(value);
        return *this;
    }

    template<typename T>
    friend bool operator==(const T& lhs, const Approx& rhs)
    {
        auto value = static_cast<double>(lhs);
        auto relative = static_cast<double>(std::numeric_limits<float>::epsilon()) * 100.0 * std::fabs(std::isinf(rhs.expected) ? 0.0 : rhs.expected);
        return ((value + rhs.absolute >= rhs.expected) && (rhs.expected + rhs.absolute >= value)) || ((value + relative >= rhs.expected) && (rhs.expected + relative >= value));
    }

    template<typename T>
    friend bool operator==(const Approx& lhs, const T& rhs)
    {
        return rhs == lhs;
    }

    friend std::ostream& operator<<(std::ostream& out, const Approx& rhs)
    {
        return out << "Approx( " << rhs.expected << " )";
    }

private:
    double expected;
    double absolute = 0.0;
};

#endif  // THORVG_TEST_FRAMEWORK_H
