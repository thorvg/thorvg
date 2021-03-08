#ifndef _TVG_PNG_LOADER_H_
#define _TVG_PNG_LOADER_H_

#include <png.h>

class PngLoader : public Loader
{
public:
    const uint32_t* content = nullptr;

    ~PngLoader();

    using Loader::open;
    bool open(const string& path) override;
    bool read() override;
    bool close() override;

    const uint32_t* pixels() override;
};


#endif //_TVG_PNG_LOADER_H_