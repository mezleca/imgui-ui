# imgui-ui

a small retained ui "framework" built on top of dear imgui and opengl. it provides a node tree,
layout, styling, and input routing with sdl or raylib backends.

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
#include <ui/widgets/button.hpp>

ui::Runtime runtime;
auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
    .title = "example",
    .size = {900.0F, 600.0F},
});

UI surface(runtime, {.backend = std::move(backend)});
surface.root().add<ui::ButtonWidget>(surface, "hello");
```

see `examples/` for a complete demo.
