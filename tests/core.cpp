#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "imgui-context.hpp"
#include <ui/imgui/draw.hpp>
#include <ui/imgui/effects/blur/blur.hpp>
#include <ui/imgui/effects/effects.hpp>
#include <ui/imgui/effects/shadow/shadow.hpp>
#include <ui/layout/container.hpp>
#include <ui/layout/geometry.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/layout/tree-container.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/dropdown.hpp>

#include <imgui_internal.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

using namespace ui;

static const BoxShadowRegion* collected_shadow_region = nullptr;
static const BlurRegion* collected_blur_region = nullptr;
static const ImDrawList* collected_effect_draw_list = nullptr;

static void collect_shadow_callback(void*, const ImDrawList* draw_list, const ImDrawCmd*, const void* command) {
    collected_shadow_region = static_cast<const BoxShadowRegion*>(command);
    collected_effect_draw_list = draw_list;
}

static void collect_blur_callback(void*, const ImDrawList*, const ImDrawCmd*, const void* command) {
    collected_blur_region = static_cast<const BlurRegion*>(command);
}

static void render_effect_commands(const ImDrawData& draw_data) {
    for (int list_index = 0; list_index < draw_data.CmdListsCount; ++list_index) {
        const ImDrawList* draw_list = draw_data.CmdLists[list_index];
        for (const ImDrawCmd& command : draw_list->CmdBuffer) {
            if (command.UserCallback != nullptr && command.UserCallback != ImDrawCallback_ResetRenderState) {
                command.UserCallback(draw_list, &command);
            }
        }
    }
}

template <typename Command>
static void initialize_effect(EffectRegistry& effects, EffectSlot slot, EffectRender render) {
    effects.register_effect<Command>(slot, {render});
    effects.initialize();
    effects.begin_frame();
}

TEST_CASE("ui does not write to imgui's fallback window") {
    Runtime runtime;
    UI surface = ui_test::make_surface(runtime);
    surface.root().add<Node>("content");

    const auto surface_context = ui_test::prepare_surface(surface, {320.0F, 240.0F});
    surface.begin_frame();

    ImGuiWindow* fallback = ImGui::FindWindowByName("Debug##Default");
    REQUIRE(fallback != nullptr);
    REQUIRE_FALSE(fallback->WriteAccessed);

    surface.update(ImGui::GetIO().DeltaTime);
    REQUIRE_FALSE(fallback->WriteAccessed);

    surface.draw();
    REQUIRE_FALSE(fallback->WriteAccessed);

    surface.end_frame();
    REQUIRE_FALSE(fallback->Active);
}

TEST_CASE("rounded border paths split corners between adjacent sides") {
    const BorderPath path = rounded_rect_border_path({{10.0F, 20.0F}, {110.0F, 80.0F}}, 12.0F);

    REQUIRE(path.segments[0].type == BorderPathSegmentType::Line);
    REQUIRE(path.segments[0].length == Catch::Approx(76.0F));
    REQUIRE(path.segments[1].type == BorderPathSegmentType::Arc);
    REQUIRE(path.segments[1].sides == BORDER_TOP);
    REQUIRE(path.segments[2].sides == BORDER_RIGHT);
    REQUIRE(path.segments[5].sides == BORDER_BOTTOM);
    REQUIRE(path.segments[8].sides == BORDER_LEFT);
    REQUIRE(path.segments[1].end.x == Catch::Approx(106.49F).margin(0.01F));
    REQUIRE(path.segments[8].start.y == Catch::Approx(76.49F).margin(0.01F));

    const BorderPath clamped = rounded_rect_border_path({{0.0F, 0.0F}, {40.0F, 20.0F}}, 30.0F);
    REQUIRE(clamped.segments[0].length == Catch::Approx(20.0F));
    REQUIRE(clamped.segments[3].length == Catch::Approx(0.0F));
    REQUIRE(clamped.segments[1].length == Catch::Approx(std::numbers::pi_v<float> * 2.5F));
}

TEST_CASE("partial borders keep every draw style inside its selected side") {
    const BorderPath path = rounded_rect_border_path({{10.0F, 20.0F}, {110.0F, 80.0F}}, 12.0F);
    ui_test::ImGuiContext context({160.0F, 120.0F});
    ui_test::draw_window("border-path-test", [&] {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const int vertices_before = draw_list->VtxBuffer.Size;

        const auto require_left_bounds = [&](BorderStyle style) {
            const int first_vertex = draw_list->VtxBuffer.Size;
            draw_border_path(ui::draw_list(), path, BORDER_LEFT, ImColor{255, 255, 255, 255}, 2.0F, style);
            REQUIRE(draw_list->VtxBuffer.Size > first_vertex);

            float min_y = std::numeric_limits<float>::max();
            float max_y = std::numeric_limits<float>::lowest();
            for (int index = first_vertex; index < draw_list->VtxBuffer.Size; ++index) {
                const float y = draw_list->VtxBuffer[index].pos.y;
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }

            REQUIRE(min_y > 21.0F);
            REQUIRE(max_y < 79.0F);
        };

        draw_border_path(ui::draw_list(), path, BORDER_NONE, ImColor{255, 255, 255, 255}, 2.0F, BorderStyle::Solid);
        REQUIRE(draw_list->VtxBuffer.Size == vertices_before);

        require_left_bounds(BorderStyle::Solid);
        require_left_bounds(BorderStyle::Dashed);
        require_left_bounds(BorderStyle::Dotted);
    });
}

TEST_CASE("patterned borders keep every side visible") {
    const Rect rect = {{20.0F, 20.0F}, {180.0F, 100.0F}};
    const BorderPath path = rounded_rect_border_path(rect, 12.0F);
    ui_test::ImGuiContext context({220.0F, 140.0F});
    ui_test::draw_window("patterned-border-test", [&] {
        const auto require_sides = [&](BorderStyle style) {
            const int first_vertex = ImGui::GetWindowDrawList()->VtxBuffer.Size;
            draw_border_path(ui::draw_list(), path, BORDER_ALL, ImColor{255, 255, 255, 255}, 2.0F, style);
            const auto& vertices = ImGui::GetWindowDrawList()->VtxBuffer;
            const auto has_side = [&](auto&& predicate) {
                for (int index = first_vertex; index < vertices.Size; ++index) {
                    if (predicate(vertices[index].pos)) {
                        return true;
                    }
                }
                return false;
            };

            REQUIRE(has_side([&](ImVec2 point) {
                return point.x > 35.0F && point.x < 165.0F && std::abs(point.y - rect.min.y) < 3.0F;
            }));
            REQUIRE(has_side([&](ImVec2 point) {
                return point.y > 35.0F && point.y < 85.0F && std::abs(point.x - rect.max.x) < 3.0F;
            }));
            REQUIRE(has_side([&](ImVec2 point) {
                return point.x > 35.0F && point.x < 165.0F && std::abs(point.y - rect.max.y) < 3.0F;
            }));
            REQUIRE(has_side([&](ImVec2 point) {
                return point.y > 35.0F && point.y < 85.0F && std::abs(point.x - rect.min.x) < 3.0F;
            }));
        };

        require_sides(BorderStyle::Dashed);
        require_sides(BorderStyle::Dotted);
    });
}

TEST_CASE("style normalizes discrete fields and interpolates effect values") {
    Style style;
    style.border(BORDER_LEFT | BORDER_BOTTOM | 0x80).border_style(BorderStyle::Dashed);

    REQUIRE(style.border() == (BORDER_LEFT | BORDER_BOTTOM));
    REQUIRE(style.border_style() == BorderStyle::Dashed);

    Style target = style;
    target.border_style(BorderStyle::Dotted);
    REQUIRE(style.border_style() != target.border_style());

    Style::lerp(style, target, 0.0F);
    REQUIRE(style.border_style() == BorderStyle::Dotted);

    style.blur(8);
    REQUIRE(style.blur() == 8);

    Style blur_target;
    blur_target.blur(14);
    Style::lerp(style, blur_target, 0.0F);
    REQUIRE(style.blur() == 14);

    style.blur(-1);
    REQUIRE(style.blur() == 0);

    style.blur(MAX_BLUR_STRENGTH + 1);
    REQUIRE(style.blur() == MAX_BLUR_STRENGTH);

    style.border_thickness(0.25F);
    REQUIRE(style.border_thickness() == MIN_BORDER_THICKNESS);

    style.border_thickness(0.0F);
    REQUIRE(style.border_thickness() == 0.0F);

    Style shadow_target;
    shadow_target.box_shadow(
        {
            .offset = {8.0F, 4.0F},
            .blur = 12.0F,
            .spread = 2.0F,
            .color = ImColor{0.1F, 0.2F, 0.3F, 1.4F},
        },
        0.2F
    );
    Style shadow_current;
    Style::lerp(shadow_current, shadow_target, 0.1F);
    REQUIRE(shadow_current.box_shadow().offset.x == Catch::Approx(4.0F));
    REQUIRE(shadow_current.box_shadow().offset.y == Catch::Approx(2.0F));
    REQUIRE(shadow_current.box_shadow().blur == Catch::Approx(6.0F));
    REQUIRE(shadow_current.box_shadow().spread == Catch::Approx(1.0F));
    REQUIRE(shadow_current.box_shadow().color.Value.w == Catch::Approx(0.5F));

    Style normalized;
    normalized.box_shadow({.blur = -4.0F, .color = ImColor{0.0F, 0.0F, 0.0F, -1.0F}});
    REQUIRE(normalized.box_shadow().blur == 0.0F);
    REQUIRE(normalized.box_shadow().color.Value.w == 0.0F);
}

TEST_CASE("container shadows stay in their owner draw list") {
    ui_test::ImGuiContext context({320.0F, 240.0F});
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F});
    ImGui::SetNextWindowSize({320.0F, 240.0F});
    ImGui::Begin("container-shadow-test");

    EffectRegistry effects;
    initialize_effect<BoxShadowRegion>(effects, EffectSlot::BoxShadow, collect_shadow_callback);
    ImDrawList* foreground_draw_list = ImGui::GetForegroundDrawList();

    Container node("container");
    node.set_size({px(100.0F), px(60.0F)});
    node.configure_all_styles([](Style& style) {
        style.background_color(ImColor{0.2F, 0.2F, 0.2F, 1.0F})
            .box_shadow({
                .offset = {4.0F, 6.0F},
                .blur = 12.0F,
                .spread = 40.0F,
                .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F},
            });
    });
    node.update(1.0F);
    node.draw();
    draw_box_shadow(effects, *ImGui::GetWindowDrawList(), node.layout().visual_rect(), node.computed_style().box_shadow(), 0.0F);

    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();

    const ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    collected_shadow_region = nullptr;
    collected_effect_draw_list = nullptr;
    render_effect_commands(*draw_data);

    REQUIRE(collected_shadow_region != nullptr);
    REQUIRE(collected_shadow_region->shape.size().x == Catch::Approx(180.0F));
    REQUIRE(collected_shadow_region->shape.size().y == Catch::Approx(140.0F));
    REQUIRE(collected_shadow_region->cutout.size().x == Catch::Approx(100.0F));
    REQUIRE(collected_shadow_region->cutout.size().y == Catch::Approx(60.0F));
    REQUIRE(collected_effect_draw_list != foreground_draw_list);
}

TEST_CASE("tree shadows use the tree outer rect") {
    ui_test::ImGuiContext context({320.0F, 240.0F});
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F});
    ImGui::SetNextWindowSize({320.0F, 240.0F});
    ImGui::Begin("tree-shadow-test");

    EffectRegistry effects;
    initialize_effect<BoxShadowRegion>(effects, EffectSlot::BoxShadow, collect_shadow_callback);

    Container page("page");
    page.set_size({px(240.0F), px(160.0F)});
    auto& tree = page.add<TreeContainer>("tree");
    tree.set_size({px(160.0F), px(100.0F)});
    tree.configure_all_styles([](Style& style) {
        style.box_shadow({.spread = 40.0F, .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F}});
    });
    page.update(1.0F);

    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    page.draw();
    draw_box_shadow(effects, *ImGui::GetWindowDrawList(), tree.layout().visual_rect(), tree.computed_style().box_shadow(), 0.0F);
    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();

    const ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    collected_shadow_region = nullptr;
    render_effect_commands(*draw_data);

    REQUIRE(collected_shadow_region != nullptr);
    REQUIRE(collected_shadow_region->cutout.min.y == Catch::Approx(tree.layout().visual_rect().min.y));
}

TEST_CASE("dropdown trigger shadows use the trigger rect below its label") {
    Runtime runtime;
    UI surface = ui_test::make_surface(runtime);
    std::string value = "first";
    auto& dropdown = surface.root().add<DropdownWidget>(
        value, std::vector<DropdownOption>{{"first", "first"}, {"second", "second"}}, "dropdown"
    );
    dropdown.set_label("label").set_size({px(180.0F), fit()});
    dropdown.trigger().style().box_shadow({.spread = 8.0F, .color = ImColor{0.0F, 0.0F, 0.0F, 1.0F}});
    surface.effects().register_effect<BoxShadowRegion>(EffectSlot::BoxShadow, {collect_shadow_callback});

    const auto surface_context = ui_test::prepare_surface(surface, {320.0F, 180.0F});
    surface.begin_frame();
    surface.update(ImGui::GetIO().DeltaTime);
    surface.draw();
    surface.end_frame();

    const ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    collected_shadow_region = nullptr;
    render_effect_commands(*draw_data);

    REQUIRE(collected_shadow_region != nullptr);
    const Rect trigger_rect = dropdown.trigger().layout().visual_rect();
    REQUIRE(collected_shadow_region->cutout.min.x == Catch::Approx(trigger_rect.min.x));
    REQUIRE(collected_shadow_region->cutout.min.y == Catch::Approx(trigger_rect.min.y));
    REQUIRE(collected_shadow_region->cutout.max.x == Catch::Approx(trigger_rect.max.x));
    REQUIRE(collected_shadow_region->cutout.max.y == Catch::Approx(trigger_rect.max.y));
}

TEST_CASE("container blur excludes its scrollbar") {
    class TallNode final : public Node {
    public:
        TallNode() : Node("tall") {
            set_size({grow(), px(200.0F)});
        }

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({320.0F, 240.0F});
    Container node("container");
    node.set_size({px(100.0F), px(60.0F)});
    node.set_scrollable(true);
    node.configure_all_styles([](Style& style) { style.blur(8); });
    node.add<TallNode>();

    EffectRegistry effects;
    initialize_effect<BlurRegion>(effects, EffectSlot::Blur, collect_blur_callback);

    const auto draw_frame = [&] {
        effects.begin_frame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({320.0F, 240.0F});
        ImGui::Begin("container-blur-test");
        node.update(1.0F);
        node.draw();
        const Rect rect = node.layout().visual_rect();
        draw_blur(
            effects, *ImGui::GetWindowDrawList(), {rect.min, {rect.max.x - ImGui::GetStyle().ScrollbarSize, rect.max.y}}, 8, 0.0F
        );
        ImGui::End();
        ImGui::EndFrame();
        ImGui::Render();
    };

    draw_frame();
    draw_frame();

    const ImDrawData* draw_data = ImGui::GetDrawData();
    REQUIRE(draw_data != nullptr);
    collected_blur_region = nullptr;
    render_effect_commands(*draw_data);

    REQUIRE(collected_blur_region != nullptr);
    REQUIRE(
        node.layout().visual_rect().max.x - collected_blur_region->output.max.x == Catch::Approx(ImGui::GetStyle().ScrollbarSize)
    );
}

TEST_CASE("styled paint slots render before the node and above completed subtrees") {
    ui_test::ImGuiContext context({160.0F, 120.0F});
    ui_test::draw_window("decoration-test", [&] {
        StyledNode node("node");
        node.set_size({px(80.0F), px(40.0F)});
        node.before().style().background_color(ImColor{255, 0, 0, 255});
        node.after().style().border(BORDER_ALL).border_color(ImColor{255, 255, 255, 255});
        node.update(1.0F);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const int vertices_before = draw_list->VtxBuffer.Size;
        ImDrawList* foreground_draw_list = ImGui::GetForegroundDrawList();
        const int foreground_vertices_before = foreground_draw_list->VtxBuffer.Size;
        node.draw();
        REQUIRE(draw_list->VtxBuffer.Size > vertices_before);
        REQUIRE(foreground_draw_list->VtxBuffer.Size > foreground_vertices_before);

        const ImU32 before_color = ImGui::GetColorU32(ImVec4{1.0F, 0.0F, 0.0F, 1.0F});
        const ImU32 after_color = ImGui::GetColorU32(ImVec4{1.0F, 1.0F, 1.0F, 1.0F});
        int first_before = -1;
        int first_after = -1;
        for (int index = vertices_before; index < draw_list->VtxBuffer.Size; ++index) {
            if (draw_list->VtxBuffer[index].col == before_color && first_before < 0) {
                first_before = index;
            }
        }

        for (int index = foreground_vertices_before; index < foreground_draw_list->VtxBuffer.Size; ++index) {
            if (foreground_draw_list->VtxBuffer[index].col == after_color && first_after < 0) first_after = index;
        }

        REQUIRE(first_before >= 0);
        REQUIRE(first_after >= foreground_vertices_before);
    });
}

TEST_CASE("styled paint slots pass owner bounds and their target draw list to callbacks") {
    ui_test::ImGuiContext context({160.0F, 120.0F});
    ui_test::draw_window("decoration-callback-test", [&] {
        StyledNode node("node");
        node.set_size({px(80.0F), px(40.0F)});
        node.configure_all_styles([](Style& style) { style.padding({8.0F, 6.0F}); });
        Rect before_rect{};
        Rect after_rect{};
        Rect before_content_rect{};
        ImDrawList* before_draw_list = nullptr;
        ImDrawList* after_draw_list = nullptr;
        node.before().set_draw_callback([&](const PaintContext& context) {
            before_rect = context.rect;
            before_content_rect = context.content_rect;
            before_draw_list = &context.draw_list;
        });
        node.after().set_draw_callback([&](const PaintContext& context) {
            after_rect = context.rect;
            after_draw_list = &context.draw_list;
        });

        node.update(1.0F);
        node.draw();

        REQUIRE(before_rect.valid());
        REQUIRE(after_rect.valid());
        REQUIRE(before_rect.min.x == Catch::Approx(after_rect.min.x));
        REQUIRE(before_rect.min.y == Catch::Approx(after_rect.min.y));
        REQUIRE(before_rect.max.x == Catch::Approx(after_rect.max.x));
        REQUIRE(before_rect.max.y == Catch::Approx(after_rect.max.y));
        REQUIRE(before_content_rect.min.x == Catch::Approx(before_rect.min.x + 8.0F));
        REQUIRE(before_content_rect.min.y == Catch::Approx(before_rect.min.y + 6.0F));
        REQUIRE(before_content_rect.max.x == Catch::Approx(before_rect.max.x - 8.0F));
        REQUIRE(before_content_rect.max.y == Catch::Approx(before_rect.max.y - 6.0F));
        REQUIRE(before_draw_list == ImGui::GetWindowDrawList());
        REQUIRE(after_draw_list == ImGui::GetForegroundDrawList());
    });
}

TEST_CASE("ui nodes close child scopes before drawing after hooks") {
    ui_test::ImGuiContext context({100.0F, 100.0F});

    class DrawNode final : public Node {
    public:
        DrawNode(std::string id, std::vector<std::string>& events, bool skip = false)
            : Node(std::move(id)), m_events(events), m_skip(skip) {}

    private:
        void on_layout() override {
            m_events.push_back(id() + ":layout");
        }

        bool on_draw() override {
            m_events.push_back(id() + ":begin");
            return !m_skip;
        }

        void on_draw_end() override {
            m_events.push_back(id() + ":end");
        }

        void draw_after() override {
            m_events.push_back(id() + ":after");
        }

        std::vector<std::string>& m_events;
        bool m_skip = false;
    };

    std::vector<std::string> events;
    DrawNode root("root", events);
    root.add<DrawNode>("child", events);

    root.draw();
    REQUIRE(
        events ==
        std::vector<std::string>{
            "root:layout", "root:begin", "child:layout", "child:begin", "child:end", "child:after", "root:end", "root:after"
        }
    );

    events.clear();
    DrawNode hidden_root("hidden", events, true);
    hidden_root.add<DrawNode>("child", events);

    hidden_root.draw();
    REQUIRE(events == std::vector<std::string>{"hidden:layout", "hidden:begin"});
}

TEST_CASE("node measurement only reruns after invalidation") {
    ui_test::ImGuiContext context({100.0F, 100.0F});

    class MeasureNode final : public Node {
    public:
        explicit MeasureNode(int& count) : m_count(count) {}

    private:
        void on_measure() override {
            ++m_count;
        }

        int& m_count;
    };

    int root_measurements = 0;
    int child_measurements = 0;
    MeasureNode root(root_measurements);
    auto& child = root.add<MeasureNode>(child_measurements);

    root.draw();
    root.draw();
    REQUIRE(root_measurements == 1);
    REQUIRE(child_measurements == 1);

    child.invalidate_measure();
    root.draw();
    REQUIRE(root_measurements == 2);
    REQUIRE(child_measurements == 2);
}

TEST_CASE("clearing children destroys the subtree and clears input targets") {
    int destructions = 0;

    class LifetimeNode final : public Node {
    public:
        explicit LifetimeNode(int& destructions) : m_destructions(destructions) {}
        ~LifetimeNode() override {
            ++m_destructions;
        }

    private:
        int& m_destructions;
    };

    Node parent("parent");
    auto& child = parent.add<LifetimeNode>(destructions);
    child.add<LifetimeNode>(destructions);

    InputRouter router;
    parent.set_input_router(&router);
    REQUIRE(router.set_focus(child));
    REQUIRE(router.capture_pointer(child));

    parent.clear();

    REQUIRE(parent.children().empty());
    REQUIRE(destructions == 2);
    REQUIRE(router.focused_node() == nullptr);
    router.release_pointer();
}

TEST_CASE("visual bounds stay on layout unless paint overrides them") {
    class NoItemNode final : public Node {
    public:
        using Node::Node;
    };

    class ItemNode final : public Node {
    public:
        ItemNode(std::string id, bool input) : Node(std::move(id)) {
            set_size({px(10.0F), px(10.0F)});
            if (input) {
                set_input_mode(InputMode::Target);
            }
        }

    private:
        bool on_draw() override {
            ImGui::Dummy({40.0F, 30.0F});
            return true;
        }
    };

    class ManualRectNode final : public Node {
    public:
        ManualRectNode() : Node("manual") {}

    private:
        bool on_draw() override {
            set_visual_rect({{40.0F, 50.0F}, {70.0F, 80.0F}});
            return true;
        }
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});

    ImGui::NewFrame();
    ImGui::Begin("leaf-rect-test");
    ImGui::Dummy({30.0F, 20.0F});

    NoItemNode no_item("no-item");
    no_item.draw();

    ItemNode passive_item("passive-item", false);
    passive_item.draw();

    ItemNode input_item("input-item", true);
    input_item.draw();

    ManualRectNode manual;
    manual.draw();

    ImGui::End();
    ImGui::EndFrame();

    const Rect no_item_rect = no_item.layout().visual_rect();
    const Rect passive_item_rect = passive_item.layout().visual_rect();
    const Rect input_item_rect = input_item.layout().visual_rect();
    const Rect manual_rect = manual.layout().visual_rect();

    REQUIRE_FALSE(no_item_rect.valid());
    REQUIRE(passive_item_rect.size().x == 10.0F);
    REQUIRE(passive_item_rect.size().y == 10.0F);
    REQUIRE(input_item.layout().layout_rect().size().x == 10.0F);
    REQUIRE(input_item.layout().layout_rect().size().y == 10.0F);
    REQUIRE(input_item_rect.size().x == 10.0F);
    REQUIRE(input_item_rect.size().y == 10.0F);
    REQUIRE(manual_rect.min.x == 40.0F);
    REQUIRE(manual_rect.min.y == 50.0F);
    REQUIRE(manual_rect.max.x == 70.0F);
    REQUIRE(manual_rect.max.y == 80.0F);
}

TEST_CASE("nodes register only explicitly configured local input entries") {
    class RectNode final : public Node {
    public:
        RectNode(std::string id, Rect rect, std::function<void(UiEvent&)> callback = {})
            : Node(std::move(id)), m_rect(rect), m_callback(std::move(callback)) {}

    private:
        void on_event(UiEvent& event) override {
            if (m_callback) {
                m_callback(event);
            }
        }

        bool on_draw() override {
            set_visual_rect(m_rect);
            return true;
        }

        Rect m_rect;
        std::function<void(UiEvent&)> m_callback;
    };

    InputRouter router;
    Node root("root");
    int callbacks = 0;
    auto& passive = root.add<RectNode>("passive", Rect{{100.0F, 20.0F}, {200.0F, 120.0F}});
    auto& target = root.add<RectNode>("target", Rect{{100.0F, 20.0F}, {200.0F, 120.0F}}, [&callbacks](UiEvent&) { ++callbacks; });
    target.set_input_mode(InputMode::Target, {{10.0F, 20.0F}, {50.0F, 60.0F}});
    root.set_input_router(&router);

    root.draw();

    REQUIRE(router.node_at({120.0F, 50.0F}) == &target);
    REQUIRE(router.node_at({180.0F, 100.0F}) == nullptr);
    REQUIRE(router.node_at({120.0F, 50.0F}) != &passive);

    UiEvent event = UiEvent::make(EventType::PointerDown);
    event.position = {120.0F, 50.0F};
    event.button = PointerButton::Left;
    REQUIRE_FALSE(router.dispatch(event));
    REQUIRE(callbacks == 1);
}

TEST_CASE("positioned nodes preserve their cursor placement when drawing is skipped") {
    class SkippedNode final : public Node {
    private:
        bool on_draw() override {
            return false;
        }
    };

    class EmptyNode final : public Node {
    public:
        using Node::Node;
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});
    Container container("container");
    container.set_size({px(100.0F), px(80.0F)});
    auto& skipped = container.add<SkippedNode>();
    skipped.set_layout({.placement = {.offset = {120.0F, 0.0F}}, .in_flow = false});
    auto& empty = container.add<EmptyNode>();
    empty.set_layout({.placement = {.offset = {140.0F, 20.0F}}, .in_flow = false});

    ui_test::draw_node(container, "skipped-placement-test");

    REQUIRE(skipped.layout().local_rect().min.x == container.style().padding().x + 120.0F);
}

TEST_CASE("overlay children stay in the surface window") {
    class WindowNameNode final : public Node {
    public:
        std::string window_name;

    private:
        bool on_draw() override {
            window_name = ImGui::GetCurrentWindowRead()->Name;
            return true;
        }
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});
    LayerContainer overlay("overlay");
    auto& child = overlay.add<WindowNameNode>();

    REQUIRE_FALSE(overlay.layout().in_flow());

    ui_test::draw_node(overlay, "surface");

    REQUIRE(child.window_name == "surface");
    REQUIRE(overlay.layout().layout_rect().min.x == Catch::Approx(0.0F));
    REQUIRE(overlay.layout().layout_rect().min.y == Catch::Approx(0.0F));
    REQUIRE(overlay.layout().layout_rect().size().x == Catch::Approx(200.0F));
    REQUIRE(overlay.layout().layout_rect().size().y == Catch::Approx(120.0F));
}
