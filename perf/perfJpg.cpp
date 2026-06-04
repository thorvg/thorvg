// POC benchmark: opaque JPEG draw on the CPU engine.
// For each sample x scale, measures the per-draw time with the opaque fast path
// OFF (alpha-blend) vs ON, then logs an aggregated summary. No arguments.
//
// IMPORTANT: build with -Dpartial=false. The loop redraws a STATIC scene, so with
// partial rendering on there is no dirty region and the image blit is skipped
// entirely -> both paths read ~equal (~1.0x). partial=false forces a real re-raster
// every draw so the blit (what this optimizes) is actually measured.
#include <thorvg.h>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

using namespace tvg;
using namespace std::chrono;

static const char* samples[] = {"test_512x512", "test_4000x4000", "test_2539x1428"};
static const float scales[] = {0.5f, 1.0f, 2.0f};
static const int draws = 1000;

//save an ARGB8888S buffer as a 24-bit BMP (to verify the scaled output)
static void _save(const uint32_t* buf, uint32_t w, uint32_t h, const char* path)
{
    auto rowSize = (w * 3 + 3) & ~3u;
    auto dataSize = rowSize * h;
    uint8_t hdr[54] = {0};
    auto put32 = [&](int off, uint32_t v) { memcpy(hdr + off, &v, 4); };
    auto put16 = [&](int off, uint16_t v) { memcpy(hdr + off, &v, 2); };
    hdr[0] = 'B'; hdr[1] = 'M';
    put32(2, 54 + dataSize); put32(10, 54); put32(14, 40);
    put32(18, w); put32(22, h); put16(26, 1); put16(28, 24); put32(34, dataSize);

    auto f = fopen(path, "wb");
    fwrite(hdr, 1, 54, f);
    auto row = static_cast<uint8_t*>(calloc(rowSize, 1));
    for (auto y = int32_t(h) - 1; y >= 0; --y) {   //BMP rows are bottom-up
        auto src = buf + size_t(y) * w;
        for (uint32_t x = 0; x < w; ++x) {
            auto p = src[x];                       //ARGB8888S: A<<24 | R<<16 | G<<8 | B
            row[x * 3 + 0] = p & 0xff;
            row[x * 3 + 1] = (p >> 8) & 0xff;
            row[x * 3 + 2] = (p >> 16) & 0xff;
        }
        fwrite(row, 1, rowSize, f);
    }
    free(row);
    fclose(f);
}

//average ms per draw, with the opaque fast path toggled
static double _measure(SwCanvas* canvas, int draws, bool opaque)
{
    OPAQUE_OPT = opaque;
    canvas->update(); 
    canvas->draw(false); 
    canvas->sync();   //warmup
    auto t = high_resolution_clock::now();
    for (auto i = 0; i < draws; ++i) {
        canvas->update();
        canvas->draw(false);
        canvas->sync();
    }
    return duration<double, std::milli>(high_resolution_clock::now() - t).count() / draws;
}

int main()
{
    if (Initializer::init(0) != Result::Success) return 1;

    printf("%-16s %-6s %-11s %11s %11s %8s\n", "sample", "scale", "size", "blend", "opaque", "speedup");

    auto total = 0.0;
    auto count = 0;

    for (auto name : samples) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.jpg", PERF_DIR, name);

        for (auto scale : scales) {
            auto picture = Picture::gen();
            if (picture->load(path) != Result::Success) { printf("load failed: %s\n", path); picture->unref(); break; }

            float w, h;
            picture->size(&w, &h);
            auto sw = uint32_t(w * scale + 0.5f);
            auto sh = uint32_t(h * scale + 0.5f);
            picture->size(w * scale, h * scale);   //scale 1 -> direct path, else uniform-scaled path

            auto buffer = static_cast<uint32_t*>(malloc(size_t(sw) * sh * sizeof(uint32_t)));
            if (!buffer) { printf("%-16s %-6.2f alloc failed (%ux%u)\n", name, (double)scale, sw, sh); picture->unref(); continue; }

            auto canvas = SwCanvas::gen();
            canvas->target(buffer, sw, sw, sh, ColorSpace::ARGB8888S);
            canvas->add(picture);


            auto blend = _measure(canvas, draws, false);
            auto opaque = _measure(canvas, draws, true);

            char size[16];
            snprintf(size, sizeof(size), "%ux%u", sw, sh);
            printf("%-16s %-6.2f %-11s %11.4f %11.4f %7.2fx\n", name, (double)scale, size, blend, opaque, blend / opaque);

            total += blend / opaque;
            ++count;

            //save the scaled result (NOT measured) so the resize is visually verifiable
            char out[512];
            snprintf(out, sizeof(out), "%s/%s_x%.2f_%s.bmp", PERF_DIR, name, (double)scale, size);
            canvas->update(); canvas->draw(true); canvas->sync();
            _save(buffer, sw, sh, out);

            delete canvas;   //frees the added picture
            free(buffer);
        }
    }

    if (count > 0) printf("\noverall avg speedup: %.2fx over %d cases\n", total / count, count);

    Initializer::term();
    return 0;
}
