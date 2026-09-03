#include <catch2/catch_test_macros.hpp>

#include <ui/diagnostics/profiler.hpp>
#include <ui/imgui/effects/effects.hpp>
#include <ui/style/styled-node.hpp>

#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <iterator>

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
