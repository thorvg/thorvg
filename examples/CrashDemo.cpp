/*
 * random transform stress test: apply random translate/scale/rotate/shear to a picture each frame
 * to cover the edge cases of tvgSwRasterTexmap.
 */

#include "Example.h"
#include <cmath>
#include <cstdint>

struct UserExample : tvgexam::Example {
    uint32_t cw = 0, ch = 0;
    uint32_t seed = 0x12345678u;
    inline float frand(float a, float b)
    {
        seed = 1664525u * seed + 1013904223u;
        float t = (seed >> 8) * (1.0f / 16777216.0f);
        return a + (b - a) * t;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        cw = w;
        ch = h;
        seed ^= (w << 16) ^ h;
        return update(canvas, 0);
    }

    tvg::Matrix randomMatrix()
    {
        // translate/rotate/scale/shear
        float tx = frand(-(float)cw * 1.5f, (float)cw * 1.5f);
        float ty = frand(-(float)ch * 1.5f, (float)ch * 1.5f);
        float angle = frand(0.0f, 360.0f) * 3.1415926535f / 180.0f;
        float s = frand(0.1f, 5.0f);
        float shx = frand(-1.0f, 1.0f);
        float shy = frand(-1.0f, 1.0f);

        float c = std::cos(angle);
        float sn = std::sin(angle);

        // M = T * R * Sh * S
        float m00 = s * (c - sn * shy);
        float m01 = s * (c * shx - sn);
        float m10 = s * (sn + c * shy);
        float m11 = s * (sn * shx + c);
        float m02 = tx;
        float m12 = ty;

        /*
        Crash Case
        Matrix:
        [ 0.572866, -4.431353, 336.605835 ]
        [ 5.198910, -0.386219, 30.710693 ]
        [ 0.000000, 0.000000, 1.000000 ]
        */
        m00 = 0.572866f;
        m01 = -4.431353f;
        m02 = 336.605835f;
        m10 = 5.198910f;
        m11 = -0.386219f;
        m12 = 30.710693f;

        printf("Matrix:\n");
        printf("[ %f, %f, %f ]\n",
            static_cast<double>(m00),
            static_cast<double>(m01),
            static_cast<double>(m02));
        printf("[ %f, %f, %f ]\n",
            static_cast<double>(m10),
            static_cast<double>(m11),
            static_cast<double>(m12));
        printf("[ 0.000000, 0.000000, 1.000000 ]\n\n");

        return tvg::Matrix { m00, m01, m02, m10, m11, m12, 0.0f, 0.0f, 1.0f };
    }

    bool update(tvg::Canvas* canvas, uint32_t /*elapsed*/) override
    {
        if (!tvgexam::verify(canvas->remove()))
            return false;

        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(EXAMPLE_DIR "/image/red.png")))
            return false;
        picture->transform(randomMatrix());
        canvas->push(picture);

        return true;
    }
};

int main(int argc, char** argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, 960, 960);
}
