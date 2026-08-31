#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "imgui-context.hpp"
#include <ui/imgui/draw.hpp>
#include <ui/layout/child-container.hpp>
#include <ui/layout/geometry.hpp>
#include <ui/layout/overlay-container.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>

#include <imgui_internal.h>

#include <algorithm>
#include <memory>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

using namespace ui;

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
    ImGui::NewFrame();
    ImGui::Begin("border-path-test");
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const int vertices_before = draw_list->VtxBuffer.Size;

    const auto require_left_bounds = [&](BorderStyle style) {
        const int first_vertex = draw_list->VtxBuffer.Size;
        draw_border_path(path, BORDER_LEFT, ImColor{255, 255, 255, 255}, 2.0F, style);
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

    draw_border_path(path, BORDER_NONE, ImColor{255, 255, 255, 255}, 2.0F, BorderStyle::Solid);
    REQUIRE(draw_list->VtxBuffer.Size == vertices_before);

    require_left_bounds(BorderStyle::Solid);
    require_left_bounds(BorderStyle::Dashed);
    require_left_bounds(BorderStyle::Dotted);

    ImGui::End();
    ImGui::EndFrame();
}

TEST_CASE("border style is preserved by style updates") {
    Style style;
    style.border(BORDER_LEFT | BORDER_BOTTOM | 0x80).border_style(BorderStyle::Dashed);

    REQUIRE(style.border() == (BORDER_LEFT | BORDER_BOTTOM));
    REQUIRE(style.border_style() == BorderStyle::Dashed);

    Style target = style;
    target.border_style(BorderStyle::Dotted);
    REQUIRE_FALSE(style.is_close_to(target, 0.0F));

    Style::lerp(style, target, 0.0F);
    REQUIRE(style.border_style() == BorderStyle::Dotted);
}

TEST_CASE("blur style is preserved by style updates") {
    Style style;
    style.blur(8);
    REQUIRE(style.blur() == 8);

    Style target;
    target.blur(14);
    Style::lerp(style, target, 0.0F);
    REQUIRE(style.blur() == 14);

    style.blur(-1);
    REQUIRE(style.blur() == 0);
}

TEST_CASE("styled nodes create paint slots only when requested") {
    ChildContainer node("node");
    REQUIRE_FALSE(node.has_before());
    REQUIRE_FALSE(node.has_after());

    node.before().style().background_color(ImColor{255, 0, 0, 255});
    node.after().style().border(BORDER_ALL).border_color(ImColor{255, 255, 255, 255});

    REQUIRE(node.has_before());
    REQUIRE(node.has_after());

    node.remove_before();
    node.remove_after();
    REQUIRE_FALSE(node.has_before());
    REQUIRE_FALSE(node.has_after());
}

TEST_CASE("styled paint slots render in before and after order") {
    ui_test::ImGuiContext context({160.0F, 120.0F});
    ImGui::NewFrame();
    ImGui::Begin("decoration-test");

    StyledNode node("node");
    node.set_size({80.0F, 40.0F});
    node.before().style().background_color(ImColor{255, 0, 0, 255});
    node.after().style().border(BORDER_ALL).border_color(ImColor{255, 255, 255, 255});
    node.update(1.0F);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const int vertices_before = draw_list->VtxBuffer.Size;
    node.draw();
    REQUIRE(draw_list->VtxBuffer.Size > vertices_before);

    const ImU32 before_color = ImGui::GetColorU32(ImVec4{1.0F, 0.0F, 0.0F, 1.0F});
    const ImU32 after_color = ImGui::GetColorU32(ImVec4{1.0F, 1.0F, 1.0F, 1.0F});
    int first_before = -1;
    int first_after = -1;
    for (int index = vertices_before; index < draw_list->VtxBuffer.Size; ++index) {
        if (draw_list->VtxBuffer[index].col == before_color && first_before < 0) {
            first_before = index;
        }
        if (draw_list->VtxBuffer[index].col == after_color && first_after < 0) {
            first_after = index;
        }
    }

    REQUIRE(first_before >= 0);
    REQUIRE(first_after > first_before);

    ImGui::End();
    ImGui::EndFrame();
}

TEST_CASE("styled paint slots receive the owner rect and support custom drawing") {
    ui_test::ImGuiContext context({160.0F, 120.0F});
    ImGui::NewFrame();
    ImGui::Begin("decoration-callback-test");

    StyledNode node("node");
    node.set_size({80.0F, 40.0F});
    node.configure_all_styles([](Style& style) { style.padding({8.0F, 6.0F}); });
    Rect before_rect{};
    Rect after_rect{};
    Rect before_content_rect{};
    ImDrawList* before_draw_list = nullptr;
    node.before().set_draw_callback([&](const PaintContext& context) {
        before_rect = context.rect;
        before_content_rect = context.content_rect;
        before_draw_list = &context.draw_list;
    });
    node.after().set_draw_callback([&after_rect](const PaintContext& context) { after_rect = context.rect; });

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

    ImGui::End();
    ImGui::EndFrame();
}

TEST_CASE("ui nodes draw children and after hooks before end hooks") {
    class DrawNode final : public ui::Node {
    public:
        DrawNode(std::string id, std::vector<std::string>& events, bool skip = false)
            : ui::Node(std::move(id)), m_events(events), m_skip(skip) {}

    private:
        void on_layout() override {
            m_events.push_back(id() + ":layout");
        }

        bool on_draw() override {
            m_events.push_back(id() + ":begin");
            if (m_skip) {
                return false;
            }

            return true;
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
    root.add(std::make_unique<DrawNode>("child", events));

    root.draw();
    REQUIRE(
        events ==
        std::vector<std::string>{
            "root:layout", "root:begin", "child:layout", "child:begin", "child:after", "child:end", "root:after", "root:end"
        }
    );

    events.clear();
    DrawNode hidden_root("hidden", events, true);
    hidden_root.add(std::make_unique<DrawNode>("child", events));

    hidden_root.draw();
    REQUIRE(events == std::vector<std::string>{"hidden:layout", "hidden:begin"});
}

TEST_CASE("node measurement only reruns after invalidation") {
    class MeasureNode final : public ui::Node {
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
    auto& child = root.add_child<MeasureNode>(child_measurements);

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

    class LifetimeNode final : public ui::Node {
    public:
        explicit LifetimeNode(int& destructions) : m_destructions(destructions) {}
        ~LifetimeNode() override {
            ++m_destructions;
        }

    private:
        int& m_destructions;
    };

    ui::Node parent("parent");
    auto& child = parent.add_child<LifetimeNode>(destructions);
    child.add_child<LifetimeNode>(destructions);

    ui::InputRouter router;
    parent.set_input_router(&router);
    REQUIRE(router.set_focus(child));
    REQUIRE(router.capture_pointer(child));

    parent.clear();

    REQUIRE(parent.children().empty());
    REQUIRE(destructions == 2);
    REQUIRE(router.focused_node() == nullptr);
    router.release_pointer();
}

TEST_CASE("only input leaves capture their final imgui item rectangle") {
    class NoItemNode final : public ui::Node {
    public:
        using ui::Node::Node;
    };

    class ItemNode final : public ui::Node {
    public:
        ItemNode(std::string id, bool input) : Node(std::move(id)) {
            set_size({10.0F, 10.0F});
            if (input) {
                set_input_target();
            }
        }

    private:
        bool on_draw() override {
            ImGui::Dummy({40.0F, 30.0F});
            return true;
        }
    };

    class ManualRectNode final : public ui::Node {
    public:
        ManualRectNode() : ui::Node("manual") {}

    private:
        bool on_draw() override {
            set_screen_rect({{40.0F, 50.0F}, {70.0F, 80.0F}});
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

    const ui::Rect no_item_rect = no_item.layout().screen_rect();
    const ui::Rect passive_item_rect = passive_item.layout().screen_rect();
    const ui::Rect input_item_rect = input_item.layout().screen_rect();
    const ui::Rect manual_rect = manual.layout().screen_rect();

    REQUIRE_FALSE(no_item_rect.valid());
    REQUIRE(passive_item_rect.size().x == 10.0F);
    REQUIRE(passive_item_rect.size().y == 10.0F);
    REQUIRE(input_item_rect.size().x == 40.0F);
    REQUIRE(input_item_rect.size().y == 30.0F);
    REQUIRE(manual_rect.min.x == 40.0F);
    REQUIRE(manual_rect.min.y == 50.0F);
    REQUIRE(manual_rect.max.x == 70.0F);
    REQUIRE(manual_rect.max.y == 80.0F);
}

TEST_CASE("nodes register only explicitly configured local input regions") {
    class RectNode final : public ui::Node {
    public:
        RectNode(std::string id, ui::Rect rect) : Node(std::move(id)), m_rect(rect) {}

    private:
        bool on_draw() override {
            set_screen_rect(m_rect);
            return true;
        }

        ui::Rect m_rect;
    };

    ui::InputRouter router;
    ui::Node root("root");
    auto& passive = root.add_child<RectNode>("passive", ui::Rect{{100.0F, 20.0F}, {200.0F, 120.0F}});
    auto& target = root.add_child<RectNode>("target", ui::Rect{{100.0F, 20.0F}, {200.0F, 120.0F}});
    int callbacks = 0;
    target.set_input_target({
        .rect = {{10.0F, 20.0F}, {50.0F, 60.0F}},
        .on_event = [&callbacks](ui::UiEvent&) { ++callbacks; },
    });
    root.set_input_router(&router);

    root.draw();

    REQUIRE(router.node_at({120.0F, 50.0F}) == &target);
    REQUIRE(router.node_at({180.0F, 100.0F}) == nullptr);
    REQUIRE(router.node_at({120.0F, 50.0F}) != &passive);

    ui::UiEvent event = ui::UiEvent::make(ui::EventType::PointerDown);
    event.position = {120.0F, 50.0F};
    event.button = ui::PointerButton::Left;
    REQUIRE_FALSE(router.dispatch(event));
    REQUIRE(callbacks == 1);
}

TEST_CASE("skipped explicitly placed nodes keep imgui child boundaries valid") {
    class SkippedNode final : public ui::Node {
    private:
        bool on_draw() override {
            return false;
        }
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});
    ui::ChildContainer container("container");
    container.set_size({100.0F, 80.0F});
    auto& skipped = container.add_child<SkippedNode>();
    skipped.set_placement({.anchor = ui::Anchor::TopLeft, .origin = ui::Origin::TopLeft, .offset = {120.0F, 0.0F}});

    ImGui::NewFrame();
    ImGui::Begin("skipped-placement-test");
    container.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(skipped.layout().arranged_position().x == container.style().padding().x + 120.0F);
}

TEST_CASE("overlay children stay in the surface window") {
    class WindowNameNode final : public ui::Node {
    public:
        std::string window_name;

    private:
        bool on_draw() override {
            window_name = ImGui::GetCurrentWindowRead()->Name;
            return true;
        }
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});
    ui::OverlayNode overlay("overlay");
    auto& child = overlay.add_child<WindowNameNode>();

    ImGui::NewFrame();
    ImGui::Begin("surface");
    overlay.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(child.window_name == "surface");
}
