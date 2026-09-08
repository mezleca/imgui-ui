#include <catch2/catch_test_macros.hpp>

#include <ui/backends/sdl/backend.hpp>
#include <ui/diagnostics/debugger.hpp>
#include <ui/imgui/context-scope.hpp>
#include <ui/layout/container.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/dropdown.hpp>

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <imgui.h>

#include "imgui-context.hpp"
#include "../vendor/imgui/backends/imgui_impl_opengl3.h"

#include <algorithm>
#include <string>
#include <vector>

class SdlVideoSession final {
public:
    SdlVideoSession() {
        REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    }

    ~SdlVideoSession() {
        SDL_Quit();
    }
};

TEST_CASE("opengl box shadows cover the spread outside a panel", "[render][regression]") {
    SdlVideoSession sdl;
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
        .size = {128.0F, 128.0F},
        .visible = false,
        .swap_interval = 0,
    });
    ui::UI surface(runtime, {.backend = std::move(backend)});
    REQUIRE(surface.ready());

    auto& panel = surface.root().add<ui::Container>("shadow-panel");
    panel.set_layout({
        .size = {ui::px(40.0F), ui::px(40.0F)},
        .placement = {.offset = {44.0F, 44.0F}},
        .in_flow = false,
    });
    panel.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F})
            .box_shadow({
                .blur = 20.0F,
                .spread = 20.0F,
                .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F},
            });
    });

    surface.begin_frame();
    surface.update(ImGui::GetIO().DeltaTime);
    surface.draw();
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    glFinish();

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const ImVec2 panel_center = ui_test::center(panel.layout().visual_rect());
    const ImVec2 sample = {panel_center.x, panel.layout().visual_rect().min.y - 8.0F};
    const ImVec2 framebuffer_sample = {
        (sample.x - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x,
        static_cast<float>(viewport[3]) - (sample.y - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y,
    };
    unsigned char pixel[4]{};
    const int pixel_x = std::clamp(static_cast<int>(framebuffer_sample.x), 0, viewport[2] - 1);
    const int pixel_y = std::clamp(static_cast<int>(framebuffer_sample.y), 0, viewport[3] - 1);
    glReadPixels(pixel_x, pixel_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] < 80);
    CHECK(pixel[1] < 80);
    CHECK(pixel[2] < 80);
    surface.end_frame();
}

TEST_CASE("handled button clicks still release ImGui mouse state", "[input][regression]") {
    SdlVideoSession sdl;
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
        .size = {320.0F, 240.0F},
        .visible = false,
    });
    ui::UI surface(runtime, {.backend = std::move(backend)});
    REQUIRE(surface.ready());
    ui_test::prepare_surface(surface);

    int click_count = 0;
    auto& button = surface.root().add<ui::ButtonWidget>(surface, "test button", ui::LayoutSize{ui::px(160.0F), ui::px(36.0F)});
    button.set_on_event([&click_count](ui::UiEvent& event) {
        if (event.type == ui::EventType::Click) {
            ++click_count;
            event.mark_handled();
        }
    });

    ui_test::draw_surface(surface);
    const ui::Rect button_rect = button.layout().visual_rect();
    const ImVec2 click_position = ui_test::center(button_rect);
    const SDL_WindowID window_id = surface.backend().window_id();

    SDL_Event down{};
    down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    down.button.windowID = window_id;
    down.button.x = click_position.x;
    down.button.y = click_position.y;
    down.button.button = SDL_BUTTON_LEFT;
    REQUIRE_FALSE(ui::process_sdl_event(surface, down));

    SDL_Event up = down;
    up.type = SDL_EVENT_MOUSE_BUTTON_UP;
    REQUIRE(ui::process_sdl_event(surface, up));
    REQUIRE(click_count == 1);

    ui_test::draw_surface(surface);
    {
        const ui::ImGuiContextScope context(surface.imgui_context());
        CHECK(ImGui::IsMouseDown(ImGuiMouseButton_Left));
    }

    ui_test::draw_surface(surface);
    {
        const ui::ImGuiContextScope context(surface.imgui_context());
        REQUIRE_FALSE(ImGui::IsMouseDown(ImGuiMouseButton_Left));
    }
}

TEST_CASE("debugger hotkey is received through the sdl backend", "[input][regression]") {
    SdlVideoSession sdl;
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
        .size = {320.0F, 240.0F},
        .visible = false,
    });
    ui::UI surface(
        runtime, {
                     .backend = std::move(backend),
                     .enable_debugger = true,
                 }
    );
    REQUIRE(surface.ready());
    REQUIRE(surface.debugger() != nullptr);
    surface.debugger()->set_open(false);
    ui_test::prepare_surface(surface);

    const SDL_WindowID window_id = surface.backend().window_id();
    SDL_Event shift_down{};
    shift_down.type = SDL_EVENT_KEY_DOWN;
    shift_down.key.windowID = window_id;
    shift_down.key.key = SDLK_LSHIFT;
    shift_down.key.scancode = SDL_SCANCODE_LSHIFT;
    shift_down.key.mod = SDL_KMOD_SHIFT;
    REQUIRE_FALSE(ui::process_sdl_event(surface, shift_down));

    SDL_Event d_down = shift_down;
    d_down.key.key = SDLK_D;
    d_down.key.scancode = SDL_SCANCODE_D;
    REQUIRE_FALSE(ui::process_sdl_event(surface, d_down));

    surface.begin_frame();
    REQUIRE(surface.debugger()->is_open());
    surface.end_frame();
}

TEST_CASE("pointer blocker prevents native content mutation but keeps descendants interactive", "[input][regression]") {
    SdlVideoSession sdl;
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
        .size = {320.0F, 240.0F},
        .visible = false,
    });
    ui::UI surface(runtime, {.backend = std::move(backend)});
    REQUIRE(surface.ready());
    ui_test::prepare_surface(surface);

    bool content_value = false;
    bool overlay_value = false;
    std::string dropdown_value = "one";
    auto& content_checkbox = surface.root().add<ui::CheckboxWidget>(surface, content_value, "content");
    content_checkbox.set_layout({
        .size = {ui::fit(), ui::fit()},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    auto& content_dropdown = surface.root().add<ui::DropdownWidget>(
        surface, dropdown_value, std::vector<ui::DropdownOption>{{"one", "one"}, {"two", "two"}}, "dropdown"
    );
    content_dropdown.set_size({ui::px(180.0F), ui::px(52.0F)});
    content_dropdown.set_layout({
        .size = {ui::px(180.0F), ui::px(52.0F)},
        .placement = {.offset = {20.0F, 60.0F}},
        .in_flow = false,
    });

    auto& blocker = surface.root().add<ui::LayerContainer>("blocker", ui::LayerMode::Inline);
    blocker.set_input_mode(InputMode::Blocker);
    auto& overlay_checkbox = blocker.add<ui::CheckboxWidget>(surface, overlay_value, "overlay");
    overlay_checkbox.set_layout({
        .size = {ui::fit(), ui::fit()},
        .placement = {.offset = {20.0F, 130.0F}},
        .in_flow = false,
    });

    ui_test::draw_surface(surface);
    const auto checkbox_center = [](const ui::CheckboxWidget& checkbox) {
        const ui::Rect widget_rect = checkbox.layout().visual_rect();
        const ImVec2 padding = checkbox.style().padding();
        const ui::Rect box_rect = ui::Rect::from_position_size(
            {widget_rect.min.x + padding.x, widget_rect.min.y + padding.y}, checkbox.frame().layout().size()
        );
        return ui_test::center(box_rect);
    };
    const ImVec2 content_position = checkbox_center(content_checkbox);
    const ImVec2 dropdown_position = ui_test::center(content_dropdown.layout().visual_rect());
    const ImVec2 overlay_position = checkbox_center(overlay_checkbox);
    const SDL_WindowID window_id = surface.backend().window_id();

    const auto click = [&](ImVec2 position, bool expected_handled) {
        SDL_Event motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = window_id;
        motion.motion.x = position.x;
        motion.motion.y = position.y;
        CHECK(ui::process_sdl_event(surface, motion) == expected_handled);

        SDL_Event down{};
        down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        down.button.windowID = window_id;
        down.button.x = position.x;
        down.button.y = position.y;
        down.button.button = SDL_BUTTON_LEFT;
        CHECK(ui::process_sdl_event(surface, down) == expected_handled);

        SDL_Event up = down;
        up.type = SDL_EVENT_MOUSE_BUTTON_UP;
        CHECK(ui::process_sdl_event(surface, up) == expected_handled);
        ui_test::draw_surface(surface);
        ui_test::draw_surface(surface);
    };

    click(content_position, true);
    CHECK_FALSE(content_value);

    click(dropdown_position, true);
    CHECK_FALSE(content_dropdown.is_open());
    CHECK(dropdown_value == "one");

    click(overlay_position, false);
    CHECK(overlay_value);
}

TEST_CASE("dropdown selection and cursor use the sdl input path", "[dropdown][input][regression]") {
    SdlVideoSession sdl;
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
        .size = {320.0F, 240.0F},
        .visible = false,
    });
    ui::UI surface(runtime, {.backend = std::move(backend)});
    REQUIRE(surface.ready());
    ui_test::prepare_surface(surface);

    std::string value = "one";
    int changes = 0;
    auto& dropdown = surface.root().add<ui::DropdownWidget>(
        surface, value, std::vector<ui::DropdownOption>{{"one", "one"}, {"two", "two"}}, "dropdown"
    );
    dropdown.set_layout({
        .size = {ui::px(180.0F), ui::px(36.0F)},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    dropdown.set_on_change([&changes] { ++changes; });

    const SDL_WindowID window_id = surface.backend().window_id();
    const auto send_pointer = [window_id, &surface](SDL_EventType type, ImVec2 position) {
        SDL_Event event{};
        event.type = type;
        if (type == SDL_EVENT_MOUSE_MOTION) {
            event.motion.windowID = window_id;
            event.motion.x = position.x;
            event.motion.y = position.y;
        } else {
            event.button.windowID = window_id;
            event.button.x = position.x;
            event.button.y = position.y;
            event.button.button = SDL_BUTTON_LEFT;
        }
        return ui::process_sdl_event(surface, event);
    };

    ui_test::draw_surface(surface);
    const ui::Rect trigger_rect = dropdown.trigger().layout().visual_rect();
    const ImVec2 trigger_position = ui_test::center(trigger_rect);
    send_pointer(SDL_EVENT_MOUSE_MOTION, trigger_position);
    send_pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, trigger_position);
    send_pointer(SDL_EVENT_MOUSE_BUTTON_UP, trigger_position);
    ui_test::draw_surface(surface);
    REQUIRE(dropdown.is_open());
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);

    const ui::Rect body_rect = dropdown.body().layout().visual_rect();
    const float item_height = body_rect.size().y * 0.5F;
    const float body_center_x = ui_test::center(body_rect).x;
    const auto require_option = [&surface](ImVec2 position) {
        const ui::Node* option = surface.input_router().node_at(position);
        REQUIRE(option != nullptr);
        REQUIRE(option->type_name() == "DropdownOption");
    };
    for (int index = 0; index < 2; ++index) {
        const ImVec2 option_position = {
            body_center_x,
            body_rect.min.y + item_height * (static_cast<float>(index) + 0.5F),
        };

        send_pointer(SDL_EVENT_MOUSE_MOTION, option_position);
        require_option(option_position);
        REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);

        ui_test::draw_surface(surface);
        require_option(option_position);
        REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);
    }

    const ImVec2 option_position = {body_center_x, body_rect.min.y + item_height * 1.5F};
    send_pointer(SDL_EVENT_MOUSE_MOTION, option_position);

    const float visible_opacity = dropdown.body().opacity();
    send_pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, option_position);
    send_pointer(SDL_EVENT_MOUSE_BUTTON_UP, option_position);
    REQUIRE(value == "two");
    REQUIRE_FALSE(dropdown.is_open());

    ui_test::draw_surface(surface);
    REQUIRE(dropdown.body().opacity() < visible_opacity);
    for (int frame = 0; frame < 8; ++frame) {
        ui_test::draw_surface(surface);
    }
    REQUIRE_FALSE(dropdown.body().visually_visible());
    REQUIRE(changes == 1);
}
