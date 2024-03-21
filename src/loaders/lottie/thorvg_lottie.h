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
     * @brief Override Lottie properties using slot data.
     *
     * @param[in] slot The Lottie slot data in JSON format to override, or @c nullptr to reset.
     *
     * @retval Result::Success When succeed.
     * @retval Result::InsufficientCondition In case the animation is not loaded.
     * @retval Result::InvalidArguments When the given parameter is invalid.
     *
     * @note Experimental API
     */
    Result override(const char* slot) noexcept;

    /**
    * @brief Specifies a segment by marker. 
    * 
    * Markers are used to control animation playback by specifying start and end points, 
    * eliminating the need to know the exact frame numbers.
    * Generally, markers are designated at the design level, 
    * meaning the callers must know the marker name in advance to use it.
    *
    * @param[in] marker The name of the segment marker.
    *
    * @retval Result::Success When successful.
    * @retval Result::InsufficientCondition If the animation is not loaded.
    * @retval Result::InvalidArguments When the given parameter is invalid.
    * @retval Result::NonSupport When it's not animatable.
    *
    * @note Set @c nullptr to reset the specified segment.
    */
    Result segment(const char* marker) noexcept;
    
    /**
     * @brief Specifies the playback segment of the animation.
     *
     * The set segment is designated as the play area of the animation. 
     * This is useful for playing a specific segment within the entire animation. 
     * After setting, the number of animation frames and the playback time are calculated 
     * by mapping the playback segment as the entire range.
     *
     * @param[in] start segment start.
     * @param[in] end segment end.
     *
     * @retval Result::Success When succeed.
     * @retval Result::InsufficientCondition In case the animation is not loaded.
     * @retval Result::InvalidArguments When the given parameter is invalid.
     * @retval Result::NonSupport When it's not animatable.
     *
     * @note Range from 0.0~1.0
     */
    Result segment(float start, float end) noexcept;
    
    /**
     * @brief Return the marker count
     *
     * @retval Return 0 when failed or no marker exists..
     */
    uint32_t markerCnt() noexcept;
    
    /**
     * @brief Return the marker name by a given index.
     *
     * @param[in] idx marker index. idx starts from 0.
     *
     * @retval return nullptr when failed.
     */
    const char* markers(uint32_t idx) noexcept;
    
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
