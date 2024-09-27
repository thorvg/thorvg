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

#include "tvgMergePath.h"
#include "tvgMath.h"
#include "tvgInlist.h"

#define MERGE_PATH_EPSILON 1e-3f

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
            intersection->nextBezier = new Bezier(bezier);
            intersection->prevBezier = new Bezier;
            intersection->nextBezier->split(intersection->t, *intersection->prevBezier);

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


    struct LineEquation // Ax + By + C = 0
    {
        float A{}, B{}, C{};

        LineEquation(const Point& pt1, const Point& pt2)
        {
            A = pt1.y - pt2.y;
            B = pt2.x - pt1.x;
            auto len = sqrtf(A * A + B * B);
            if (!tvg::zero(len)) {
                A /= len;
                B /= len;
                C = (pt1.x * pt2.y - pt1.y * pt2.x) / len;
            } else A = B = C = 0.0f;
        }

        LineEquation(const Line& line) : LineEquation(line.pt1, line.pt2) {}

        float distance(const Point& pt) const
        {
            return A * pt.x +  B * pt.y +  C;
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
    };

    /************************************************************************/
    /* Finding intersections functions                                      */
    /************************************************************************/

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
        return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
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
        return {std::min({dist1, dist2, 0.0f}), std::max({dist1, dist2, 0.0f})};
    }


    bool _intersection(const Line& line, float y, Point& intersection)
    {
        if (fabsf(line.pt1.y - line.pt2.y) < MERGE_PATH_EPSILON && fabsf(line.pt1.y - y) < MERGE_PATH_EPSILON) {
            intersection = line.pt1;
            return true;
        }

        if ((line.pt1.y > y - MERGE_PATH_EPSILON && line.pt2.y < y + MERGE_PATH_EPSILON) || (line.pt1.y < y + MERGE_PATH_EPSILON && line.pt2.y > y - MERGE_PATH_EPSILON)) {
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
        if (_area(convexHull.last(), p, start) > 0) convexHull.push(p);

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

            if (_intersection(line, fatLine.max, intersection)) updateT(intersection.x);
            if (_intersection(line, fatLine.min, intersection)) updateT(intersection.x);
            if (convexHull[i].y > fatLine.min && convexHull[i].y < fatLine.max) updateT(convexHull[i].x);
        }

        if (tMin == FLT_MAX && tMax == -FLT_MAX) return {-1.0f, -2.0f}; //no intersections
        return {tMin == FLT_MAX ? 0.0f : tMin, tMax == -FLT_MAX ? 1.0f : tMax};
    }


    void _splitBezier(const Bezier& in, const Range& tRange, Bezier& out)
    {
        if (tvg::zero(1.0f - tRange.min)) {
            out = in;
            return;
        }

        Bezier left, right = in;
        right.split(tRange.min, left);
        float t = tRange.length() / ( 1.0f - tRange.min);
        right.split(t, out);
    }


    void _intersections(Segment* segment1, Segment* segment2, Bezier& bezier1, Bezier& bezier2, Range range1, Range range2, uint32_t depth, uint32_t* count)
    {
        if (depth > 10) {
            TVGLOG("MERGE PATH", "Maximum nesting depth reached, aborting calculations.");
            return;
        }

        Range fatLine, range;
        auto converged1 = false, converged2 = false;
        auto iteration = 0;

        while (iteration++ < 20) {
            auto prevRange1 = range1;
            auto prevRange2 = range2;

            LineEquation lineEq1(bezier1.start, bezier1.end);
            fatLine = _fatLine(bezier1, lineEq1);
            range = _clip(bezier2, lineEq1, fatLine);
            if (range.min == -1.0f && range.max == -2.0f) return; //no intersection
            range2.rescale(range);
            if (range2.length() >= MERGE_PATH_EPSILON) _splitBezier(segment2->bezier, range2, bezier2);
            else converged2 = true;

            LineEquation lineEq2(bezier2.start, bezier2.end);
            fatLine = _fatLine(bezier2, lineEq2);
            range = _clip(bezier1, lineEq2, fatLine);
            if (range.min == -1.0f && range.max == -2.0f) return; // no intersection
            range1.rescale(range);
            if (range1.length() >= MERGE_PATH_EPSILON) _splitBezier(segment1->bezier, range1, bezier1);
            else converged1 = true;

            if (converged1 && converged2) break;

            //ranges changed less than 20% -> multiple intersectionDatas
            if (range1.length() > prevRange1.length() * 0.8f || range2.length() > prevRange2.length() * 0.8f) {
                if (range.length() > range.length()) {
                    auto tHalf = (range1.min + range1.max) * 0.5f;
                    Range range1a = {range1.min, tHalf};
                    Range range1b = {tHalf, range1.max};
                    if (range1a.length() < MERGE_PATH_EPSILON || range1b.length() < MERGE_PATH_EPSILON) break;

                    Bezier bezier1a, bezier1b;
                    _splitBezier(segment1->bezier, range1a, bezier1a);
                    _splitBezier(segment1->bezier, range1b, bezier1b);

                    Bezier bezier2Dup = bezier2;
                    _intersections(segment1, segment2, bezier1a, bezier2, range1a, range2, depth + 1, count);
                    _intersections(segment1, segment2, bezier1b, bezier2Dup, range1b, range2, depth + 1, count);
                    return;
                } else {
                    auto tHalf = (range2.min + range2.max) * 0.5f;
                    Range range2a = {range2.min, tHalf};
                    Range range2b = {tHalf, range2.max};
                    if (range2a.length() < MERGE_PATH_EPSILON || range2b.length() < MERGE_PATH_EPSILON) break;

                    Bezier bezier2a, bezier2b;
                    _splitBezier(segment2->bezier, range2a, bezier2a);
                    _splitBezier(segment2->bezier, range2b, bezier2b);

                    Bezier bezier1Dup = bezier1;
                    _intersections(segment1, segment2, bezier1, bezier2a, range1, range2a, depth + 1, count);
                    _intersections(segment1, segment2, bezier1Dup, bezier2b, range1, range2b, depth + 1, count);
                    return;
                }
            }
        }
        if (iteration >= 20) TVGLOG("MERGE PATH", "Loop terminated after reaching the maximum number of iterations. Results may be inaccurate.");

        Point p1 = segment1->bezier.at((range1.min + range1.max) * 0.5f);
        Point p2 = segment2->bezier.at((range2.min + range2.max) * 0.5f);
        if (fabsf(p1.x / p2.x - 1.0f) > MERGE_PATH_EPSILON || fabsf(p1.y / p2.y - 1.0f) > MERGE_PATH_EPSILON) {
            TVGLOG("MERGE PATH", "The found intersection point does not match between both curves within the specified precision.");
            return;
        }

        if (!count) {
            auto intersection1 = new Intersection(segment1, segment2, nullptr, (range1.min + range1.max) * 0.5f);
            auto intersection2 = new Intersection(segment2, segment1, intersection1, (range2.min + range2.max) * 0.5f);
            intersection1->pairedIntersection = intersection2;
            segment1->intersections.back(intersection1);
            segment2->intersections.back(intersection2);
        } else (*count)++;
    }


    void _intersections(Segment* segment1, Segment* segment2, uint32_t* count = nullptr)
    {
        Bezier bezier1 = segment1->bezier;
        Bezier bezier2 = segment2->bezier;
        _intersections(segment1, segment2, bezier1, bezier2, Range{0,1}, Range{0,1}, 0, count);
    }

    /************************************************************************/
    /* Merge functions                                                      */
    /************************************************************************/

    bool _inside(const Inlist<Segment>& segments1, const Inlist<Segment>& segments2)
    {
        Point start = segments1.head->bezier.start;
        auto intersection = segments1.head->intersections.head;
        if (intersection && intersection->t < MERGE_PATH_EPSILON) {
            //start is an intersection point, chose another one
            if (intersection->next) start = segments1.head->bezier.at(intersection->next->t * 0.5f);
            else start = segments1.head->bezier.at(0.5f);
        }

        auto bbox = _getBBox(segments2);
        if (start.x < bbox.min.x || start.x > bbox.max.x || start.y < bbox.min.y || start.y > bbox.max.y) return false;

        Point end = {start.x > bbox.max.x ? bbox.min.x - 50.0f : bbox.max.x + 50.0f, start.y};
        Bezier line = {start, start, end, end};

        uint32_t counter = 0;
        for (auto segment = segments2.head; segment != nullptr; segment = segment->next) {
            auto lineSegment = Segment(line, nullptr);
            _intersections(segment, &lineSegment, &counter);
            //TODO: check & remove intersections at segments connecting points
        }
        return counter % 2 == 1;
    }


    void _markInOut(Inlist<Segment>& segments1, Inlist<Segment>& segments2, bool revert)
    {
        bool inside = _inside(segments1, segments2);
        bool inOut = revert ? !inside : inside;
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
                _intersections(segment1, segment2);
            }
        }

        for (auto segment1 = segments1.head; segment1 != nullptr; segment1 = segment1->next) {
            segment1->sort();
            segment1->split();
        }

        for (auto segment2 = segments2.head; segment2 != nullptr; segment2 = segment2->next) {
            segment2->sort();
            segment2->split();
        }

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
                    _add(intersection->nextBezier, cmds, pts, moveTo);
                    moveTo = false;

                    if (!intersection->next) {
                        auto segment = intersection->segment->nextSegment();
                        while (segment && segment->intersections.empty()) {
                            _add(&segment->bezier, cmds, pts, false);
                            segment = segment->nextSegment();
                        }
                        intersection = segment->intersections.head;
                        _add(intersection->prevBezier, cmds, pts, moveTo);
                    } else intersection = intersection->next;
                } else {
                    _addRevert(intersection->prevBezier, cmds, pts, moveTo);
                    moveTo = false;

                    if (!intersection->prev) {
                        auto segment = intersection->segment->prevSegment();
                        while (segment->intersections.empty()) {
                            _addRevert(&segment->bezier, cmds, pts, false);
                            segment = segment->prevSegment();
                        }
                        intersection = segment->intersections.tail;
                        _addRevert(intersection->nextBezier, cmds, pts, moveTo);
                    } else intersection = intersection->prev;
                }

                if (!intersection) break;
                intersection->visited = true;
                intersection = intersection->pairedIntersection;
            }
            moveTo = true;
        }

        return true;
    }


    void _create(const Array<PathCommand>& cmds, const Array<Point>& pts, Inlist<Segment>& segments)
    {
        Point start{};
        int i = 0;

        for (const auto& cmd: cmds) {
            if (cmd == PathCommand::MoveTo) {
                start = pts[i++];
            } else if (cmd == PathCommand::CubicTo) {
                segments.back(new Segment({start, pts[i], pts[i + 1], pts[i + 2]}, &segments));
                start = pts[i + 2];
                i += 3;
            } else if (cmd == PathCommand::LineTo) {  //TODO: optimize for shapes with lines only?
                segments.back(new Segment({start, start, pts[i], pts[i]}, &segments));
                start = pts[i++];
            } else if (cmd == PathCommand::Close) {
                //TODO: handle multicontour shapes
                break;
            }
        }
    }


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


} //namespace


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

namespace tvg
{

bool mergePath(const Array<PathCommand>& cmds1, const Array<Point>& pts1, const Array<PathCommand>& cmds2, const Array<Point>& pts2, Array<PathCommand>& mergedCmds, Array<Point>& mergedPts, MergeMode mode)
{
    if (cmds1.empty() || pts1.empty() || cmds2.empty() || pts2.empty()) return false;

    auto bbox1 = _getBBox(pts1);
    auto bbox2 = _getBBox(pts2);
    if (!bbox1.intersect(bbox2)) {
        switch (mode) {
            case MergeMode::Add: {
                mergedCmds.push(cmds1);
                mergedCmds.push(cmds2);
                mergedPts.push(pts1);
                mergedPts.push(pts2);
                return true;
            }
            case MergeMode::Subtract: {
                mergedCmds.push(cmds1);
                mergedPts.push(pts1);
                return true;
            }
            case MergeMode::Intersect: {
                return true;
            }
            default:
                return false;
        }
    }

    Inlist<Segment> segments1, segments2;
    _create(cmds1, pts1, segments1);
    _create(cmds2, pts2, segments2);

    _merge(segments1, segments2, mergedCmds, mergedPts, mode);

    segments1.free();
    segments2.free();

    return true;
}

}