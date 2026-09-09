# imgui-ui

A small ui framework built on top of [Dear ImGui](https://github.com/ocornut/imgui).

imgui-ui provides:
- node tree's
- layouts (containers)
- tween (animator)
- styling
- input routing
- custom widgets

<br>while keeping full compatibility with imgui internals.

# usage

```cmake
set(IMGUI_UI_BUILD_SDL ON)
# or
set(IMGUI_UI_BUILD_RAYLIB ON)

add_subdirectory(vendor/imgui-ui)

# cmake also exposes the selected backend and vendored dependencies through the "imgui-ui" namespace
target_link_libraries(my-app PRIVATE imgui-ui::sdl)
```

```cpp
#include <ui/backends/sdl/backend.hpp>
#include <ui/ui.hpp>
#include <imgui.hpp>
#include <ui/widgets/button.hpp>

ui::Runtime runtime;
auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
    .title = "example",
    .size = {900.0F, 600.0F},
});

ui::UI surface(runtime, {.backend = std::move(backend)});
surface.root().add<ui::ButtonWidget>(surface, "hello");

while (!surface.is_done()) {
    surface.begin_frame();
    surface.update(ImGui::GetIO().DeltaTime);
    surface.draw();
    surface.end_frame();
}
```

<img src="https://github.com/mezleca/imgui-ui/blob/main/assets/images/preview.png?raw=true" width="60%"/><br>

see `examples/` for a complete demo.
