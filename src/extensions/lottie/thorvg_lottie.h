#ifndef _THORVG_LOTTIE_H_
#define _THORVG_LOTTIE_H_

#include <thorvg.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace tvg
{

class TVG_EXPORT Lottie final : public Picture, public Animation
{
public:
    virtual ~Lottie();

    //TODO: LOTTIE Specific methods

    static std::unique_ptr<Lottie> gen() noexcept;
    static uint32_t identifier() noexcept;

    _TVG_DECLARE_ACCESSOR();
    _TVG_DECALRE_IDENTIFIER();
    _TVG_DECLARE_PRIVATE(Lottie);
};

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_THORVG_LOTTIE_H_
