#ifdef _USEIMGUI
#pragma once

class SceneManagerWindow {
public:
    SceneManagerWindow() = default;
    ~SceneManagerWindow() = default;

    void Draw(bool* show = nullptr);
};
#endif
