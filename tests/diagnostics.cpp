#include <catch2/catch_test_macros.hpp>

#include <ui/diagnostics/profiler.hpp>
#include <ui/style/styled-node.hpp>

#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <iterator>

#include "imgui-context.hpp"

TEST_CASE("ui profiler exposes completed nested zones") {
    ui::Profiler profiler;
    profiler.set_enabled(true);
    profiler.begin_frame();

    {
        ui::ScopedProfileZone outer(&profiler, "outer", 10);
        ui::ScopedProfileZone inner(&profiler, "inner", 20);
    }

    profiler.end_frame();
    const std::span<const ui::ProfileEvent> events = profiler.latest_events();

    REQUIRE(events.size() == 2);
    REQUIRE(events[0].name == "outer");
    REQUIRE(events[0].depth == 0);
    REQUIRE(events[1].name == "inner");
    REQUIRE(events[1].depth == 1);
    REQUIRE(events[0].end >= events[0].start);
    REQUIRE(events[1].end >= events[1].start);
    REQUIRE(profiler.node_duration_ms(20) >= 0.0);
    REQUIRE(profiler.dropped_events() == 0);
    REQUIRE(profiler.has_report());
    REQUIRE(profiler.save_report());

    {
        std::ifstream report(profiler.output_path());
        const std::string contents{std::istreambuf_iterator<char>(report), {}};
        REQUIRE(contents.find("latest.style_pushes =") != std::string::npos);
        REQUIRE(contents.find("latest.active_transitions =") != std::string::npos);
        REQUIRE(contents.find("latest.update_ms =") != std::string::npos);
        REQUIRE(contents.find("latest.draw_ms =") != std::string::npos);
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
        ui::ScopedProfileZone draw(&profiler, "Node::draw", 10);
    }

    profiler.end_frame();
    const ui::ProfileFrameMetrics& metrics = profiler.latest_metrics();
    REQUIRE(metrics.update_ms >= 0.0);
    REQUIRE(metrics.draw_ms >= 0.0);
}

TEST_CASE("ui profiler records styled draw scopes and transitions") {
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
    REQUIRE(metrics.style_pushes == 1);
    REQUIRE(metrics.style_pops == 1);
    REQUIRE(metrics.active_transitions == 1);

    ImGui::EndFrame();
}
