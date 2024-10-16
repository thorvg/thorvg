/*
 * Copyright (c) 2024 the ThorVG project. All rights capacity().

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

#include "tvgLottieMergePath.h"
#include "tvgMath.h"
#include "tvgInlist.h"
#include <algorithm>

#define MERGE_PATH_EPSILON 1e-4f

#define FOR_EACH_SEGMENT(segments_list, action) \
for (auto segment = segments_list.head; segment != nullptr; segment = segment->next) { \
segment->action; \
}

/************************************************************************/
/* Comparison functions                                                 */
/************************************************************************/

bool _zero(float f)
{
    return fabsf(f) < MERGE_PATH_EPSILON;
}


bool _equal(const Point& p1, const Point& p2)
{
    return tvg::length(p1 - p2) / std::max(tvg::length(p1), tvg::length(p2)) < MERGE_PATH_EPSILON;
}


bool _equal2(const Point& p1, const Point& p2)
{
    return tvg::length(p1 - p2) / std::max(tvg::length(p1), tvg::length(p2)) < 1e-3;
}

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

namespace {
    struct Segment;

    struct Intersection
    {
        INLIST_ITEM(Intersection);

        Segment* segment;
        Segment* pairedSegment;
        Intersection* pairedIntersection;
        float t;
        Bezier* prevBezier = nullptr;
        Bezier* nextBezier = nullptr;
        bool inOut {};
        bool visited {};

        Intersection(Segment* segment, Segment* pairedSegment, Intersection* paired, float t) :
        segment{segment}, pairedSegment{pairedSegment}, pairedIntersection{paired}, t{t} {}
    };


    struct Segment
    {
        INLIST_ITEM(Segment);

        Bezier bezier;
        const Inlist<Segment>* parent;
        Inlist<Intersection> intersections;

        Segment(const Bezier& b, const Inlist<Segment>* parent) : bezier{b}, parent{parent} {}

        ~Segment()
        {
            if (auto intersection = intersections.head) {
                while (intersection) {
                    delete intersection->prevBezier;
                    intersection = intersection->next;
                }
                delete intersections.tail->nextBezier;
                intersections.free();
            }
        }

        void sort()
        {
            intersections.sort<CompareIntersections>();
        }

        void split()
        {
            if (!intersections.tail) return;

            auto intersection = intersections.tail;
            if (1.0f - intersection->t < MERGE_PATH_EPSILON) {
                intersection->nextBezier = nullptr;
                intersection->prevBezier = new Bezier(bezier);
            } else {
                intersection->nextBezier = new Bezier(bezier);
                intersection->prevBezier = new Bezier;
                intersection->nextBezier->split(intersection->t, *intersection->prevBezier);
            }

            while ((intersection = intersection->prev)) {
                intersection->nextBezier = intersection->next->prevBezier;
                intersection->prevBezier = new Bezier;
                intersection->nextBezier->split(intersection->t / intersection->next->t, *intersection->prevBezier);
            }
        }

        const Segment* nextSegment() const
        {
            return next ? next : parent->head;
        }

        const Segment* prevSegment() const
        {
            return prev ? prev : parent->tail;
        }

    private:
        struct CompareIntersections
        {
            bool operator()(const Intersection& a, const Intersection& b) const
            {
                return a.t < b.t;
            }
        };
    };


    struct LineEquation
    {
        float len{};
        Point start{}, delta{};

        LineEquation(const Point& pt1, const Point& pt2)
        {
            start = pt1;
            delta = pt2 - pt1;
            len = hypotf(delta.x, delta.y);
        }

        LineEquation(const Line& line) : LineEquation(line.pt1, line.pt2) {}

        float distance(const Point& pt) const
        {
            if (len == 0.0f) return 0.0f;

            auto ptLen = pt - start;
            auto cross = delta.x * ptLen.y - delta.y * ptLen.x;

            return cross / len;
        }
    };


    struct Range
    {
        float min = 0.0f;
        float max = 1.0f;

        float length() const
        {
            return max - min;
        }

        void rescale(const Range& range)
        {
            auto len = length();
            max = min + len * range.max;
            min = min + len * range.min;
        }
    };


    struct BoundingBox
    {
        Point min{};
        Point max{};

        bool intersect(const BoundingBox& bbox) const
        {
            if (max.x < bbox.min.x || min.x > bbox.max.x || max.y < bbox.min.y || min.y > bbox.max.y) return false;
            return true;
        }

        bool intersect(const Point& p) const
        {
            if (p.x < min.x || p.x > max.x || p.y < min.y || p.y > max.y) return false;
            return true;
        }
    };


    /************************************************************************/
    /* Finding intersections functions                                      */
    /************************************************************************/

    BoundingBox _getBBox(const Array<Point>& pts)
    {
        if (pts.empty()) return {};

        BoundingBox bbox = {{FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX}};

        for (const auto& pt: pts) {
            if (pt.x < bbox.min.x) bbox.min.x = pt.x;
            else if (pt.x > bbox.max.x) bbox.max.x = pt.x;
            if (pt.y < bbox.min.y) bbox.min.y = pt.y;
            else if (pt.y > bbox.max.y) bbox.max.y = pt.y;
        }

        return bbox;
    }


    BoundingBox _getBBox(const Inlist<Segment>& segments)
    {
        if (segments.empty()) return {};

        BoundingBox bbox = {{FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX}};

        for (auto segment = segments.head; segment != nullptr; segment = segment->next) {
            const auto& bezier = segment->bezier;
            bbox.min.x = std::min({bbox.min.x, bezier.start.x, bezier.ctrl1.x, bezier.ctrl2.x, bezier.end.x});
            bbox.min.y = std::min({bbox.min.y, bezier.start.y, bezier.ctrl1.y, bezier.ctrl2.y, bezier.end.y});
            bbox.max.x = std::max({bbox.max.x, bezier.start.x, bezier.ctrl1.x, bezier.ctrl2.x, bezier.end.x});
            bbox.max.y = std::max({bbox.max.y, bezier.start.y, bezier.ctrl1.y, bezier.ctrl2.y, bezier.end.y});
        }

        return bbox;
    }


    float _area(const Point& p1, const Point& p2, const Point& p3)
    {
        auto p21 = p2 - p1;
        auto p31 = p3 - p1;
        return p21.x * p31.y - p21.y * p31.x;
    }


    bool _compare(const Point& start, const Point& p1, const Point& p2)
    {
        if (tvg::zero(p1.x - start.x) && tvg::zero(p1.y - start.y)) return true;
        if (tvg::zero(p2.x - start.x) && tvg::zero(p2.y - start.y)) return false;

        auto area = _area(start, p1, p2);
        if (tvg::zero(area)) return tvg::zero(p1.x - p2.x) ? p1.y >= p2.y : p1.x >= p2.x;

        return area > 0.0f;
    }


    Range _fatLine(const Bezier& bezier, const LineEquation& lineEq)
    {
        auto dist1 = lineEq.distance(bezier.ctrl1);
        auto dist2 = lineEq.distance(bezier.ctrl2);

        //http://nishitalab.org/user/nis/cdrom/cad/CAGD90Curve.pdf
        auto factor = dist1 * dist2 > 0.0f ? 0.75f : 0.444444f;
        if (dist1 > dist2) std::swap(dist1, dist2);

        return {std::min(factor * dist1, 0.0f), std::max(factor * dist2, 0.0f)};
    }


    bool _intersect(const Line& line, float y, Point& intersection)
    {
        if (_zero(line.pt1.y - line.pt2.y) && _zero(line.pt1.y - y)) { //TODO: overlapping?
            intersection = line.pt1;
            return true;
        }

        if ((line.pt1.y >= y && line.pt2.y <= y) || (line.pt1.y <= y && line.pt2.y >= y)) {
            intersection.x = line.pt1.x + (y - line.pt1.y) * (line.pt2.x - line.pt1.x) / (line.pt2.y - line.pt1.y);
            intersection.y = y;
            return intersection.x >= 0.0f && intersection.x <= 1.0f;
        }

        return false;
    }


    Array<Point> _convexHull(Array<Point>& points)
    {
        Array<Point> convexHull(4);

        if (tvg::zero(_area(points[0], points[1], points[2])) && tvg::zero(_area(points[1], points[2], points[3]))) {
            convexHull.push(points[0]);
            convexHull.push(points[3]);
            return convexHull;
        }

        auto startIndex = 0;
        Point start = points[0];
        for (auto i = 1; i < 4; i++) {
            if (points[i].y > start.y || (tvg::zero(points[i].y - start.y) && points[i].x < start.x)) {
                startIndex = i;
                start = points[i];
            }
        }
        points[startIndex] = points[0];
        points[0] = start;

        std::sort(points.begin() + 1, points.end(), [start](const Point& p1, const Point& p2) {
            return _compare(start, p1, p2);
        });

        convexHull.push(points[0]);
        convexHull.push(points[1]);
        for (int i = 2; i < 4; ++i) {
            auto next = points[i];
            auto p = convexHull.last();
            convexHull.pop();

            while (!convexHull.empty() && _area(convexHull.last(), p, next) <= 0.0f) {
                p = convexHull.last();
                convexHull.pop();
            }
            convexHull.push(p);
            convexHull.push(next);
        }

        auto p = convexHull.last();
        convexHull.pop();
        if (_area(convexHull.last(), p, start) > 0.0f) convexHull.push(p);

        return convexHull;
    }


    Range _clip(const Bezier& bezier, const LineEquation& fatLineEq, const Range& fatLine)
    {
        Array<Point> distances(4);
        distances.push({0.0f, fatLineEq.distance(bezier.start)});
        distances.push({1.0f/3.0f, fatLineEq.distance(bezier.ctrl1)});
        distances.push({2.0f/3.0f, fatLineEq.distance(bezier.ctrl2)});
        distances.push({1.0f, fatLineEq.distance(bezier.end)});

        auto convexHull = _convexHull(distances);

        auto tMin = FLT_MAX;
        auto tMax = -FLT_MAX;
        auto updateT = [&](float x) {
            if (x < tMin) tMin = x;
            if (x > tMax) tMax = x;
        };

        Point intersection{};
        for (auto i = 0; i < convexHull.count; ++i) {
            Line line = {convexHull[i], convexHull[(i + 1) % convexHull.count]};

            if (_intersect(line, fatLine.max, intersection)) updateT(intersection.x);
            if (_intersect(line, fatLine.min, intersection)) updateT(intersection.x);
            if (convexHull[i].y > fatLine.min && convexHull[i].y < fatLine.max) updateT(convexHull[i].x);
        }

        if (tMin == FLT_MAX && tMax == -FLT_MAX) return {-1.0f, -2.0f}; //no intersections
        return {tMin == FLT_MAX ? 0.0f : tMin, tMax == -FLT_MAX ? 1.0f : tMax};
    }


    void _splitBezier(const Bezier& in, const Range& tRange, Bezier& out)
    {
        if (_zero(tRange.max)) {
            out.start = out.ctrl1 = out.ctrl2 = out.end = in.start;
            return;
        }
        if (_zero(1.0f - tRange.min)) {
            out.start = out.ctrl1 = out.ctrl2 = out.end = in.end;
            return;
        }
        if (_zero(tRange.length())) {
            out.start = out.ctrl1 = out.ctrl2 = out.end = in.at((tRange.min + tRange.max) * 0.5f);
            return;
        }

        Bezier right = in;
        right.split(tRange.min, out);
        float t = tRange.length() / ( 1.0f - tRange.min);
        right.split(t, out);
    }


    float _newton(const Point& pIntersect, const Bezier& bezier, float t)
    {
        auto p = bezier.at(t);
        auto pPrim = bezier.derivative(t);
        auto pBis = bezier.secondDerivative(t);

        auto f = dot(p - pIntersect, pPrim);
        auto fPrim = dot(p - pIntersect, pBis) + dot(pPrim, pPrim);

        return t - f / fPrim;
    }


    void _intersect(Segment* segment1, Segment* segment2, Bezier& bezier1, Bezier& bezier2, Range range1, Range range2, uint32_t depth, uint32_t* count)
    {
        if (depth > 20) {
            TVGLOG("MERGE PATH", "Maximum nesting depth reached, aborting calculations.");
            return;
        }

        auto converged = [](const Range& range) -> bool {
            return range.length() < MERGE_PATH_EPSILON;
        };

        auto equal = [](const Bezier& bezier1, const Bezier& bezier2) -> bool {
            return (_equal(bezier1.start, bezier2.start) && _equal(bezier1.ctrl1, bezier2.ctrl1) && _equal(bezier1.ctrl2, bezier2.ctrl2) && _equal(bezier1.end, bezier2.end)) ||
                   (_equal(bezier1.start, bezier2.end) && _equal(bezier1.ctrl1, bezier2.ctrl2) && _equal(bezier1.ctrl2, bezier2.ctrl1) && _equal(bezier1.end, bezier2.start));
        };

        Range fatLine, range;
        auto iteration = 0;
        while (iteration < 20 && (iteration == 0 || !converged(range1) || !converged(range2))) {
            auto prevRange1 = range1;
            auto prevRange2 = range2;

            LineEquation lineEq1(bezier1.start, bezier1.end);
            fatLine = _fatLine(bezier1, lineEq1);
            range = _clip(bezier2, lineEq1, fatLine);
            if (range.min == -1.0f && range.max == -2.0f) return; //no intersection
            range2.rescale(range);
            _splitBezier(segment2->bezier, range2, bezier2);
            if (iteration > 0 && converged(range2)) {
                break;
            }

            LineEquation lineEq2(bezier2.start, bezier2.end);
            fatLine = _fatLine(bezier2, lineEq2);
            range = _clip(bezier1, lineEq2, fatLine);
            if (range.min == -1.0f && range.max == -2.0f) return; //no intersection
            range1.rescale(range);
            _splitBezier(segment1->bezier, range1, bezier1);
            if (iteration > 0 && converged(range1)) {
                break;
            }

            //ranges changed less than 10% -> possible multiple intersections
            if (range1.length() > prevRange1.length() * 0.9f && range2.length() > prevRange2.length() * 0.9f) {
                if (equal(bezier1, bezier2)) {
                    TVGLOG("MERGE PATH", "The curves overlap - should be handled accordingly."); //TODO
                    return;
                }

                if (range1.length() > range2.length()) {
                    auto tHalf = (range1.min + range1.max) * 0.5f;
                    Range range1a = {range1.min, tHalf};
                    Range range1b = {tHalf, range1.max};
                    if (converged(range1a) || converged(range1b)) break;

                    Bezier bezier1a, bezier1b;
                    bezier1b = bezier1;
                    bezier1b.split(tHalf, bezier1a);
                    Bezier bezier2copy = bezier2;

                    _intersect(segment1, segment2, bezier1a, bezier2, range1a, range2, depth + 1, count);
                    _intersect(segment1, segment2, bezier1b, bezier2copy, range1b, range2, depth + 1, count);
                    return;
                } else {
                    auto tHalf = (range2.min + range2.max) * 0.5f;
                    Range range2a = {range2.min, tHalf};
                    Range range2b = {tHalf, range2.max};
                    if (converged(range2a) || converged(range2b)) break;

                    Bezier bezier2a, bezier2b;
                    bezier2b = bezier2;
                    bezier2b.split(tHalf, bezier2a);
                    Bezier bezier1copy = bezier1;

                    _intersect(segment1, segment2, bezier1, bezier2a, range1, range2a, depth + 1, count);
                    _intersect(segment1, segment2, bezier1copy, bezier2b, range1, range2b, depth + 1, count);
                    return;
                }
            }

            iteration++;
        }

        if (iteration >= 20) TVGLOG("MERGE PATH", "Loop terminated after reaching the maximum number of iterations. Results may be inaccurate.");

        auto t1 = (range1.min + range1.max) * 0.5f;
        auto t2 = (range2.min + range2.max) * 0.5f;

        if (converged(range1) && !converged(range2)) {
            for (auto i = 0; i < 20; ++i) {
                auto t = _newton(bezier1.at(t1), segment2->bezier, t2);
                if (tvg::zero(t - t2)) break;
                t2 = std::min(range2.max, std::max(range2.min, t));
            }
        } else if (!converged(range1) && converged(range2)) {
            for (auto i = 0; i < 20; ++i) {
                auto t = _newton(bezier2.at(t2), segment1->bezier, t1);
                if (tvg::zero(t - t1)) break;
                t1 = std::min(range1.max, std::max(range1.min, t));
            }
        }

        Point p1 = segment1->bezier.at(t1);
        Point p2 = segment2->bezier.at(t2);

        if (!_equal2(p1, p2)) {
            TVGLOG("MERGE PATH", "The found intersection point does not match between both curves within the specified precision.");
            return;
        }

        if (!count) {
            auto intersection1 = new Intersection(segment1, segment2, nullptr, t1);
            auto intersection2 = new Intersection(segment2, segment1, intersection1, t2);
            intersection1->pairedIntersection = intersection2;
            segment1->intersections.back(intersection1);
            segment2->intersections.back(intersection2);
        } else {
            auto intersection1 = new Intersection(segment1, segment2, nullptr, t1);
            segment1->intersections.back(intersection1);
            (*count)++;
        }
    }


    void _intersect(Segment* segment1, Segment* segment2, uint32_t* count = nullptr)
    {
        Bezier bezier1 = segment1->bezier;
        Bezier bezier2 = segment2->bezier;
        _intersect(segment1, segment2, bezier1, bezier2, Range{0.0f,1.0f}, Range{0.0f,1.0f}, 0, count);
    }


    /************************************************************************/
    /* Merge functions                                                      */
    /************************************************************************/

    uint32_t _unique(Segment& segment)
    {
        uint32_t removed = 0;

        if (auto intersection = segment.intersections.head) {
            while (intersection && intersection->next) {
                auto pNext = segment.bezier.at(intersection->next->t);
                auto p = segment.bezier.at(intersection->t);

                auto nextIntersection = intersection = intersection->next;
                if (_equal(p, pNext) ) {
                    segment.intersections.remove(intersection);
                    removed++;
                }
                intersection = nextIntersection;
            }
        }

        return removed;
    }


    uint32_t _unique(const Inlist<Segment> segments)
    {
        uint32_t removed = 0;
        auto segment = segments.head;
        auto prevSegment = segment->prevSegment();

        while (segment) {
            if (prevSegment != segment && prevSegment->intersections.tail && segment->intersections.head) {
                if (_zero(prevSegment->intersections.tail->t - 1.0f) && _zero(segment->intersections.head->t)) {
                    segment->intersections.remove(segment->intersections.head);
                    removed++;
                }
            }

            if (auto intersection = segment->intersections.head) {
                while (intersection && intersection->next) {
                    auto pNext = segment->bezier.at(intersection->next->t);
                    auto p = segment->bezier.at(intersection->t);
                    auto qNext = intersection->next->pairedIntersection->segment->bezier.at(intersection->next->pairedIntersection->t);
                    auto q = intersection->pairedIntersection->segment->bezier.at(intersection->pairedIntersection->t);

                    auto nextIntersection = intersection = intersection->next;
                    if (_equal(pNext, p) && _equal(qNext, q)) {
                        intersection->pairedSegment->intersections.remove(intersection->pairedIntersection);
                        segment->intersections.remove(intersection);
                        removed++;
                    }
                    intersection = nextIntersection;
                }
            }

            prevSegment = segment;
            segment = segment->next;
        }

        return removed;
    }


    bool _inside(const Inlist<Segment>& segments1, const Inlist<Segment>& segments2, bool& skip)
    {
        Point start = segments1.head->bezier.start;
        if (auto intersection = segments1.head->intersections.head) {
            if (intersection->t < MERGE_PATH_EPSILON) {
                skip = true;
                if (intersection->next) start = segments1.head->bezier.at((intersection->next->t + intersection->t) * 0.5f);
                else start = segments1.head->bezier.ctrl1;
            }
        }

        auto bbox = _getBBox(segments2);
        if (!bbox.intersect(start)) return false;

        Point end = {start.x > bbox.max.x ? bbox.min.x - 50.0f : bbox.max.x + 50.0f, start.y};

        uint32_t counter = 0;
        Bezier line = {start, end, end, end};
        Segment lineSeg(line, nullptr);
        for (auto segment = segments2.head; segment != nullptr; segment = segment->next) {
            _intersect(&lineSeg, segment, &counter);
        }

        if (counter > 0) counter -= _unique(lineSeg);
        return counter % 2 == 1;
    }


    void _markInOut(Inlist<Segment>& segments1, Inlist<Segment>& segments2, bool revert)
    {
        bool skip = false;
        bool inside = _inside(segments1, segments2, skip);
        bool inOut = revert ? !inside : inside;
        if (skip) inOut = !inOut;
        for (auto segment = segments1.head; segment != nullptr; segment = segment->next) {
            for (auto intersection = segment->intersections.head; intersection != nullptr; intersection = intersection->next) {
                intersection->inOut = inOut;
                inOut = !inOut;
            }
        }
    }


    void _add(const Bezier* bezier, Array<PathCommand>& cmds, Array<Point>& pts, bool moveTo)
    {
        if (moveTo) {
            if (!cmds.empty()) cmds.push(PathCommand::Close);
            cmds.push(PathCommand::MoveTo);
            pts.push(bezier->start);
        }
        cmds.push(PathCommand::CubicTo);
        pts.push(bezier->ctrl1);
        pts.push(bezier->ctrl2);
        pts.push(bezier->end);
    }


    void _addRevert(const Bezier* bezier, Array<PathCommand>& cmds, Array<Point>& pts, bool moveTo)
    {
        if (moveTo) {
            if (!cmds.empty()) cmds.push(PathCommand::Close);
            cmds.push(PathCommand::MoveTo);
            pts.push(bezier->end);
        }
        cmds.push(PathCommand::CubicTo);
        pts.push(bezier->ctrl2);
        pts.push(bezier->ctrl1);
        pts.push(bezier->start);
    }


    Intersection* _unvisited(const Inlist<Segment>& segments)
    {
        for (auto segment = segments.head; segment != nullptr; segment = segment->next) {
            for (auto intersection = segment->intersections.head; intersection != nullptr; intersection = intersection->next) {
                if (!intersection->visited) return intersection;
            }
        }
        return nullptr;
    }


    bool _merge(Inlist<Segment>& segments1, Inlist<Segment>& segments2, Array<PathCommand>& cmds, Array<Point>& pts, MergeMode mode)
    {
        if (segments1.empty() || segments2.empty()) return false;

        for (auto segment1 = segments1.head; segment1 != nullptr; segment1 = segment1->next) {
            for (auto segment2 = segments2.head; segment2 != nullptr; segment2 = segment2->next) {
                _intersect(segment1, segment2);
            }
        }

        FOR_EACH_SEGMENT(segments1, sort());
        _unique(segments1);
        FOR_EACH_SEGMENT(segments1, split());

        FOR_EACH_SEGMENT(segments2, sort());
        _unique(segments2);
        FOR_EACH_SEGMENT(segments2, split());

        bool inOut1{}, inOut2{};
        if (mode == MergeMode::Add) inOut1 = inOut2 = false;
        else if (mode == MergeMode::Intersect) inOut1 = inOut2 = true;
        else if (mode == MergeMode::Subtract) {
            inOut1 = false;
            inOut2 = true;
        }
        _markInOut(segments1, segments2, inOut1);
        _markInOut(segments2, segments1, inOut2);

        bool moveTo = true;
        while (auto intersection = _unvisited(segments1)) {
            while (!intersection->visited) {
                intersection->visited = true;

                if (intersection->inOut) {
                    if (intersection->nextBezier) {
                        _add(intersection->nextBezier, cmds, pts, moveTo);
                        moveTo = false;
                    }

                    if (!intersection->next) {
                        auto segment = intersection->segment->nextSegment();
                        while (segment && segment->intersections.empty()) {
                            _add(&segment->bezier, cmds, pts, moveTo);
                            segment = segment->nextSegment();
                        }
                        intersection = segment->intersections.head;
                        _add(intersection->prevBezier, cmds, pts, moveTo);
                    } else intersection = intersection->next;
                } else {
                    if (intersection->prevBezier) _addRevert(intersection->prevBezier, cmds, pts, moveTo);
                    moveTo = false;

                    if (!intersection->prev) {
                        auto segment = intersection->segment->prevSegment();
                        while (segment->intersections.empty()) {
                            _addRevert(&segment->bezier, cmds, pts, false);
                            segment = segment->prevSegment();
                        }
                        intersection = segment->intersections.tail;

                        if (intersection->nextBezier) _addRevert(intersection->nextBezier, cmds, pts, moveTo);
                    } else intersection = intersection->prev;
                }

                if (!intersection) break;
                intersection->visited = true;
                intersection = intersection->pairedIntersection;
            }
            moveTo = true;
        }
        if (cmds.count > 0) cmds.push(PathCommand::Close);
        return true;
    }


    void _create(const Array<PathCommand>& cmds, const Array<Point>& pts, Inlist<Segment>& segments)
    {
        int i = 0;
        for (const auto& cmd: cmds) {
            if (cmd == PathCommand::MoveTo) {
                ++i;
            } else if (cmd == PathCommand::CubicTo) {
                segments.back(new Segment({pts[i - 1], pts[i], pts[i + 1], pts[i + 2]}, &segments));
                i += 3;
            } else if (cmd == PathCommand::LineTo) {  //TODO: optimize for shapes with lines only?
                segments.back(new Segment({pts[i - 1], pts[i - 1], pts[i], pts[i]}, &segments));
                ++i;
            } else if (cmd == PathCommand::Close) {
                //TODO: handle multicontour shapes
                break;
            }
        }
    }

} //namespace


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

namespace tvg
{

    bool mergePath(Array<PathCommand>& cmds1, Array<Point>& pts1, const Array<PathCommand>& cmds2, const Array<Point>& pts2, MergeMode mode)
    {
        if (cmds1.empty() || pts1.empty() || cmds2.empty() || pts2.empty()) return false;

        //TODO: handle one inside the other; handle touched
        auto bbox1 = _getBBox(pts1);
        auto bbox2 = _getBBox(pts2);
        if (!bbox1.intersect(bbox2)) {
            switch (mode) {
                case MergeMode::Add: {
                    cmds1.push(cmds2);
                    pts1.push(pts2);
                    return true;
                }
                case MergeMode::Subtract: {
                    return true;
                }
                case MergeMode::Intersect: {
                    cmds1.clear();
                    pts1.clear();
                    return true;
                }
                default:
                    return false;
            }
        }

        Inlist<Segment> segments1, segments2;
        _create(cmds1, pts1, segments1);
        _create(cmds2, pts2, segments2);

        cmds1.clear();
        pts1.clear();
        _merge(segments1, segments2, cmds1, pts1, mode);

        segments1.free();
        segments2.free();

        return true;
    }

} //namespace tvg