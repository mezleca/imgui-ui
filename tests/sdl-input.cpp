#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <ui/backends/opengl/texture-loader.hpp>
#include <ui/backends/sdl/backend.hpp>
#include <ui/diagnostics/debugger.hpp>
#include <ui/imgui/context-scope.hpp>
#include <ui/layout/container.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/layout/tree-container.hpp>
#include <ui/layout/virtual-layout.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/file-dialog.hpp>
#include <ui/widgets/number-input.hpp>
#include <ui/widgets/text.hpp>

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui-context.hpp"
#include "../vendor/imgui/backends/imgui_impl_opengl3.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

class SdlVideoSession final {
public:
    explicit SdlVideoSession(ImVec2 size) {
        REQUIRE(SDL_Init(SDL_INIT_VIDEO));

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        m_window = SDL_CreateWindow(
            "imgui-ui test", static_cast<int>(size.x), static_cast<int>(size.y), SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
        );
        m_context = m_window == nullptr ? nullptr : SDL_GL_CreateContext(m_window);
        bool supports_opengl = m_context != nullptr;
        if (supports_opengl) {
            SDL_GL_MakeCurrent(m_window, m_context);
            supports_opengl = gladLoadGL(SDL_GL_GetProcAddress) != 0 && GLAD_GL_VERSION_3_3;
        }

        if (!supports_opengl) {
            SKIP("OpenGL 3.3 is unavailable on this runner");
        }
    }

    ~SdlVideoSession() {
        if (m_context != nullptr) SDL_GL_DestroyContext(m_context);
        if (m_window != nullptr) SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    SDL_Window* window() const {
        return m_window;
    }

    SDL_GLContext context() const {
        return m_context;
    }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;
};

static bool process_sdl_event(ui::UI& surface, const SDL_Event& event) {
    return static_cast<ui::SdlBackend&>(surface.backend()).process_event(surface, event);
}

static void draw_frame(ui::UI& surface, std::optional<float> delta_time = std::nullopt) {
    surface.begin_frame();
    surface.update(delta_time.value_or(ImGui::GetIO().DeltaTime));
    surface.draw();
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    glFinish();
    surface.end_frame();
}

static void click_tree(ui::UI& surface, const ui::TreeContainer& tree) {
    const ui::ImGuiContextScope context(surface.imgui_context());
    const ui::Rect rect = tree.layout().visual_rect();
    ImGui::GetIO().AddMousePosEvent(rect.min.x + 4.0F, rect.min.y + 4.0F);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
}

static void release_click(ui::UI& surface) {
    const ui::ImGuiContextScope context(surface.imgui_context());
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
}

TEST_CASE("opengl box shadows cover the spread outside a panel", "[render][regression]") {
    SdlVideoSession sdl({128.0F, 128.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
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

TEST_CASE("opengl blur excludes content outside its rect", "[render][regression]") {
    SdlVideoSession sdl({160.0F, 160.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& parent = surface.root().add<ui::Container>("scroll-parent");
    parent.set_layout({
        .size = {ui::px(100.0F), ui::px(100.0F)},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    parent.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 0.0F, 0.0F, 1.0F}); });
    auto& outside = parent.add<ui::Container>("outside");
    outside.set_layout({
        .size = {ui::px(20.0F), ui::px(60.0F)},
        .placement = {.offset = {80.0F, 0.0F}},
        .in_flow = false,
    });
    outside.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 0.0F, 1.0F, 1.0F}); });
    auto& blurred = parent.add<ui::Container>("blurred");
    blurred.set_size({ui::px(80.0F), ui::px(60.0F)});
    blurred.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 0.0F, 0.0F, 0.0F}).blur(12); });

    draw_frame(surface);
    draw_frame(surface);

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const auto read_pixel = [&](ImVec2 position) {
        unsigned char pixel[4]{};
        const int pixel_x = std::clamp(static_cast<int>(position.x), 0, viewport[2] - 1);
        const int pixel_y = std::clamp(viewport[3] - static_cast<int>(position.y), 0, viewport[3] - 1);
        glReadPixels(pixel_x, pixel_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        return std::array<unsigned char, 4>{pixel[0], pixel[1], pixel[2], pixel[3]};
    };

    const ui::Rect blur_rect = blurred.layout().visual_rect();
    const auto pixel = read_pixel({blur_rect.max.x - 2.0F, blur_rect.min.y + 20.0F});
    CHECK(pixel[2] < 32);
}

TEST_CASE("container border stays above a child widget surface", "[render][regression]") {
    SdlVideoSession sdl({160.0F, 140.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& parent = surface.root().add<ui::Container>("parent");
    parent.set_layout({
        .size = {ui::px(120.0F), ui::px(80.0F)},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    parent.set_spacing(6.0F);
    parent.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .border_thickness(1.0F)
            .padding({});
    });
    auto& spacer = parent.add<ui::Container>("spacer");
    spacer.set_size({ui::grow(), ui::px(12.0F)});
    auto& dialog = parent.add<ui::FileDialogWidget>("file");
    dialog.set_size({ui::grow(), ui::px(60.0F)});
    dialog.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 1.0F, 1.0F}).border(ui::BORDER_NONE).padding({});
    });

    draw_frame(surface);
    draw_frame(surface);

    const ui::Rect parent_rect = parent.layout().visual_rect();
    const ui::Rect dialog_rect = dialog.layout().visual_rect();
    REQUIRE(parent_rect.valid());
    REQUIRE(dialog_rect.valid());
    REQUIRE(dialog_rect.max.y == Catch::Approx(parent_rect.max.y - 1.0F));

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    glReadPixels(
        static_cast<int>(dialog_rect.min.x + 20.0F), viewport[3] - static_cast<int>(parent_rect.max.y - 1.0F), 1, 1, GL_RGBA,
        GL_UNSIGNED_BYTE, pixel
    );
    CHECK(pixel[0] > 32);
    CHECK(pixel[2] < 224);
}

TEST_CASE("scrolling keeps inline overlay panels above earlier content", "[LayerContainer][render][scroll][regression]") {
    class ScrollContainer final : public ui::Container {
    public:
        ScrollContainer() : Container("scroll-overlay-parent") {
            set_size({ui::px(140.0F), ui::px(100.0F)});
            set_scrollable(true);
        }

        bool scroll_to_end = false;

    protected:
        bool paint() override {
            ImGui::SetNextWindowContentSize({140.0F, 400.0F});
            return Container::paint();
        }

        void on_draw_end() override {
            if (scroll_to_end) {
                ImGui::SetScrollY(20.0F);
            }
            Container::on_draw_end();
        }
    };

    SdlVideoSession sdl({240.0F, 180.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& scroll = surface.root().add<ScrollContainer>();

    auto& input = scroll.add<ui::Container>("input");
    input.set_layout({
        .size = {ui::px(100.0F), ui::px(50.0F)},
        .placement = {.offset = {20.0F, 60.0F}},
        .in_flow = false,
    });
    input.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .padding({});
    });

    scroll.add<ui::Container>("filler").set_size({ui::px(140.0F), ui::px(400.0F)});

    auto& overlay = scroll.add<ui::LayerContainer>("overlay");
    auto& panel = overlay.add<ui::Container>("panel");
    panel.set_layout({
        .size = {ui::px(100.0F), ui::px(50.0F)},
        .placement = {.offset = {20.0F, 60.0F}},
        .in_flow = false,
    });
    panel.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 1.0F, 0.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{0.5F, 0.5F, 0.5F, 1.0F})
            .padding({});
    });
    auto& resize = panel.add<ui::ResizableContainer>("resize");
    resize.set_size({ui::grow(), ui::grow()});
    resize.set_resize(ui::ResizeAxes::Both).set_scrollable(true);
    resize.add<ui::Container>("row").set_size({ui::grow(), ui::px(200.0F)});

    draw_frame(surface);
    const ImVec2 initial_size = resize.layout().size();
    scroll.scroll_to_end = true;
    draw_frame(surface);
    draw_frame(surface);

    REQUIRE(panel.layout().visual_rect().valid());
    REQUIRE(resize.layout().visual_rect().valid());
    REQUIRE(resize.layout().size().x == Catch::Approx(initial_size.x));
    REQUIRE(resize.layout().size().y == Catch::Approx(initial_size.y));

    const ui::Rect panel_rect = panel.layout().visual_rect();
    const ImVec2 sample = ui_test::center(panel_rect);
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    glReadPixels(static_cast<int>(sample.x), viewport[3] - static_cast<int>(sample.y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[1] > 160);
    CHECK(pixel[0] < 96);
}

TEST_CASE("scrolled inline layers keep virtual list input separate", "[LayerContainer][VirtualLayout][input][regression]") {
    class VirtualListProbe final : public ui::VirtualLayout {
    public:
        VirtualListProbe() : VirtualLayout("virtual-list", 20.0F) {}

        ImGuiWindow* window = nullptr;
        ImRect scrollbar{};

    protected:
        void draw_children() override {
            window = ImGui::GetCurrentWindow();
            scrollbar = window->ScrollbarY ? ImGui::GetWindowScrollbarRect(window, ImGuiAxis_Y) : ImRect{};
            VirtualLayout::draw_children();
        }
    };

    SdlVideoSession sdl({900.0F, 600.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& demo = surface.root().add<ui::Container>("demo");
    demo.set_size({ui::px(900.0F), ui::px(600.0F)});
    demo.set_scrollable(true).style().padding({12.0F, 12.0F});
    demo.add<ui::Container>("before-list").set_size({ui::grow(), ui::px(300.0F)});

    auto& list = demo.add<VirtualListProbe>();
    list.set_size({ui::grow(), ui::px(100.0F)});
    std::vector<ui::Node*> rows(1000);
    list.set_items(1000, [&list, &rows](size_t index) -> ui::Node& {
        if (rows[index] == nullptr) {
            rows[index] = &list.add<ui::Node>(std::to_string(index));
        }
        return *rows[index];
    });
    demo.add<ui::Container>("after-list").set_size({ui::grow(), ui::px(300.0F)});
    auto& overlay = demo.add<ui::LayerContainer>("demo-overlay");
    auto& panel = overlay.add<ui::Container>("overlay-panel");
    panel.set_layout({
        .size = {ui::px(180.0F), ui::px(80.0F)},
        .placement = {.anchor = ui::Anchor::TopRight, .origin = ui::Anchor::TopRight, .offset = {-20.0F, 20.0F}},
        .in_flow = false,
    });

    const auto send = [&surface](SDL_EventType type, ImVec2 position) {
        SDL_Event event{};
        event.type = type;
        if (type == SDL_EVENT_MOUSE_MOTION) {
            event.motion.windowID = surface.backend().window_id();
            event.motion.x = position.x;
            event.motion.y = position.y;
        } else {
            event.button.windowID = surface.backend().window_id();
            event.button.x = position.x;
            event.button.y = position.y;
            event.button.button = SDL_BUTTON_LEFT;
        }
        return process_sdl_event(surface, event);
    };
    const auto scroll = [&surface](ImVec2 position) {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_WHEEL;
        event.wheel.windowID = surface.backend().window_id();
        event.wheel.mouse_x = position.x;
        event.wheel.mouse_y = position.y;
        event.wheel.y = -1.0F;
        return process_sdl_event(surface, event);
    };

    draw_frame(surface);
    REQUIRE_FALSE(send(SDL_EVENT_MOUSE_MOTION, {100.0F, 240.0F}));
    draw_frame(surface);
    for (int frame = 0; frame < 6; ++frame) {
        scroll({100.0F, 240.0F});
        draw_frame(surface);
    }

    REQUIRE(list.scrollbar.GetHeight() > 0.0F);
    REQUIRE(list.layout().visual_rect().contains({100.0F, 240.0F}));
    scroll({100.0F, 240.0F});
    draw_frame(surface);
    REQUIRE(list.window->Scroll.y > 0.0F);

    const ImVec2 thumb = {(list.scrollbar.Min.x + list.scrollbar.Max.x) * 0.5F, list.scrollbar.Min.y + 4.0F};

    send(SDL_EVENT_MOUSE_MOTION, thumb);
    draw_frame(surface);
    REQUIRE_FALSE(send(SDL_EVENT_MOUSE_BUTTON_DOWN, thumb));
    draw_frame(surface);
    REQUIRE(GImGui->ActiveId == ImGui::GetWindowScrollbarID(list.window, ImGuiAxis_Y));
    send(SDL_EVENT_MOUSE_MOTION, {thumb.x, thumb.y + 40.0F});
    draw_frame(surface);
    send(SDL_EVENT_MOUSE_BUTTON_UP, {thumb.x, thumb.y + 40.0F});
    draw_frame(surface);

    const float dragged_scroll = list.window->Scroll.y;

    const ImVec2 list_position = {list.layout().visual_rect().min.x + 20.0F, list.layout().visual_rect().min.y + 20.0F};
    REQUIRE_FALSE(send(SDL_EVENT_MOUSE_MOTION, list_position));
    draw_frame(surface);

    scroll(list_position);
    draw_frame(surface);

    REQUIRE(list.window->Scroll.y > dragged_scroll);
}

TEST_CASE("container borders stay below popup surfaces", "[render][regression]") {
    SdlVideoSession sdl({160.0F, 120.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& file = surface.root().add<ui::FileDialogWidget>("file");
    file.set_layout({
        .size = {ui::px(120.0F), ui::px(50.0F)},
        .placement = {.offset = {20.0F, 50.0F}},
        .in_flow = false,
    });
    std::string value = "first";
    auto& dropdown = surface.root().add<ui::DropdownWidget>(
        value, std::vector<ui::DropdownOption>{{"first", "first"}, {"second", "second"}}, "dropdown"
    );
    dropdown.set_layout({
        .size = {ui::px(80.0F), ui::px(30.0F)},
        .placement = {.offset = {40.0F, 10.0F}},
        .in_flow = false,
    });
    draw_frame(surface, 0.1F);
    file.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 0.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .border_thickness(20.0F)
            .padding({});
    });
    dropdown.body().style().background_color(ImColor{0.0F, 1.0F, 0.0F, 1.0F}).border(ui::BORDER_NONE).padding({});
    draw_frame(surface, 0.1F);
    CHECK(file.computed_style().border_color().value.Value.x > 0.9F);
    CHECK(dropdown.body().computed_style().background_color().value.Value.y > 0.9F);
    dropdown.open();
    for (int frame = 0; frame < 5; ++frame) {
        draw_frame(surface, 0.1F);
    }
    CHECK(dropdown.body().opacity() > 0.99F);
    const ui::Rect file_rect = file.layout().visual_rect();
    const ImVec2 overlap = {file_rect.min.x + file_rect.size().x * 0.5F, file_rect.min.y + 10.0F};
    REQUIRE(dropdown.body().layout().visual_rect().contains(overlap));

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    glReadPixels(
        static_cast<int>(std::round(overlap.x)), viewport[3] - static_cast<int>(std::round(overlap.y)), 1, 1, GL_RGBA,
        GL_UNSIGNED_BYTE, pixel
    );
    CHECK(pixel[0] < 96);
    CHECK(pixel[1] > 160);
}

TEST_CASE("visible file dialog overflow lets text shadows cross its border", "[render][regression]") {
    SdlVideoSession sdl({180.0F, 140.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});

    auto& backdrop = surface.root().add<ui::Container>("backdrop");
    backdrop.set_layout({.size = {ui::px(180.0F), ui::px(140.0F)}, .in_flow = false});
    backdrop.style().background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F});

    auto& dialog = surface.root().add<ui::FileDialogWidget>("file");
    dialog.set_layout({
        .size = {ui::px(70.0F), ui::px(40.0F)},
        .placement = {.offset = {60.0F, 50.0F}},
        .in_flow = false,
    });
    dialog.set_content_alignment({0.0F, 0.5F});
    dialog.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{0.5F, 0.5F, 0.5F, 1.0F})
            .border_thickness(4.0F)
            .overflow(ui::Overflow::Visible)
            .padding({});
    });
    auto& text = static_cast<ui::TextWidget&>(*dialog.children().front());
    text.configure_all_styles([](ui::Style& style) {
        style.color(ImColor{0.0F, 0.0F, 0.0F, 1.0F})
            .background_color(ImColor{0.0F, 0.0F, 0.0F, 0.0F})
            .border(ui::BORDER_NONE)
            .box_shadow({.spread = 16.0F, .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F}})
            .padding({});
    });

    draw_frame(surface);
    draw_frame(surface);

    const ui::Rect dialog_rect = dialog.layout().visual_rect();
    const ui::Rect text_rect = text.layout().visual_rect();
    REQUIRE(dialog_rect.valid());
    REQUIRE(text_rect.valid());
    REQUIRE(text_rect.min.x < dialog_rect.max.x);

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    const ImVec2 sample = {dialog_rect.min.x - 4.0F, (text_rect.min.y + text_rect.max.y) * 0.5F};
    glReadPixels(static_cast<int>(sample.x), viewport[3] - static_cast<int>(sample.y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] < 80);
    CHECK(pixel[1] < 80);
    CHECK(pixel[2] < 80);

    const ImVec2 border_sample = {dialog_rect.min.x + 1.0F, sample.y};
    glReadPixels(
        static_cast<int>(border_sample.x), viewport[3] - static_cast<int>(border_sample.y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel
    );
    CHECK(pixel[0] < 80);
    CHECK(pixel[1] < 80);
    CHECK(pixel[2] < 80);
}

TEST_CASE("container borders stay below window-layer panels", "[render][regression]") {
    SdlVideoSession sdl({160.0F, 120.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& input = surface.root().add<ui::Container>("input");
    input.set_layout({
        .size = {ui::px(120.0F), ui::px(50.0F)},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    input.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 0.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .border_thickness(2.0F)
            .padding({});
    });

    auto& layer = surface.root().add<ui::LayerContainer>("modal-layer");
    auto& panel = layer.add<ui::Container>("modal-panel");
    panel.set_layout({
        .size = {ui::px(120.0F), ui::px(60.0F)},
        .placement = {.offset = {20.0F, 10.0F}},
        .in_flow = false,
    });
    panel.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 1.0F, 0.0F, 1.0F}).border(ui::BORDER_NONE).padding({});
    });

    draw_frame(surface);
    draw_frame(surface);

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    glReadPixels(70, viewport[3] - 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] < 96);
    CHECK(pixel[1] > 160);
}

TEST_CASE("dropdown trigger shadows render below its label", "[render][regression]") {
    SdlVideoSession sdl({240.0F, 140.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& backdrop = surface.root().add<ui::Container>("backdrop");
    backdrop.set_layout({.size = {ui::px(240.0F), ui::px(140.0F)}, .in_flow = false});
    backdrop.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F}); });

    std::string value = "first";
    auto& dropdown = surface.root().add<ui::DropdownWidget>(
        value, std::vector<ui::DropdownOption>{{"first", "first"}, {"second", "second"}}, "dropdown"
    );
    dropdown.set_layout({
        .size = {ui::px(180.0F), ui::fit()},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    dropdown.set_label("label");
    dropdown.trigger().configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F})
            .border(ui::BORDER_NONE)
            .box_shadow({.spread = 8.0F, .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F}});
    });

    draw_frame(surface);
    draw_frame(surface);

    const ui::Rect trigger_rect = dropdown.trigger().layout().visual_rect();
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    glReadPixels(
        static_cast<int>((trigger_rect.min.x + trigger_rect.max.x) * 0.5F),
        viewport[3] - static_cast<int>(trigger_rect.min.y - 4.0F), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel
    );
    CHECK(pixel[0] < 80);
    CHECK(pixel[1] < 80);
    CHECK(pixel[2] < 80);
}

TEST_CASE("container overflow controls child surfaces", "[render][regression]") {
    const auto sample_overflow = [](ui::Overflow overflow) {
        SdlVideoSession sdl({160.0F, 120.0F});
        ui::Runtime runtime;
        auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
        ui::UI surface(runtime, {.backend = std::move(backend)});
        auto& parent = surface.root().add<ui::Container>("parent");
        parent.set_layout({
            .size = {ui::px(80.0F), ui::px(60.0F)},
            .placement = {.offset = {20.0F, 20.0F}},
            .in_flow = false,
        });
        auto& child = parent.add<ui::Container>("child");
        child.set_layout({
            .size = {ui::px(40.0F), ui::px(30.0F)},
            .placement = {.offset = {70.0F, 10.0F}},
            .in_flow = false,
        });
        draw_frame(surface);
        parent.style().overflow(overflow).border(ui::BORDER_NONE).padding({});
        child.style().background_color(ImColor{0.0F, 0.0F, 1.0F, 1.0F}).padding({});
        for (int frame = 0; frame < 2; ++frame) {
            draw_frame(surface);
        }

        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        unsigned char pixel[4]{};
        glReadPixels(108, viewport[3] - 40, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        return std::array<unsigned char, 4>{pixel[0], pixel[1], pixel[2], pixel[3]};
    };

    const auto visible = sample_overflow(ui::Overflow::Visible);
    CHECK(visible[2] > 200);

    for (const ui::Overflow overflow : {ui::Overflow::Hidden, ui::Overflow::Clip}) {
        const auto clipped = sample_overflow(overflow);
        CHECK(clipped[2] < 80);
    }
}

TEST_CASE("scrolled tree bodies clip oversized checkbox surfaces", "[TreeContainer][render][regression]") {
    class ScrollContainer final : public ui::Container {
    public:
        ScrollContainer() : Container("tree-scroll-parent") {
            set_size({ui::px(160.0F), ui::px(100.0F)});
            set_scrollable(true);
        }

        bool scroll_to_end = false;

    protected:
        bool paint() override {
            ImGui::SetNextWindowContentSize({160.0F, 360.0F});
            return Container::paint();
        }

        void on_draw_end() override {
            if (scroll_to_end) {
                ImGui::SetScrollY(20.0F);
            }
            Container::on_draw_end();
        }
    };

    SdlVideoSession sdl({240.0F, 180.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& backdrop = surface.root().add<ui::Container>("backdrop");
    backdrop.set_layout({.size = {ui::px(240.0F), ui::px(180.0F)}, .in_flow = false});
    backdrop.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F}); });
    auto& parent = surface.root().add<ScrollContainer>();
    parent.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 1.0F, 0.0F, 1.0F}).padding({}); });

    auto& tree = parent.add<ui::TreeContainer>("widgets");
    auto& panel = tree.add<ui::Container>("panel");
    panel.set_size({ui::px(80.0F), ui::px(64.0F)});
    panel.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 0.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .padding({});
    });
    bool checked = false;
    auto& checkbox = panel.add<ui::CheckboxWidget>(checked, "oversized");
    checkbox.set_size({ui::px(160.0F), ui::px(40.0F)});
    checkbox.set_box_size(120.0F);
    checkbox.frame().configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 1.0F, 1.0F}).border(ui::BORDER_NONE).padding({});
    });
    parent.add<ui::Container>("filler").set_size({ui::px(160.0F), ui::px(280.0F)});

    draw_frame(surface);
    click_tree(surface, tree);
    draw_frame(surface);
    release_click(surface);
    draw_frame(surface);
    parent.scroll_to_end = true;
    draw_frame(surface);
    draw_frame(surface);

    const ui::Rect panel_rect = panel.layout().visual_rect();
    const ui::Rect frame_rect = checkbox.frame().layout().visual_rect();
    REQUIRE(panel_rect.valid());
    REQUIRE(frame_rect.valid());
    REQUIRE(frame_rect.max.x > panel_rect.max.x);

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    glReadPixels(
        static_cast<int>(panel_rect.max.x + 5.0F), viewport[3] - static_cast<int>((frame_rect.min.y + frame_rect.max.y) * 0.5F),
        1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel
    );
    CHECK(pixel[1] > 160);
    CHECK(pixel[2] < 80);
}

TEST_CASE("tree widget viewports clip oversized file dialog surfaces", "[TreeContainer][render][regression]") {
    SdlVideoSession sdl({240.0F, 180.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& backdrop = surface.root().add<ui::Container>("backdrop");
    backdrop.set_layout({.size = {ui::px(240.0F), ui::px(180.0F)}, .in_flow = false});
    backdrop.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 1.0F, 0.0F, 1.0F}); });

    auto& visual = surface.root().add<ui::Container>("visual-tests");
    visual.set_layout({
        .size = {ui::px(180.0F), ui::px(140.0F)},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    visual.set_scrollable(true);
    visual.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 1.0F, 0.0F, 1.0F}); });
    auto& outer = visual.add<ui::TreeContainer>("ui visual tests");
    auto& tree = outer.add<ui::TreeContainer>("widgets");
    auto& widgets = tree.add<ui::Container>("widget-view");
    widgets.set_size({ui::px(100.0F), ui::px(80.0F)});
    widgets.set_scrollable(true);
    widgets.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 0.0F, 1.0F})
            .border(ui::BORDER_ALL)
            .border_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F})
            .padding({12.0F, 12.0F});
    });
    auto& dialog = widgets.add<ui::FileDialogWidget>("file dialog");
    dialog.set_size({ui::px(240.0F), ui::px(100.0F)});
    dialog.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 1.0F, 1.0F}).border(ui::BORDER_NONE).padding({});
    });
    widgets.add<ui::Container>("filler").set_size({ui::px(100.0F), ui::px(200.0F)});

    draw_frame(surface);
    click_tree(surface, outer);
    draw_frame(surface);
    release_click(surface);
    draw_frame(surface);
    click_tree(surface, tree);
    draw_frame(surface);
    release_click(surface);
    draw_frame(surface);

    const ui::Rect viewport_rect = widgets.layout().visual_rect();
    const ui::Rect dialog_rect = dialog.layout().visual_rect();
    REQUIRE(viewport_rect.valid());
    REQUIRE(dialog_rect.valid());
    REQUIRE(dialog_rect.max.x > viewport_rect.max.x);

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned char pixel[4]{};
    glReadPixels(
        static_cast<int>(viewport_rect.max.x + 5.0F),
        viewport[3] - static_cast<int>((dialog_rect.min.y + dialog_rect.max.y) * 0.5F), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel
    );
    CAPTURE(viewport_rect.min.x, viewport_rect.max.x, viewport_rect.min.y, viewport_rect.max.y);
    CAPTURE(dialog_rect.min.x, dialog_rect.max.x, dialog_rect.min.y, dialog_rect.max.y);
    CAPTURE(pixel[0], pixel[1], pixel[2], pixel[3]);
    CHECK(pixel[2] < 80);
}

TEST_CASE("nested tree shadows escape their parent body clip", "[render][regression]") {
    SdlVideoSession sdl({200.0F, 180.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    auto& backdrop = surface.root().add<ui::Container>("backdrop");
    backdrop.set_layout({
        .size = {ui::px(200.0F), ui::px(180.0F)},
        .in_flow = false,
    });
    backdrop.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F}); });
    auto& visual_tests = surface.root().add<ui::Container>("visual-tests");
    visual_tests.set_layout({
        .size = {ui::px(140.0F), ui::px(140.0F)},
        .placement = {.offset = {40.0F, 20.0F}},
        .in_flow = false,
    });
    visual_tests.set_scrollable(true);
    auto& outer = visual_tests.add<ui::TreeContainer>("outer");
    auto& inner = outer.add<ui::TreeContainer>("inner");
    inner.set_size({ui::grow(), ui::px(80.0F)});
    inner.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F})
            .box_shadow({.spread = 16.0F, .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F}});
    });
    auto& child = inner.add<ui::Container>("child");
    child.set_size({ui::grow(), ui::px(40.0F)});
    child.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{1.0F, 1.0F, 1.0F, 1.0F})
            .box_shadow({.spread = 16.0F, .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F}});
    });
    auto& filler = visual_tests.add<ui::Container>("filler");
    filler.set_size({ui::grow(), ui::px(200.0F)});

    draw_frame(surface);
    click_tree(surface, outer);
    draw_frame(surface);
    release_click(surface);
    draw_frame(surface);
    click_tree(surface, inner);
    draw_frame(surface);
    release_click(surface);
    draw_frame(surface);
    draw_frame(surface);

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const ui::Rect inner_rect = inner.layout().visual_rect();
    const ui::Rect child_rect = child.layout().visual_rect();
    REQUIRE(inner_rect.valid());
    REQUIRE(child_rect.valid());
    unsigned char pixel[4]{};
    glReadPixels(
        static_cast<int>(inner_rect.min.x - 8.0F), viewport[3] - static_cast<int>(inner_rect.min.y + 10.0F), 1, 1, GL_RGBA,
        GL_UNSIGNED_BYTE, pixel
    );
    CHECK(pixel[0] < 80);
    CHECK(pixel[1] < 80);
    CHECK(pixel[2] < 80);
    glReadPixels(
        static_cast<int>(child_rect.min.x - 8.0F), viewport[3] - static_cast<int>(child_rect.min.y + 20.0F), 1, 1, GL_RGBA,
        GL_UNSIGNED_BYTE, pixel
    );
    CHECK(pixel[0] < 80);
    CHECK(pixel[1] < 80);
    CHECK(pixel[2] < 80);
}

TEST_CASE("gif texture data decodes into an opengl texture", "[texture][gif]") {
    static constexpr char gif_data[] = "GIF89a\x01\x00\x01\x00\x80\x00\x00\x00\x00\x00\xff\xff\xff!\xf9\x04\x01\x00\x00\x00\x00,"
                                       "\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x02D\x01\x00;";
    constexpr std::string_view gif{gif_data, sizeof(gif_data) - 1U};

    SdlVideoSession sdl({128.0F, 128.0F});
    ui::Runtime runtime({.texture_loader = std::make_unique<ui::OpenGLTextureLoader>()});
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    ui::Texture* texture = runtime.textures().add("gif", gif);
    REQUIRE(texture != nullptr);
    REQUIRE(texture->size().x == 1.0F);
    REQUIRE(texture->size().y == 1.0F);

    const ui::ImGuiContextScope context(surface.imgui_context());
    const ImTextureID id = texture->get(texture->size());
    REQUIRE(id != ImTextureID{});
    REQUIRE(glIsTexture(static_cast<GLuint>(id)) == GL_TRUE);
}

TEST_CASE("handled button clicks still release ImGui mouse state", "[input][regression]") {
    SdlVideoSession sdl({320.0F, 240.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    const auto surface_context = ui_test::prepare_surface(surface);

    int click_count = 0;
    auto& button = surface.root().add<ui::ButtonWidget>("test button", ui::LayoutSize{ui::px(160.0F), ui::px(36.0F)});
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
    REQUIRE_FALSE(process_sdl_event(surface, down));

    SDL_Event up = down;
    up.type = SDL_EVENT_MOUSE_BUTTON_UP;
    REQUIRE(process_sdl_event(surface, up));
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

TEST_CASE("blocked modal number sliders keep receiving sdl drag motion", "[input][regression]") {
    SdlVideoSession sdl({900.0F, 600.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    const auto surface_context = ui_test::prepare_surface(surface, {900.0F, 600.0F});

    auto& modal_layer = surface.root().add<ui::LayerContainer>("modal-layer");
    modal_layer.set_input_mode(ui::InputMode::Blocker);
    modal_layer.configure_all_styles([](ui::Style& style) {
        style.background_color(ImColor{0.0F, 0.0F, 0.0F, 0.0F}).blur(5.0F);
    });

    auto& modal = modal_layer.add<ui::Container>("modal");
    modal.set_size({ui::px(480.0F), ui::px(220.0F)});
    modal.set_layout({
        .size = {ui::px(480.0F), ui::px(220.0F)},
        .placement = {.anchor = ui::Anchor::Center, .origin = ui::Anchor::Center},
        .in_flow = false,
    });
    modal.set_spacing(10.0F);

    int value = 5;
    auto& input = modal.add<ui::NumberInputWidget>(value, "modal-blur");
    input.set_range(0, 32).set_size({ui::px(180.0F), ui::px(48.0F)});
    input.set_on_change([&modal_layer, &value] {
        modal_layer.configure_all_styles([&value](ui::Style& style) { style.blur(value); });
    });
    surface.input_router().set_focus(modal_layer);
    const SDL_WindowID window_id = surface.backend().window_id();
    const auto send_pointer = [&](SDL_EventType type, ImVec2 position) {
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
        process_sdl_event(surface, event);
    };

    ui_test::draw_surface(surface);
    const ui::Rect rect = input.layout().visual_rect();
    const ImVec2 press = {rect.min.x + rect.size().x * 0.70F, ui_test::center(rect).y};

    send_pointer(SDL_EVENT_MOUSE_MOTION, press);
    ui_test::draw_surface(surface);
    send_pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, press);
    ui_test::draw_surface(surface);
    const ImGuiID slider_id = GImGui->ActiveId;
    REQUIRE(slider_id != 0);
    int previous_value = value;

    for (float ratio : {0.75F, 0.80F, 0.85F, 0.90F, 0.95F}) {
        const ImVec2 position = {rect.min.x + rect.size().x * ratio, press.y};
        send_pointer(SDL_EVENT_MOUSE_MOTION, position);
        ui_test::draw_surface(surface);
        REQUIRE(GImGui->ActiveId == slider_id);
        REQUIRE(value > previous_value);
        previous_value = value;
    }

    // blur reaching zero removes the visual effect while the active slider keeps the same imgui parent.
    for (float ratio : {0.20F, 0.05F}) {
        const ImVec2 position = {rect.min.x + rect.size().x * ratio, press.y};
        send_pointer(SDL_EVENT_MOUSE_MOTION, position);
        ui_test::draw_surface(surface);
        REQUIRE(GImGui->ActiveId == slider_id);
        REQUIRE(value <= previous_value);
        previous_value = value;
    }

    REQUIRE(value == 0);
    const ImVec2 restore = {rect.min.x + rect.size().x * 0.80F, press.y};
    send_pointer(SDL_EVENT_MOUSE_MOTION, restore);
    ui_test::draw_surface(surface);
    REQUIRE(GImGui->ActiveId == slider_id);
    REQUIRE(value > 0);

    const int value_before_leaving_modal = value;
    send_pointer(SDL_EVENT_MOUSE_MOTION, {850.0F, press.y});
    ui_test::draw_surface(surface);
    const int outside_value = value;
    REQUIRE(outside_value != value_before_leaving_modal);
    send_pointer(SDL_EVENT_MOUSE_BUTTON_UP, {850.0F, press.y});
    ui_test::draw_surface(surface);
    REQUIRE(GImGui->ActiveId == 0);
    send_pointer(SDL_EVENT_MOUSE_MOTION, {rect.min.x + rect.size().x * 0.80F, press.y});
    ui_test::draw_surface(surface);
    REQUIRE(value == outside_value);
}

TEST_CASE("debugger hotkey is received through the sdl backend", "[input][regression]") {
    SdlVideoSession sdl({320.0F, 240.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(
        runtime, {
                     .backend = std::move(backend),
                     .enable_debugger = true,
                 }
    );
    REQUIRE(surface.debugger() != nullptr);
    surface.debugger()->set_open(false);
    const auto surface_context = ui_test::prepare_surface(surface);

    const SDL_WindowID window_id = surface.backend().window_id();
    SDL_Event shift_down{};
    shift_down.type = SDL_EVENT_KEY_DOWN;
    shift_down.key.windowID = window_id;
    shift_down.key.key = SDLK_LSHIFT;
    shift_down.key.scancode = SDL_SCANCODE_LSHIFT;
    shift_down.key.mod = SDL_KMOD_SHIFT;
    REQUIRE_FALSE(process_sdl_event(surface, shift_down));

    SDL_Event d_down = shift_down;
    d_down.key.key = SDLK_D;
    d_down.key.scancode = SDL_SCANCODE_D;
    REQUIRE_FALSE(process_sdl_event(surface, d_down));

    surface.begin_frame();
    REQUIRE(surface.debugger()->is_open());
    surface.end_frame();
}

TEST_CASE("pointer blocker prevents native content mutation but keeps descendants interactive", "[input][regression]") {
    SdlVideoSession sdl({320.0F, 240.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    const auto surface_context = ui_test::prepare_surface(surface);

    bool content_value = false;
    bool overlay_value = false;
    std::string dropdown_value = "one";
    auto& content_checkbox = surface.root().add<ui::CheckboxWidget>(content_value, "content");
    content_checkbox.set_layout({
        .size = {ui::fit(), ui::fit()},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });
    auto& content_dropdown = surface.root().add<ui::DropdownWidget>(
        dropdown_value, std::vector<ui::DropdownOption>{{"one", "one"}, {"two", "two"}}, "dropdown"
    );
    content_dropdown.set_size({ui::px(180.0F), ui::px(52.0F)});
    content_dropdown.set_layout({
        .size = {ui::px(180.0F), ui::px(52.0F)},
        .placement = {.offset = {20.0F, 60.0F}},
        .in_flow = false,
    });

    auto& blocker = surface.root().add<ui::LayerContainer>("blocker", ui::LayerMode::Inline);
    blocker.set_input_mode(ui::InputMode::Blocker);
    auto& overlay_checkbox = blocker.add<ui::CheckboxWidget>(overlay_value, "overlay");
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
        CHECK(process_sdl_event(surface, motion) == expected_handled);

        SDL_Event down{};
        down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        down.button.windowID = window_id;
        down.button.x = position.x;
        down.button.y = position.y;
        down.button.button = SDL_BUTTON_LEFT;
        CHECK(process_sdl_event(surface, down) == expected_handled);

        SDL_Event up = down;
        up.type = SDL_EVENT_MOUSE_BUTTON_UP;
        CHECK(process_sdl_event(surface, up) == expected_handled);
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
    SdlVideoSession sdl({320.0F, 240.0F});
    ui::Runtime runtime;
    auto backend = std::make_unique<ui::SdlBackend>(sdl.window(), sdl.context());
    ui::UI surface(runtime, {.backend = std::move(backend)});
    const auto surface_context = ui_test::prepare_surface(surface);

    std::string value = "one";
    int changes = 0;
    auto& dropdown = surface.root().add<ui::DropdownWidget>(
        value, std::vector<ui::DropdownOption>{{"one", "one"}, {"two", "two"}}, "dropdown"
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
        return process_sdl_event(surface, event);
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

    ui_test::draw_surface(surface, 1.0F / 60.0F);
    REQUIRE(dropdown.body().opacity() < visible_opacity);
    for (int frame = 0; frame < 8; ++frame) {
        ui_test::draw_surface(surface, 1.0F / 60.0F);
    }
    REQUIRE_FALSE(dropdown.body().visually_visible());
    REQUIRE(changes == 1);
}
