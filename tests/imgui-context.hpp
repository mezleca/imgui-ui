#pragma once

#include <ui/backends/backend.hpp>
#include <ui/layout/geometry.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>

#include <imgui.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace ui_test {
    class TestBackend final : public ui::Backend {
    public:
        bool initialize() override {
            return true;
        }

        bool initialize_imgui() override {
            return true;
        }

        void shutdown_imgui() override {}
        void begin_frame(ImVec4) override {}
        void set_mouse_cursor(ImGuiMouseCursor) override {}
        void render(ImDrawData*) override {}

        float content_scale() const override {
            return 1.0F;
        }

        uint64_t window_id() const override {
            return 1;
        }

        ImVec2 display_size() const override {
            return config().size;
        }
    };

    inline std::unique_ptr<ui::Backend> make_backend() {
        return std::make_unique<TestBackend>();
    }

    class ImGuiContext {
    public:
        explicit ImGuiContext(ImVec2 display_size) : m_previous(ImGui::GetCurrentContext()) {
            m_context = ImGui::CreateContext();
            ImGui::SetCurrentContext(m_context);
            ImGui::GetIO().DisplaySize = display_size;
            ImGui::GetIO().DeltaTime = 1.0F / 60.0F;
            build_fonts();
        }

        ImGuiContext(const ImGuiContext&) = delete;
        ImGuiContext& operator=(const ImGuiContext&) = delete;

        ~ImGuiContext() {
            ImGui::DestroyContext(m_context);
            ImGui::SetCurrentContext(m_previous == m_context ? nullptr : m_previous);
        }

        static void build_fonts() {
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            int bytes_per_pixel = 0;
            ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);
        }

    private:
        ::ImGuiContext* m_previous = nullptr;
        ::ImGuiContext* m_context = nullptr;
    };

    inline void prepare_surface(ui::UI& surface) {
        ImGui::SetCurrentContext(surface.imgui_context());
        ImGuiContext::build_fonts();
    }

    inline void prepare_surface(ui::UI& surface, ImVec2 display_size) {
        prepare_surface(surface);
        ImGui::GetIO().DisplaySize = display_size;
    }

    inline void draw_surface(ui::UI& surface, std::optional<float> delta_time = std::nullopt) {
        surface.begin_frame();
        surface.update(delta_time.value_or(ImGui::GetIO().DeltaTime));
        surface.draw();
        surface.end_frame();
    }

    inline ImVec2 center(const ui::Rect& rect) {
        return {(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
    }

    inline ui::UiEvent pointer_event(ui::EventType type, ImVec2 position, ui::PointerButton button = ui::PointerButton::Left) {
        ui::UiEvent event = ui::UiEvent::make(type);
        event.position = position;
        event.button = button;
        return event;
    }
} // namespace ui_test
