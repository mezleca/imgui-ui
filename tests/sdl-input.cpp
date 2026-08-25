#include <catch2/catch_test_macros.hpp>

#include "../ui/backends/sdl/backend.hpp"
#include "../ui/imgui/context-scope.hpp"
#include "../ui/ui.hpp"
#include "../ui/layout/overlay-container.hpp"
#include "../ui/widgets/button.hpp"
#include "../ui/widgets/checkbox.hpp"
#include "../ui/widgets/dropdown.hpp"
#include "imgui-context.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <string>
#include <vector>

TEST_CASE("handled button clicks still release ImGui mouse state", "[input][regression]") {
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    ui::set_backend(ui::create_sdl_backend);

    {
        ui::Runtime runtime;
        UI surface(runtime, ui::Config{.size = {320.0F, 240.0F}, .visible = false});
        REQUIRE(surface.ready());
        ImGui::SetCurrentContext(surface.imgui_context());
        ui_test::ImGuiContext::build_fonts();

        int click_count = 0;
        auto& button = surface.root().add_child<ui::ButtonWidget>(surface, "test button", ImVec2{160.0F, 36.0F});
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
        const ui::Rect button_rect = button.layout().screen_rect();
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
    ui::set_backend(ui::create_sdl_backend);

    {
        ui::Runtime runtime;
        UI surface(runtime, ui::Config{.size = {320.0F, 240.0F}, .visible = false});
        REQUIRE(surface.ready());
        ImGui::SetCurrentContext(surface.imgui_context());
        ui_test::ImGuiContext::build_fonts();

        bool content_value = false;
        bool overlay_value = false;
        std::string dropdown_value = "one";
        auto& content_checkbox = surface.root().add_child<ui::CheckboxWidget>(surface, content_value, "content");
        content_checkbox.set_placement(ui::Anchor::TopLeft, ui::Origin::TopLeft, {20.0F, 20.0F});
        auto& content_dropdown = surface.root().add_child<ui::DropdownWidget>(
            surface, dropdown_value, std::vector<ui::DropdownOption>{{"one", "one"}, {"two", "two"}}, "dropdown"
        );
        content_dropdown.set_size({180.0F, 52.0F});
        content_dropdown.set_placement(ui::Anchor::TopLeft, ui::Origin::TopLeft, {20.0F, 60.0F});

        auto& blocker = surface.root().add_child<ui::OverlayNode>("blocker");
        blocker.set_blocks_pointer_input(true);
        auto& overlay_checkbox = blocker.add_child<ui::CheckboxWidget>(surface, overlay_value, "overlay");
        overlay_checkbox.set_placement(ui::Anchor::TopLeft, ui::Origin::TopLeft, {20.0F, 130.0F});

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
        const ImVec2 content_position = center(content_checkbox.layout().screen_rect());
        const ImVec2 dropdown_position = center(content_dropdown.layout().screen_rect());
        const ImVec2 overlay_position = center(overlay_checkbox.layout().screen_rect());
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
