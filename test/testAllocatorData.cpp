#include <thorvg.h>
#include <type_traits>

//Public value types stay aggregates, not Allocator-derived heap objects.
//Keep this list in sync with DATA_TYPES in checkAllocator.py.
template<typename T>
constexpr bool plainData()
{
    return !std::is_base_of<tvg::Allocator, T>::value &&
           std::is_trivial<T>::value && std::is_standard_layout<T>::value;
}

static_assert(plainData<tvg::Matrix>(), "Matrix must remain plain data");
static_assert(plainData<tvg::Point>(), "Point must remain plain data");
static_assert(plainData<tvg::TextMetrics>(), "TextMetrics must remain plain data");
static_assert(plainData<tvg::GlyphMetrics>(), "GlyphMetrics must remain plain data");
static_assert(plainData<tvg::Fill::ColorStop>(), "ColorStop must remain plain data");

//C++14 has no std::is_aggregate: exercise the public aggregate initializers.
constexpr tvg::Matrix matrix{1, 0, 0, 0, 1, 0, 0, 0, 1};
constexpr tvg::Point point{1, 2};
constexpr tvg::TextMetrics text{1, 2, 3, 4};
constexpr tvg::GlyphMetrics glyph{1, 2, {3, 4}, {5, 6}};
constexpr tvg::Fill::ColorStop stop{0.5f, 10, 20, 30, 255};
static_assert(matrix.e33 == 1 && point.y == 2 && text.advance == 4 &&
              glyph.max.y == 6 && stop.a == 255, "Preserve aggregate initialization");
