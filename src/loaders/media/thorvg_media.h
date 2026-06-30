#ifndef _THORVG_MEDIA_H_
#define _THORVG_MEDIA_H_

#include "thorvg.h"

namespace tvg
{

/**
 * @class Video
 *
 * @brief The Video class enables control of video playback.
 *
 * @note Experimental API
 */
struct TVG_API Video final
{
    ~Video();

    /**
     * @brief Starts or resumes video playback.
     *
     * @retval Result::InsufficientCondition If the video is not loaded.
     *
     * @see stop()
     * @see pause()
     */
    Result play() noexcept;

    /**
     * @brief Pauses video playback.
     *
     * @retval Result::InsufficientCondition If the video is not started playing.
     *
     * @see play()
     */
    Result pause() noexcept;

    /**
     * @brief Stops video playback.
     *
     * @retval Result::InsufficientCondition If the video is not loaded.
     *
     * @see play()
     */
    Result stop() noexcept;

    /**
     * @brief Specifies the current video position in seconds.
     *
     * @param[in] seconds The playback position in seconds. The value should be in the range from 0.0 to duration().
     *
     * @retval Result::InsufficientCondition If the video is not loaded.
     * @retval Result::InvalidArguments If @p seconds is out of range.
     *
     * @see Video::time()
     * @see Video::duration()
     */
    Result seek(float seconds) noexcept;

    /**
     * @brief Enables or disables repeated playback.
     *
     * @param[in] on @c true to repeat playback, @c false otherwise.
     *
     * @retval Result::InsufficientCondition If the video is not loaded.
     *
     * @see Video::loop()
     */
    Result loop(bool on) noexcept;

    /**
     * @brief Retrieves whether repeated playback is enabled.
     *
     * @return @c true if looping, @c false otherwise (or if the video is not loaded).
     *
     * @see Video::loop(bool)
     */
    bool loop() const noexcept;

    /**
     * @brief Retrieves a picture instance associated with this video player instance.
     *
     * This function provides access to the picture instance that can be added to the designated canvas,
     * enabling control over video frames with this Video instance.
     *
     * @return A picture instance that is tied to this video player.
     *
     * @warning The returned picture remains valid until the Video object is destroyed.
     */
    Picture* picture() const noexcept;

    /**
     * @brief Retrieves the current playback position of the video.
     *
     * @return The current playback position in seconds.
     *
     * @see Video::seek()
     * @see Video::duration()
     */
    float time() const noexcept;

    /**
     * @brief Retrieves the duration of the video in seconds.
     *
     * @return The duration of the video in seconds.
     */
    float duration() const noexcept;

    /**
     * @brief Sets the audio volume level of the video.
     *
     * @param[in] volume The volume level in the range [0.0, 1.0],
     *                   where 0.0 is silent and 1.0 is the original volume.
     *
     * @retval Result::InsufficientCondition If the video is not loaded.
     * @retval Result::InvalidArguments If @p volume is out of range.
     *
     * @see Video::volume()
     * @see Video::mute()
     */
    Result volume(float volume) noexcept;

    /**
     * @brief Retrieves the current audio volume level of the video.
     *
     * @return The current volume level in the range [0.0, 1.0].
     *
     * @note It returns @c 0, if the video is not loaded.
     *
     * @see Video::volume(float)
     */
    float volume() const noexcept;

    /**
     * @brief Enables or disables audio muting.
     *
     * When muted, the video audio is silenced without modifying the current
     * volume level. Unmuting restores audio playback using the previously
     * configured volume.
     *
     * @param[in] on @c true to mute the audio, @c false to unmute it.
     *
     * @retval Result::InsufficientCondition If the video is not loaded.
     *
     * @see Video::muted()
     * @see Video::volume()
     */
    Result mute(bool on) noexcept;

    /**
     * @brief Retrieves whether the video audio is currently muted.
     *
     * @return @c true if the audio is muted, @c false otherwise.
     *
     * @note It returns @c false, if the video is not loaded.
     *
     * @see Video::mute()
     */
    bool muted() const noexcept;

    /**
     * @brief Creates a new Video object.
     *
     * @return A new Video object.
     */
    static Video* gen() noexcept;

    _TVG_DECLARE_PRIVATE_BASE(Video);
};

}  // namespace tvg

#endif  //_THORVG_MEDIA_H_
