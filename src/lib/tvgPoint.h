
#include <thorvg.h>

#include <cmath>

namespace tvg
{

inline bool operator==(const Point &p1, const Point &p2)
{
    return p1.x == p2.x && p1.y == p2.y;
}

inline bool operator!=(const Point &p1, const Point &p2)
{
    return p1.x != p2.x || p1.y != p2.y;
}

inline Point operator-(const Point &p1, const Point &p2)
{
    return Point{p1.x - p2.x, p1.y - p2.y};
}

inline Point operator+(const Point &p1, const Point &p2)
{
    return Point{p1.x + p2.x, p1.y + p2.y};
}

inline Point operator*(const Point &p1, const Point &p2)
{
    return Point{p1.x * p2.x, p1.y * p2.y};
}

inline Point operator/(const Point &p1, float scale)
{
    return Point{p1.x / scale, p1.y / scale};
}

inline Point operator*(const Point &p1, float scale)
{
    return Point{p1.x * scale, p1.y * scale};
}

inline Point pointNormalize(const Point &p)
{
    if (p.x == 0 && p.y == 0) return {};

    return p / std::sqrt(p.x * p.x + p.y * p.y);
}

inline float pointLength(const Point &p)
{
    return std::sqrt(p.x * p.x + p.y * p.y);
}

}