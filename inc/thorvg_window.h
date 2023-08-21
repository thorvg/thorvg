/*!
 * @file thorvg_window.h
 *
 */


#ifndef _THORVG_WINDOW_H_
#define _THORVG_WINDOW_H_

#include <functional>
#include <memory>
#include <string>
#include <list>
#include <thorvg.h>

namespace tvg
{

/**
 * @class Window
 *
 * @brief .
 *
 *
 * @BETA_API
 */
class TVG_API Window final
{
public:
    ~Window();

    void close();

    bool run();

    void resize(int width, int height);

    void init(std::function<bool(tvg::Canvas*)> on_init);

    void update(std::function<bool(tvg::Canvas*)> on_update);

    /**
     * @brief Creates a new Window object.
     *
     * @return A new Window object.
     */
    static std::unique_ptr<Window> gen(int width = 1, int height = 1, std::string name = "dummy", tvg::CanvasEngine engine = tvg::CanvasEngine::Sw) noexcept; 

    static void loop();
private:
    // hmmm ....?
    Window(int width, int height, std::string name, tvg::CanvasEngine engine);
    _TVG_DECLARE_PRIVATE(Window);
};


/** @}*/

} //namespace

#endif //_THORVG_WINDOW_H_
