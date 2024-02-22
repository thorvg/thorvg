#ifndef _THORVG_LOTTIE_H_
#define _THORVG_LOTTIE_H_

#include <thorvg.h>

namespace tvg
{

/**
 * @class LottieAnimation
 *
 * @brief The LottieAnimation class enables control of advanced Lottie features.
 *
 * This class extends the Animation and has additional interfaces.
 *
 * @see Animation
 * 
 * @note Experimental API
 */
class TVG_API LottieAnimation final : public Animation
{
public:
    ~LottieAnimation();

    /**
     * @brief Rewrite lottie properties through the slot data
     *
     * @param[in] slot The lottie slot data
     *
     * @retval Result::Success When succeed.
     * @retval Result::InsufficientCondition When the given parameter is invalid.
     *
     * @note Experimental API
     */
    Result override(const char* slot) noexcept;

    /**
     * @brief Creates a new LottieAnimation object.
     *
     * @return A new LottieAnimation object.
     *
     * @note Experimental API
     */
    static std::unique_ptr<LottieAnimation> gen() noexcept;
};

} //namespace

#endif //_THORVG_LOTTIE_H_
