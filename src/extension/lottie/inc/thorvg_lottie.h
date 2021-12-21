/*!
 * @file thorvg_lottie.h
 *
 */


#ifndef _THORVG_LOTTIE_H_
#define _THORVG_LOTTIE_H_

#include "thorvg.h"

#ifdef __cplusplus
extern "C" {
#endif


namespace tvg
{

/**
 * @defgroup ThorVG_Lottie ThorVG_Lottie
 * @brief ThorVG classes for lotie
 */

/**@{*/


/**
 * @class Lottie
 *
 * @brief
 *
 * @BETA_API
 */
class TVG_EXPORT Lottie final : public Picture, public Animation
{
public:
    ~Lottie();


    /**
     * @brief Load a lottie data(json) for specific frames directly from a file.
     *        This API is a temporary API for Lottie animation support.
     *
     * @warning Please do not use it, this API is not official one. It could be modified in the next version.
     *
     * @BETA_API
     */
    Result load(const std::string& path) noexcept;


    /**
     * @brief Creates a new Lottie object.
     *
     * @return A new Lottie object.
     */
    static std::unique_ptr<Lottie> gen() noexcept;

    /**
     * @brief Return the unique id value of this class.
     *
     * This method can be referred for identifying the Lottie class type.
     *
     * @return The type id of the Lottie class.
     *
     * @BETA_API
     */
    static uint32_t identifier() noexcept;

    _TVG_DECLARE_ACCESSOR();
    _TVG_DECLARE_PRIVATE(Lottie);
};
/** @}*/

} //namespace

#ifdef __cplusplus
}
#endif

#endif //_THORVG_LOTTIE_H_
