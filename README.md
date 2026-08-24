# imgui-ui

a small retained ui "framework" built on top of dear imgui. it provides a node tree,
layout, styling, input routing with sdl or raylib backends.

# usage

```cmake
add_subdirectory(vendor/imgui-ui)
target_link_libraries(my-app PRIVATE imgui-ui::sdl)
```

dear imgui is fetched, built and exposed by the framework target.

```cpp
#include <ui/backends/sdl/backend.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>

ui::set_backend(ui::create_sdl_backend);

ui::Runtime runtime;
UI surface(runtime, {.title = "example", .size = {900.0F, 600.0F}});
surface.root().add_child<ui::ButtonWidget>(surface, "hello");
```

the application forwards platform input to `process_sdl_event` or
`process_raylib_events`, then calls `begin_input_frame`, `begin_frame`, updates
and draws the root, and finishes with `end_frame`.

```sh
./ez format
./ez tests
./ez build --backend sdl
./ez build --backend raylib
./ez run sdl
./ez run raylib
```

see `examples/` for a "complete" demo.
