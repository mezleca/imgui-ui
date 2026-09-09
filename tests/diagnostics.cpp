#include <catch2/catch_test_macros.hpp>

#include <ui/diagnostics/debugger.hpp>
#include <ui/diagnostics/profiler.hpp>
#include <ui/imgui/effects/effects.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/style/styled-node.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/color-picker.hpp>

#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>

#include "imgui-context.hpp"

struct EffectProbe {
    int initialized = 0;
    int frames = 0;
    int shutdown = 0;
};

static bool initialize_test_effect(void* data) {
    ++static_cast<EffectProbe*>(data)->initialized;
    return true;
}

static void begin_test_effect_frame(void* data) {
    ++static_cast<EffectProbe*>(data)->frames;
}

static void shutdown_test_effect(void* data) {
    ++static_cast<EffectProbe*>(data)->shutdown;
}

static void render_test_effect(const ImDrawList*, const ImDrawCmd*) {}

static int draw_list_index(const ImDrawData& draw_data, std::string_view owner) {
    for (int index = 0; index < draw_data.CmdListsCount; ++index) {
        const ImDrawList* draw_list = draw_data.CmdLists[index];
        if (draw_list != nullptr && draw_list->_OwnerName != nullptr &&
            std::string_view{draw_list->_OwnerName}.find(owner) != std::string_view::npos) {
            return index;
        }
    }

    return -1;
}

TEST_CASE("debugger renders in the target surface and intercepts its overlay") {
    ui::Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend(), .enable_debugger = true});
    REQUIRE(surface.debugger() != nullptr);
    REQUIRE_FALSE(surface.debugger()->is_open());

    ui_test::prepare_surface(surface, {320.0F, 240.0F});

    surface.begin_frame();
    surface.end_frame();
    REQUIRE_FALSE(surface.debugger()->is_open());

    surface.debugger()->set_open(true);
    const auto draw_modal_frame = [&surface] {
        surface.begin_frame();
        const bool visible = ImGui::Begin("modal-layer");
        ImGui::TextUnformatted("modal");
        const int command_count = ImGui::GetWindowDrawList()->CmdBuffer.Size;
        ImGui::End();
        surface.update(ImGui::GetIO().DeltaTime);
        surface.draw();
        surface.end_frame();
        return std::pair{visible, command_count};
    };

    const auto [modal_visible, command_count] = draw_modal_frame();
    REQUIRE(modal_visible);
    REQUIRE(command_count > 1);
    draw_modal_frame();

    ImGui::SetCurrentContext(surface.imgui_context());
    const ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    const int modal_index = draw_list_index(*draw_data, "modal-layer");
    const int debugger_index = draw_list_index(*draw_data, "##debugger-sections");
    REQUIRE(modal_index >= 0);
    REQUIRE(debugger_index >= 0);
    REQUIRE(modal_index < debugger_index);

    surface.begin_frame();

    ui::UiEvent down = ui_test::pointer_event(ui::EventType::PointerDown, {10.0F, 10.0F});
    REQUIRE(surface.dispatch(down));

    ui::UiEvent up = ui_test::pointer_event(ui::EventType::PointerUp, down.position);
    REQUIRE(surface.dispatch(up));
    surface.end_frame();
}

TEST_CASE("debugger hotkey toggles on the target surface") {
    ui::Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend(), .enable_debugger = true});
    REQUIRE(surface.debugger() != nullptr);
    REQUIRE_FALSE(surface.debugger()->is_open());

    ui_test::prepare_surface(surface, {320.0F, 240.0F});
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, true);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_D, true);

    surface.begin_frame();

    REQUIRE(surface.debugger()->is_open());
    surface.end_frame();
}

TEST_CASE("debugger clicks preserve an open popup") {
    ui::Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend(), .enable_debugger = true});
    ImColor color = {0.26F, 0.59F, 0.98F, 1.0F};
    auto& picker = surface.root().add<ui::ColorPickerWidget>(surface, color);

    ui_test::prepare_surface(surface, {900.0F, 600.0F});
    ui_test::draw_surface(surface);

    const ImVec2 preview = ui_test::center(picker.preview().layout().visual_rect());
    ui::UiEvent down = ui_test::pointer_event(ui::EventType::PointerDown, preview);
    ui::UiEvent up = ui_test::pointer_event(ui::EventType::PointerUp, preview);
    surface.dispatch(down);
    surface.dispatch(up);
    ui_test::draw_surface(surface);
    REQUIRE(picker.is_open());

    surface.debugger()->set_open(true);
    ui_test::draw_surface(surface);
    REQUIRE(picker.is_open());

    const ui::Rect debugger_rect = surface.debugger()->layout().visual_rect();
    const ImVec2 inspect = {debugger_rect.min.x + 20.0F, debugger_rect.min.y + 20.0F};
    down = ui_test::pointer_event(ui::EventType::PointerDown, inspect);
    REQUIRE(surface.dispatch(down));
    REQUIRE_FALSE(down.native_input_blocked);
    ImGui::GetIO().AddMousePosEvent(inspect.x, inspect.y);
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    ui_test::draw_surface(surface);
    REQUIRE(picker.is_open());

    up = ui_test::pointer_event(ui::EventType::PointerUp, inspect);
    REQUIRE(surface.dispatch(up));
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    ui_test::draw_surface(surface);
    REQUIRE(picker.is_open());
}

TEST_CASE("focused debugger blocks application hover") {
    ui::Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend(), .enable_debugger = true});
    bool value = false;
    auto& checkbox = surface.root().add<ui::CheckboxWidget>(surface, value, "application");
    checkbox.set_layout({
        .size = {ui::px(120.0F), ui::px(40.0F)},
        .placement = {.offset = {20.0F, 20.0F}},
        .in_flow = false,
    });

    ui_test::prepare_surface(surface, {900.0F, 600.0F});

    const auto draw_frame = [&surface] {
        ImGui::GetIO().MousePos = {30.0F, 30.0F};
        ui_test::draw_surface(surface);
    };

    draw_frame();
    REQUIRE(checkbox.input_state().hovered);

    surface.debugger()->set_open(true);
    draw_frame();
    REQUIRE_FALSE(checkbox.input_state().hovered);

    ui::UiEvent down = ui_test::pointer_event(ui::EventType::PointerDown, {30.0F, 30.0F});
    REQUIRE(surface.dispatch(down));

    ui::UiEvent up = ui_test::pointer_event(ui::EventType::PointerUp, down.position);
    REQUIRE(surface.dispatch(up));

    draw_frame();
    REQUIRE(checkbox.input_state().hovered);
}

TEST_CASE("debugger renders as a panel in the surface layout") {
    ui::Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend(), .enable_debugger = true});
    surface.debugger()->set_open(true);

    ui_test::prepare_surface(surface, {900.0F, 600.0F});
    ui_test::draw_surface(surface);

    ImGui::SetCurrentContext(surface.imgui_context());
    const ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    const int debugger_index = draw_list_index(*draw_data, "##debugger-sections");
    REQUIRE(debugger_index >= 0);
    REQUIRE(surface.debugger()->layout().visual_rect().valid());
    REQUIRE(surface.debugger()->layout().visual_rect().min.x > 0.0F);
}

TEST_CASE("debugger exposes the content resize handle", "[Debugger][ResizableContainer][regression]") {
    ui::Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend(), .enable_debugger = true});
    surface.debugger()->set_open(true);

    ui_test::prepare_surface(surface, {900.0F, 600.0F});
    ui_test::draw_surface(surface);

    auto* content = dynamic_cast<ui::ResizableContainer*>(&surface.root());
    REQUIRE(content != nullptr);
    const ui::Rect initial = content->layout().visual_rect();
    REQUIRE(initial.valid());

    const ImVec2 handle = {initial.max.x - 5.0F, initial.max.y - 5.0F};
    ui::UiEvent down = ui_test::pointer_event(ui::EventType::PointerDown, handle);
    REQUIRE(surface.dispatch(down));
    REQUIRE(content->resizing());

    ui::UiEvent move = ui_test::pointer_event(ui::EventType::PointerMove, {handle.x - 100.0F, handle.y});
    REQUIRE(surface.dispatch(move));

    ui::UiEvent up = ui_test::pointer_event(ui::EventType::PointerUp, move.position);
    REQUIRE(surface.dispatch(up));
    REQUIRE_FALSE(content->resizing());

    ui_test::draw_surface(surface);
    REQUIRE(content->layout().size().x < initial.size().x);
    REQUIRE(surface.debugger()->layout().visual_rect().min.x < 900.0F - 440.0F + 1.0F);
    REQUIRE(surface.debugger()->layout().visual_rect().max.x == 900.0F);
}

TEST_CASE("effect registry manages lifecycle and draw submission") {
    EffectProbe probe;
    ui::EffectRegistry effects;
    const ui::EffectId id = effects.register_effect(
        {render_test_effect, initialize_test_effect, begin_test_effect_frame, shutdown_test_effect, &probe}
    );

    REQUIRE(id != 0);
    REQUIRE(effects.initialize());
    REQUIRE(probe.initialized == 1);

    effects.begin_frame();
    REQUIRE(probe.frames == 1);

    ui_test::ImGuiContext context({100.0F, 100.0F});
    ImGui::NewFrame();
    ImGui::Begin("effect-registry-test");
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    REQUIRE(effects.submit(*draw_list, id, &probe));
    REQUIRE(draw_list->CmdBuffer.Size >= 2);
    bool found_callback = false;
    for (const ImDrawCmd& command : draw_list->CmdBuffer) {
        found_callback = found_callback || command.UserCallback == render_test_effect;
    }
    REQUIRE(found_callback);
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(effects.unregister_effect(id));
    REQUIRE(probe.shutdown == 1);
    REQUIRE_FALSE(effects.submit(*draw_list, id, &probe));
}

TEST_CASE("ui profiler records completed zones and frame metrics") {
    ui::Profiler profiler;
    profiler.set_enabled(true);
    profiler.begin_frame();

    {
        ui::ScopedProfileZone outer(&profiler, "outer", 10);
        ui::ScopedProfileZone inner(&profiler, "Node::draw", 20);
    }
    profiler.record_frame_metrics(2, 7);

    profiler.end_frame();
    const std::span<const ui::ProfileEvent> events = profiler.latest_events();

    REQUIRE(events.size() == 2);
    REQUIRE(events[0].name == "outer");
    REQUIRE(events[1].name == "Node::draw");
    REQUIRE(events[0].end >= events[0].start);
    REQUIRE(events[1].end >= events[1].start);
    REQUIRE(profiler.node_duration_ms(20) > 0.0);
    REQUIRE(profiler.dropped_events() == 0);
    REQUIRE(profiler.latest_metrics().input_entries == 2);
    REQUIRE(profiler.latest_metrics().input_entry_checks == 7);
    REQUIRE(profiler.has_report());
    REQUIRE(profiler.save_report());

    {
        std::ifstream report(profiler.output_path());
        const std::string contents{std::istreambuf_iterator<char>(report), {}};
        REQUIRE(contents.find("latest.nodes_drawn =") != std::string::npos);
        REQUIRE(contents.find("latest.input_entry_checks =") != std::string::npos);
        REQUIRE(contents.find("latest.update_ms =") != std::string::npos);
        REQUIRE(contents.find("latest.measure_ms =") != std::string::npos);
        REQUIRE(contents.find("latest.layout_ms =") != std::string::npos);
        REQUIRE(contents.find("latest.draw_ms =") != std::string::npos);
        REQUIRE(contents.find("latest.input_ms =") != std::string::npos);
        REQUIRE(contents.find("latest.render_ms =") != std::string::npos);
        REQUIRE(contents.find("memory") == std::string::npos);
        REQUIRE(contents.find("style_") == std::string::npos);
        REQUIRE(contents.find("draw_commands") == std::string::npos);
    }

    std::filesystem::remove(profiler.output_path());

    profiler.clear_report();
    REQUIRE_FALSE(profiler.has_report());
}

TEST_CASE("ui profiler separates root update and draw time") {
    ui::Profiler profiler;
    profiler.set_root_node(10);
    profiler.set_enabled(true);
    profiler.begin_frame();

    {
        ui::ScopedProfileZone update(&profiler, "Node::update", 10);
    }
    {
        ui::ScopedProfileZone measure(&profiler, "Node::measure", 10);
    }
    {
        ui::ScopedProfileZone draw(&profiler, "Node::draw", 10);
    }
    {
        ui::ScopedProfileZone input(&profiler, "Node::input", 10);
    }
    {
        ui::ScopedProfileZone layout(&profiler, "Node::layout", 20);
    }
    {
        ui::ScopedProfileZone render(&profiler, "UI::render");
    }

    profiler.end_frame();
    const ui::ProfileFrameMetrics& metrics = profiler.latest_metrics();
    REQUIRE(metrics.update_ms > 0.0);
    REQUIRE(metrics.measure_ms > 0.0);
    REQUIRE(metrics.layout_ms > 0.0);
    REQUIRE(metrics.draw_ms > 0.0);
    REQUIRE(metrics.input_ms > 0.0);
    REQUIRE(metrics.render_ms > 0.0);
}

TEST_CASE("ui profiler reports event overflow without corrupting the frame") {
    ui::Profiler profiler;
    profiler.set_enabled(true);
    profiler.begin_frame();

    for (std::size_t index = 0; index < ui::Profiler::EVENT_CAPACITY + 1; ++index) {
        ui::ScopedProfileZone zone(&profiler, "overflow");
    }

    profiler.end_frame();

    REQUIRE(profiler.latest_events().size() == ui::Profiler::EVENT_CAPACITY);
    REQUIRE(profiler.dropped_events() == 1);
}

TEST_CASE("ui profiler counts drawn nodes") {
    ui_test::ImGuiContext context({100.0F, 100.0F});
    ImGui::NewFrame();

    ui::Profiler profiler;
    profiler.set_enabled(true);
    ui::StyledNode node("styled");
    node.set_profiler(&profiler);
    node.fade_in();

    profiler.begin_frame();
    node.update(0.01F);
    node.draw();
    profiler.end_frame();

    const ui::ProfileFrameMetrics& metrics = profiler.latest_metrics();
    REQUIRE(metrics.nodes_drawn == 1);

    ImGui::EndFrame();
}
