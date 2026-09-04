#include <catch2/catch_test_macros.hpp>

#include "../ui/backends/sdl/backend.hpp"
#include "../ui/imgui/context-scope.hpp"
#include "../ui/ui.hpp"
#include "../ui/layout/container.hpp"
#include "../ui/layout/layer-container.hpp"
#include "../ui/widgets/button.hpp"
#include "../ui/widgets/checkbox.hpp"
#include "../ui/widgets/dropdown.hpp"
#include "imgui-context.hpp"

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <imgui.h>
#include "../vendor/imgui/backends/imgui_impl_opengl3.h"

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE("opengl box shadows cover the spread outside a panel", "[render][regression]") {
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    {
        ui::Runtime runtime;
        auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
            .size = {128.0F, 128.0F},
            .visible = false,
            .swap_interval = 0,
        });
        UI surface(runtime, std::move(backend));
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

        surface.begin_input_frame();
        surface.begin_frame();
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        REQUIRE(draw_data != nullptr);
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        glFinish();

        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        const ImVec2 sample = {
            (panel.layout().visual_rect().min.x + panel.layout().visual_rect().max.x) * 0.5F,
            panel.layout().visual_rect().min.y - 8.0F
        };
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

    SDL_Quit();
}

TEST_CASE("handled button clicks still release ImGui mouse state", "[input][regression]") {
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    {
        ui::Runtime runtime;
        auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
            .size = {320.0F, 240.0F},
            .visible = false,
        });
        UI surface(runtime, std::move(backend));
        REQUIRE(surface.ready());
        ImGui::SetCurrentContext(surface.imgui_context());
        ui_test::ImGuiContext::build_fonts();

        int click_count = 0;
        auto& button =
            surface.root().add<ui::ButtonWidget>(surface, "test button", ui::LayoutSize{ui::px(160.0F), ui::px(36.0F)});
        button.on_event = [&click_count](ui::UiEvent& event) {
            if (event.type == ui::EventType::Click) {
                ++click_count;
                event.mark_handled();
            }
        };

        const auto draw_frame = [&surface] {
            surface.begin_input_frame();
            surface.begin_frame();
            surface.root().update(ImGui::GetIO().DeltaTime);
            surface.root().draw();
            surface.end_frame();
        };

        draw_frame();
        const ui::Rect button_rect = button.layout().visual_rect();
        const ImVec2 click_position = {
            (button_rect.min.x + button_rect.max.x) * 0.5F,
            (button_rect.min.y + button_rect.max.y) * 0.5F,
        };
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

        draw_frame();
        {
            const ui::ImGuiContextScope context(surface.imgui_context());
            CHECK(ImGui::IsMouseDown(ImGuiMouseButton_Left));
        }

        draw_frame();
        {
            const ui::ImGuiContextScope context(surface.imgui_context());
            REQUIRE_FALSE(ImGui::IsMouseDown(ImGuiMouseButton_Left));
        }
    }

    SDL_Quit();
}

TEST_CASE("pointer blocker prevents native content mutation but keeps descendants interactive", "[input][regression]") {
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    {
        ui::Runtime runtime;
        auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
            .size = {320.0F, 240.0F},
            .visible = false,
        });
        UI surface(runtime, std::move(backend));
        REQUIRE(surface.ready());
        ImGui::SetCurrentContext(surface.imgui_context());
        ui_test::ImGuiContext::build_fonts();

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
        blocker.set_input_blocker();
        auto& overlay_checkbox = blocker.add<ui::CheckboxWidget>(surface, overlay_value, "overlay");
        overlay_checkbox.set_layout({
            .size = {ui::fit(), ui::fit()},
            .placement = {.offset = {20.0F, 130.0F}},
            .in_flow = false,
        });

        const auto draw_frame = [&surface] {
            surface.begin_input_frame();
            surface.begin_frame();
            surface.root().update(ImGui::GetIO().DeltaTime);
            surface.root().draw();
            surface.end_frame();
        };

        draw_frame();
        const auto center = [](const ui::Rect& rect) {
            return ImVec2{(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
        };
        const auto checkbox_center = [&center](const ui::CheckboxWidget& checkbox) {
            const ui::Rect widget_rect = checkbox.layout().visual_rect();
            const ImVec2 padding = checkbox.style().padding();
            const ui::Rect box_rect = ui::Rect::from_position_size(
                {widget_rect.min.x + padding.x, widget_rect.min.y + padding.y}, checkbox.frame().layout().size()
            );
            return center(box_rect);
        };
        const ImVec2 content_position = checkbox_center(content_checkbox);
        const ImVec2 dropdown_position = center(content_dropdown.layout().visual_rect());
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
            draw_frame();
            draw_frame();
        };

        click(content_position, true);
        CHECK_FALSE(content_value);

        click(dropdown_position, true);
        CHECK_FALSE(content_dropdown.is_open());
        CHECK(dropdown_value == "one");

        click(overlay_position, false);
        CHECK(overlay_value);
    }

    SDL_Quit();
}
