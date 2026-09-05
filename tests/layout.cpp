#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <ui/layout/container.hpp>
#include <ui/layout/geometry.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/layout/stack-container.hpp>
#include <ui/layout/virtual-layout.hpp>
#include <ui/input/router.hpp>
#include <ui/widgets/text.hpp>
#include "imgui-context.hpp"

#include <cfloat>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ui;

TEST_CASE("layout containers resolve themselves before arranging children", "[layout]") {
    class TestContainer final : public Container {
    public:
        TestContainer() : Container("test-container") {}

        int arrange_count = 0;

    protected:
        void arrange_children() override {
            ++arrange_count;
        }
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});
    TestContainer container;

    ImGui::NewFrame();
    ImGui::Begin("container-layout-test");
    const ImVec2 available = ImGui::GetContentRegionAvail();
    container.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(container.arrange_count == 1);
    REQUIRE(container.layout().size().x == Catch::Approx(available.x));
    REQUIRE(container.layout().size().y == Catch::Approx(available.y));
}

TEST_CASE("input entries exclude clipped widget bounds", "[Widget][input][regression]") {
    class InputNode final : public Node {
    public:
        InputNode() {
            set_size({px(24.0F), px(20.0F)});
            set_input_target();
        }

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});
    InputRouter router;
    InputNode node;
    node.set_input_router(&router);

    router.begin_frame();
    ImGui::NewFrame();
    ImGui::SetNextWindowSize({200.0F, 120.0F});
    ImGui::Begin("clipped-image-input-test");
    const ImVec2 position = ImGui::GetCursorScreenPos();
    ImGui::PushClipRect(position, {position.x + 12.0F, position.y + 100.0F}, true);
    node.draw();
    ImGui::PopClipRect();
    ImGui::End();
    ImGui::EndFrame();

    const Rect bounds = node.layout().visual_rect();
    REQUIRE(router.stats().entry_count == 1);
    REQUIRE(router.node_at({bounds.min.x + 6.0F, bounds.min.y + 10.0F}) == &node);
    REQUIRE(router.node_at({bounds.min.x + 18.0F, bounds.min.y + 10.0F}) == nullptr);
}

TEST_CASE("layout anchors resolve the child origin against the parent") {
    const ImVec2 centered = resolve_layout_position({100.0F, 80.0F}, {20.0F, 10.0F}, Anchor::Center, Anchor::Center);
    REQUIRE(centered.x == 40.0F);
    REQUIRE(centered.y == 35.0F);

    const ImVec2 bottom_right =
        resolve_layout_position({100.0F, 80.0F}, {20.0F, 10.0F}, Anchor::BottomRight, Anchor::TopLeft, {2.0F, -3.0F});
    REQUIRE(bottom_right.x == 102.0F);
    REQUIRE(bottom_right.y == 77.0F);

    const ImVec2 custom = resolve_layout_position({100.0F, 80.0F}, {20.0F, 10.0F}, {0.25F, 0.75F}, {0.5F, 1.0F});
    REQUIRE(custom.x == 15.0F);
    REQUIRE(custom.y == 50.0F);
}

TEST_CASE("placement changes preserve implicit measured sizing") {
    class MeasuredNode final : public Node {
    public:
        MeasuredNode() : Node("measured") {}

    protected:
        void on_measure() override {
            set_measured_size({32.0F, 18.0F}, true, true);
        }

        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    } node;

    LayoutConfig config = node.layout().config();
    config.placement.anchor = Anchor::Center;
    config.placement.origin = Anchor::Center;
    config.in_flow = false;
    node.set_layout(config);

    ui_test::ImGuiContext context({120.0F, 80.0F});
    ImGui::NewFrame();
    ImGui::Begin("implicit-sizing-test");
    node.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(node.layout().size().x == Catch::Approx(32.0F));
    REQUIRE(node.layout().size().y == Catch::Approx(18.0F));
}

TEST_CASE("layout geometry exposes resolved rectangles") {
    const Rect parent{{10.0F, 20.0F}, {110.0F, 100.0F}};
    const Rect child = resolve_layout_rect(
        parent, {20.0F, 10.0F}, {.anchor = Anchor::BottomRight, .origin = Anchor::TopLeft, .offset = {2.0F, -3.0F}}
    );

    REQUIRE(child.min.x == 112.0F);
    REQUIRE(child.min.y == 97.0F);
    REQUIRE(child.size().x == 20.0F);
    REQUIRE(child.size().y == 10.0F);
    REQUIRE(child.contains({120.0F, 100.0F}));
    REQUIRE_FALSE(child.contains({50.0F, 50.0F}));
}

TEST_CASE("layout size resolves each axis from its sizing rule") {
    const ImVec2 resolved = LayoutSize{grow(), px(40.0F)}.resolve({}, {120.0F, 80.0F});
    REQUIRE(resolved.x == 120.0F);
    REQUIRE(resolved.y == 40.0F);

    const ImVec2 clamped = LayoutSize{grow(), grow()}.resolve({}, {-20.0F, 60.0F});
    REQUIRE(clamped.x == 0.0F);
    REQUIRE(clamped.y == 60.0F);

    const ImVec2 fixed_zero = LayoutSize{px(0.0F), px(0.0F)}.resolve({40.0F, 30.0F}, {120.0F, 80.0F});
    REQUIRE(fixed_zero.x == 0.0F);
    REQUIRE(fixed_zero.y == 0.0F);
}

TEST_CASE("stack layout places auto-sized children after their measured height") {
    ui_test::ImGuiContext context({240.0F, 160.0F});

    StackContainer stack("auto-size-stack");
    stack.set_size({px(200.0F), px(100.0F)});
    stack.set_spacing(4.0F);
    stack.add<TextWidget>("first");
    stack.add<TextWidget>("second");

    const auto draw_frame = [&stack] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({240.0F, 160.0F});
        ImGui::Begin("stack-auto-size-test");
        stack.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();

    const Rect first = stack.children()[0]->layout().visual_rect();
    const Rect second = stack.children()[1]->layout().visual_rect();
    REQUIRE(first.size().y > 0.0F);
    REQUIRE(second.min.y >= first.max.y + 4.0F);
    REQUIRE(stack.children()[1]->layout().config().placement.offset.y == Catch::Approx(0.0F));
}

TEST_CASE("stack layout centers flow content on requested axes") {
    ui_test::ImGuiContext context({240.0F, 160.0F});
    StackContainer stack("centered-stack", StackDirection::Horizontal);
    stack.set_size({px(200.0F), px(100.0F)});
    stack.style().padding({});
    stack.set_content_alignment(Anchor::Center);
    auto& field = stack.add<TextWidget>("field");
    field.set_size({px(40.0F), px(20.0F)});

    ImGui::NewFrame();
    ImGui::Begin("centered-stack-test");
    stack.draw();
    ImGui::End();
    ImGui::EndFrame();

    const Rect stack_rect = stack.layout().visual_rect();
    const Rect field_rect = field.layout().visual_rect();
    REQUIRE(field_rect.min.x - stack_rect.min.x == Catch::Approx(80.0F));
    REQUIRE(field_rect.min.y - stack_rect.min.y == Catch::Approx(40.0F));
    REQUIRE(field_rect.size().x == Catch::Approx(40.0F));
}

TEST_CASE("stack layout excludes explicitly positioned children from its flow") {
    class FixedNode final : public Node {
    public:
        explicit FixedNode(ImVec2 size) {
            set_size({px(size.x), px(size.y)});
        }

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({240.0F, 160.0F});

    StackContainer stack("positioned-child-stack");
    stack.set_size({px(200.0F), px(100.0F)});
    stack.set_spacing(4.0F);
    stack.style().padding({});
    auto& first = stack.add<FixedNode>(ImVec2{30.0F, 10.0F});
    auto& positioned = stack.add<FixedNode>(ImVec2{80.0F, 40.0F});
    positioned.set_layout({
        .size = {px(80.0F), px(40.0F)},
        .placement = {.offset = {100.0F, 20.0F}},
        .in_flow = false,
    });
    auto& second = stack.add<FixedNode>(ImVec2{30.0F, 10.0F});

    ImGui::NewFrame();
    ImGui::Begin("positioned-child-stack-test");
    stack.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(second.layout().local_rect().min.y == Catch::Approx(first.layout().local_rect().min.y + 14.0F));
    REQUIRE(positioned.layout().local_rect().min.x == Catch::Approx(100.0F));
    REQUIRE(positioned.layout().local_rect().min.y == Catch::Approx(20.0F));
}

TEST_CASE("fit content stack includes children spacing and padding") {
    class FixedNode final : public Node {
    public:
        FixedNode(std::string id, ImVec2 size) : Node(std::move(id)) {
            set_size({px(size.x), px(size.y)});
        }

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({240.0F, 160.0F});

    StackContainer stack("fit-content-stack");
    stack.set_size({fit(), fit()});
    stack.set_spacing(4.0F);
    stack.configure_all_styles([](Style& style) { style.padding({7.0F, 5.0F}); });
    stack.add<FixedNode>("first", ImVec2{30.0F, 10.0F});
    stack.add<FixedNode>("second", ImVec2{50.0F, 20.0F});

    ImGui::NewFrame();
    ImGui::Begin("fit-content-stack-test");
    stack.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(stack.layout().size().x == Catch::Approx(64.0F));
    REQUIRE(stack.layout().size().y == Catch::Approx(44.0F));
}

TEST_CASE("fit-height stack fills its available width without stretching children", "[StackContainer][layout]") {
    class FixedNode final : public Node {
    public:
        FixedNode() {
            set_size({grow(), px(20.0F)});
        }

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({240.0F, 160.0F});

    StackContainer root("fit-height-root");
    root.set_size({px(200.0F), px(100.0F)});
    root.style().padding({});

    auto& field = root.add<StackContainer>("fit-height-field");
    field.set_size({grow(), fit()});
    field.style().padding({});
    field.add<FixedNode>();

    ImGui::NewFrame();
    ImGui::Begin("fit-height-stack-test");
    root.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(field.layout().size().x == Catch::Approx(200.0F));
    REQUIRE(field.layout().size().y == Catch::Approx(20.0F));
    REQUIRE(field.children()[0]->layout().size().x == Catch::Approx(200.0F));
}

TEST_CASE("fit content stack remeasures after direction and spacing changes") {
    class FixedNode final : public Node {
    public:
        explicit FixedNode(ImVec2 size) {
            set_size({px(size.x), px(size.y)});
        }

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({240.0F, 160.0F});

    StackContainer stack("fit-content-remeasure");
    stack.set_size({fit(), fit()});
    stack.set_spacing(4.0F);
    stack.style().padding({0.0F, 0.0F});
    stack.add<FixedNode>(ImVec2{30.0F, 10.0F});
    stack.add<FixedNode>(ImVec2{50.0F, 20.0F});

    const auto draw_frame = [&stack] {
        ImGui::NewFrame();
        ImGui::Begin("fit-content-remeasure-test");
        stack.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();
    REQUIRE(stack.layout().size().x == Catch::Approx(50.0F));
    REQUIRE(stack.layout().size().y == Catch::Approx(34.0F));

    stack.set_spacing(10.0F);
    draw_frame();
    REQUIRE(stack.layout().size().x == Catch::Approx(50.0F));
    REQUIRE(stack.layout().size().y == Catch::Approx(40.0F));

    stack.set_direction(StackDirection::Horizontal);
    draw_frame();
    REQUIRE(stack.layout().size().x == Catch::Approx(90.0F));
    REQUIRE(stack.layout().size().y == Catch::Approx(20.0F));
}

TEST_CASE("visibility changes in an anchored overlay do not move its fixed sibling") {
    class FixedNode final : public Node {
    public:
        explicit FixedNode(ImVec2 size) {
            set_size({px(size.x), px(size.y)});
        }

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({640.0F, 360.0F});

    StackContainer overlay("overlay", StackDirection::Horizontal);
    overlay.set_layout({
        .size = {fit(), fit()},
        .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight, .offset = {-20.0F, 20.0F}},
        .in_flow = false,
    });
    overlay.style().padding({});

    auto& optional_panel = overlay.add<FixedNode>(ImVec2{120.0F, 80.0F});
    optional_panel.set_visible(false);
    auto& dynamic_panel = overlay.add<FixedNode>(ImVec2{240.0F, 160.0F});

    const auto draw_frame = [&overlay] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({640.0F, 360.0F});
        ImGui::Begin("overlay-visibility-test");
        overlay.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();
    const float initial_x = dynamic_panel.layout().visual_rect().min.x;

    optional_panel.set_visible(true);
    draw_frame();
    const float shown_x = dynamic_panel.layout().visual_rect().min.x;
    const float shown_panel_right = optional_panel.layout().visual_rect().max.x;

    optional_panel.set_visible(false);
    draw_frame();
    const float hidden_x = dynamic_panel.layout().visual_rect().min.x;

    REQUIRE(shown_x == Catch::Approx(initial_x));
    REQUIRE(shown_panel_right <= shown_x);
    REQUIRE(hidden_x == Catch::Approx(initial_x));
}

TEST_CASE("horizontal stack places a fixed item after auto-sized text") {
    class FixedItemNode final : public Node {
    public:
        explicit FixedItemNode(ImVec2 size) {
            set_size({px(size.x), px(size.y)});
        }

    private:
        bool on_draw() override {
            ImGui::Button("add notification", layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({640.0F, 180.0F});

    Node root("root");
    auto& stack = root.add<StackContainer>("notification-test", StackDirection::Horizontal);
    stack.set_size({px(620.0F), px(120.0F)});
    stack.set_spacing(8.0F);
    stack.configure_all_styles([](Style& style) { style.padding({8.0F, 8.0F}); });
    auto& text_node = stack.add<TextWidget>("notifications: 0");
    stack.add<FixedItemNode>(ImVec2{180.0F, 30.0F});

    const auto draw_frame = [&root] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({640.0F, 180.0F});
        ImGui::Begin("horizontal-stack-test");
        root.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();

    const Rect text = stack.children()[0]->layout().visual_rect();
    const Rect item = stack.children()[1]->layout().visual_rect();
    REQUIRE(text.valid());
    REQUIRE(item.valid());
    REQUIRE(item.min.x >= text.max.x + 8.0F);

    text_node.set_text("notifications: 10000");
    draw_frame();

    const Rect resized_text = stack.children()[0]->layout().visual_rect();
    const Rect repositioned_item = stack.children()[1]->layout().visual_rect();
    REQUIRE(resized_text.size().x > text.size().x);
    REQUIRE(repositioned_item.min.x >= resized_text.max.x + 8.0F);
}

TEST_CASE("stack divides remaining main-axis space between flexible children", "[layout][regression]") {
    class LayoutItemNode final : public Node {
    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({360.0F, 140.0F});

    StackContainer stack("flexible-stack", StackDirection::Horizontal);
    stack.set_size({px(300.0F), px(80.0F)});
    stack.set_spacing(5.0F);
    stack.style().padding({10.0F, 10.0F});

    auto& fixed = stack.add<LayoutItemNode>();
    fixed.set_size({px(60.0F), px(20.0F)});
    auto& first_flexible = stack.add<LayoutItemNode>();
    first_flexible.set_size({grow(), px(20.0F)});
    auto& hidden = stack.add<LayoutItemNode>();
    hidden.set_size({px(200.0F), px(20.0F)});
    hidden.set_visible(false);
    auto& second_flexible = stack.add<LayoutItemNode>();
    second_flexible.set_size({grow(), px(20.0F)});

    ImGui::NewFrame();
    ImGui::Begin("flexible-stack-test");
    stack.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(fixed.layout().size().x == Catch::Approx(60.0F));
    REQUIRE(first_flexible.layout().size().x == Catch::Approx(105.0F));
    REQUIRE(second_flexible.layout().size().x == Catch::Approx(105.0F));
    REQUIRE(second_flexible.layout().local_rect().min.x == Catch::Approx(first_flexible.layout().local_rect().min.x + 110.0F));
}

TEST_CASE("stack distributes grow space by axis weight", "[layout]") {
    class LayoutItemNode final : public Node {
    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({360.0F, 140.0F});

    StackContainer stack("weighted-stack", StackDirection::Horizontal);
    stack.set_size({px(300.0F), px(80.0F)});
    stack.set_spacing(5.0F);
    stack.style().padding({10.0F, 10.0F});

    auto& fixed = stack.add<LayoutItemNode>();
    fixed.set_size({px(60.0F), px(20.0F)});
    auto& narrow = stack.add<LayoutItemNode>();
    narrow.set_size({grow(), px(20.0F)});
    auto& wide = stack.add<TextWidget>("wide");
    wide.set_size({grow(2.0F), px(20.0F)});

    ImGui::NewFrame();
    ImGui::Begin("weighted-stack-test");
    stack.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(fixed.layout().size().x == Catch::Approx(60.0F));
    REQUIRE(narrow.layout().size().x == Catch::Approx(70.0F));
    REQUIRE(wide.layout().size().x == Catch::Approx(140.0F));
}

TEST_CASE("explicit fit keeps a text widget intrinsic size", "[layout]") {
    ui_test::ImGuiContext context({240.0F, 140.0F});

    StackContainer stack("fit-text-stack", StackDirection::Horizontal);
    stack.set_size({px(200.0F), px(80.0F)});
    stack.style().padding({});

    auto& text = stack.add<TextWidget>("fit");
    text.set_size({fit(), fit()});
    auto& fill = stack.add<Node>("fill");
    fill.set_size({grow(), px(20.0F)});

    ImGui::NewFrame();
    ImGui::Begin("fit-text-stack-test");
    stack.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(text.layout().config().size.width.mode == LayoutSizeMode::Fit);
    REQUIRE(text.layout().size().x > 0.0F);
    REQUIRE(text.layout().size().x < stack.layout().size().x);
    REQUIRE(fill.layout().size().x > 0.0F);
}

TEST_CASE("vertical stack flexible child reflows with available height", "[layout][regression]") {
    class LayoutItemNode final : public Node {
    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({220.0F, 240.0F});

    StackContainer stack("vertical-flexible-stack");
    stack.set_size({px(120.0F), grow()});
    stack.set_spacing(8.0F);
    stack.style().padding({6.0F, 6.0F});
    auto& fixed = stack.add<LayoutItemNode>();
    fixed.set_size({grow(), px(30.0F)});
    auto& flexible = stack.add<LayoutItemNode>();
    flexible.set_size({grow(), grow()});

    const auto draw_frame = [&stack](float height) {
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({220.0F, height});
        ImGui::Begin("vertical-flexible-stack-test");
        stack.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame(140.0F);
    const float initial_height = flexible.layout().size().y;
    draw_frame(220.0F);

    REQUIRE(fixed.layout().size().x == Catch::Approx(108.0F));
    REQUIRE(flexible.layout().size().x == Catch::Approx(108.0F));
    REQUIRE(flexible.layout().size().y > initial_height);
    REQUIRE(fixed.layout().size().y + flexible.layout().size().y + 8.0F == Catch::Approx(stack.layout().size().y - 12.0F));
}

TEST_CASE("changing stack direction rearranges existing children", "[layout][regression]") {
    class LayoutItemNode final : public Node {
    public:
        using Node::Node;

    private:
        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({260.0F, 160.0F});

    StackContainer stack("direction-stack");
    stack.set_size({px(200.0F), px(100.0F)});
    stack.set_spacing(5.0F);
    Node& first = stack.add<LayoutItemNode>("first");
    first.set_size({px(30.0F), px(20.0F)});
    Node& second = stack.add<LayoutItemNode>("second");
    second.set_size({px(30.0F), px(20.0F)});

    const auto draw_frame = [&stack] {
        ImGui::NewFrame();
        ImGui::Begin("stack-direction-test");
        stack.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();
    REQUIRE(second.layout().local_rect().min.x == Catch::Approx(first.layout().local_rect().min.x));
    REQUIRE(second.layout().local_rect().min.y == Catch::Approx(first.layout().local_rect().min.y + 25.0F));

    stack.set_direction(StackDirection::Horizontal);
    draw_frame();
    REQUIRE(second.layout().local_rect().min.x == Catch::Approx(first.layout().local_rect().min.x + 35.0F));
    REQUIRE(second.layout().local_rect().min.y == Catch::Approx(first.layout().local_rect().min.y));
}

TEST_CASE("text measurement uses the font inherited from its parent", "[layout][regression]") {
    class FixedItemNode final : public Node {
    public:
        FixedItemNode() {
            set_size({px(100.0F), px(30.0F)});
        }

    private:
        bool on_draw() override {
            ImGui::Button("sibling", layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({480.0F, 140.0F});

    ImFontConfig default_font_config;
    default_font_config.SizePixels = 13.0F;
    ImGui::GetIO().Fonts->AddFontDefault(&default_font_config);
    ImFontConfig large_font_config;
    large_font_config.SizePixels = 28.0F;
    ImFont* large_font = ImGui::GetIO().Fonts->AddFontDefault(&large_font_config);
    ui_test::ImGuiContext::build_fonts();

    Container parent("font-parent");
    parent.set_size({px(460.0F), px(100.0F)});
    auto& stack = parent.add<StackContainer>("font-stack", StackDirection::Horizontal);
    stack.set_size({px(440.0F), px(60.0F)});
    stack.set_spacing(8.0F);
    stack.add<TextWidget>("notifications: 0");
    stack.add<FixedItemNode>();

    const auto draw_frame = [&parent] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({480.0F, 140.0F});
        ImGui::Begin("inherited-font-layout-test");
        parent.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();
    parent.set_font(large_font);
    draw_frame();

    const Rect text = stack.children()[0]->layout().visual_rect();
    const Rect sibling = stack.children()[1]->layout().visual_rect();
    const float expected_text_width = large_font->CalcTextSizeA(large_font->LegacySize, FLT_MAX, 0.0F, "notifications: 0").x;

    REQUIRE(text.valid());
    REQUIRE(text.size().x == Catch::Approx(expected_text_width).margin(1.0F));
    REQUIRE(sibling.min.x >= text.max.x + 8.0F);
}

TEST_CASE("resizable container stays within its parent bounds") {
    ui_test::ImGuiContext context({320.0F, 220.0F});

    ResizableContainer resizable("resizable");
    InputRouter router;
    resizable.set_input_router(&router);
    resizable.set_size({px(80.0F), px(60.0F)});
    resizable.set_resize(ResizeAxes::Both);

    const auto draw_frame = [&resizable] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({320.0F, 220.0F});
        ImGui::Begin("resize-root");
        ImGui::BeginChild("resize-parent", {120.0F, 90.0F});
        const int foreground_vertices = ImGui::GetForegroundDrawList()->VtxBuffer.Size;
        resizable.draw();
        REQUIRE(ImGui::GetForegroundDrawList()->VtxBuffer.Size == foreground_vertices);
        ImGui::EndChild();
        ImGui::End();
        ImGui::EndFrame();
    };

    router.begin_frame();
    draw_frame();
    const Rect initial_rect = resizable.layout().visual_rect();
    const ImVec2 handle_position = {initial_rect.max.x - 5.0F, initial_rect.max.y - 5.0F};

    REQUIRE(router.node_at({initial_rect.min.x + 5.0F, initial_rect.min.y + 5.0F}) == nullptr);
    REQUIRE(router.node_at(handle_position) == &resizable);

    UiEvent hover = UiEvent::make(EventType::PointerMove);
    hover.position = handle_position;
    router.dispatch(hover);
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow);

    UiEvent down = UiEvent::make(EventType::PointerDown);
    down.position = handle_position;
    down.button = PointerButton::Left;
    REQUIRE(router.dispatch(down));

    UiEvent move = UiEvent::make(EventType::PointerMove);
    move.position = {300.0F, 200.0F};
    REQUIRE(router.dispatch(move));

    router.begin_frame();
    draw_frame();

    UiEvent up = UiEvent::make(EventType::PointerUp);
    up.position = move.position;
    up.button = PointerButton::Left;
    REQUIRE(router.dispatch(up));

    REQUIRE(resizable.layout().size().x > 80.0F);
    REQUIRE(resizable.layout().size().x <= 120.0F);
    REQUIRE(resizable.layout().size().y <= 90.0F);
}

TEST_CASE("nodes without explicit positions follow the ImGui cursor") {
    class FlowNode final : public Node {
    public:
        explicit FlowNode(std::string id) : Node(std::move(id)) {
            set_input_target();
        }

        bool on_draw() override {
            ImGui::Dummy({20.0F, 10.0F});
            return true;
        }
    };

    ui_test::ImGuiContext context({200.0F, 120.0F});
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F});
    ImGui::SetNextWindowSize({200.0F, 120.0F});
    ImGui::Begin("flow-test");

    FlowNode first("first");
    FlowNode second("second");
    FlowNode same_line("same-line");

    first.draw();
    second.draw();
    const Rect first_rect = first.layout().visual_rect();
    const Rect second_rect = second.layout().visual_rect();

    ImGui::SameLine();
    same_line.draw();
    const Rect same_line_rect = same_line.layout().visual_rect();

    REQUIRE(second_rect.min.y > first_rect.min.y);
    REQUIRE(same_line_rect.min.x > second_rect.min.x);

    Node logical_root("logical-root");
    auto routed_child = std::make_unique<FlowNode>("routed-child");
    logical_root.attach(std::move(routed_child));

    ImGui::SetCursorPos({0.0F, 60.0F});
    logical_root.draw();

    const ImVec2 routed_position = logical_root.children().front()->layout().visual_rect().min;
    REQUIRE(routed_position.y == Catch::Approx(60.0F));

    ImGui::End();
    ImGui::Render();
}

TEST_CASE("changing an anchor restores a node's natural top-left flow position") {
    class FlowNode final : public Node {
    public:
        explicit FlowNode(std::string node_id) : Node(std::move(node_id)) {
            set_size({px(40.0F), px(20.0F)});
        }

        bool on_draw() override {
            ImGui::Dummy(layout().size());
            return true;
        }
    };

    ui_test::ImGuiContext context({240.0F, 160.0F});

    FlowNode title("title");
    FlowNode tab("tab");

    const auto draw_frame = [&title, &tab] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({240.0F, 160.0F});
        ImGui::Begin("anchor-flow-test");
        title.draw();
        ImGui::SameLine();
        tab.draw();
        const ImVec2 position = tab.layout().visual_rect().min;
        ImGui::End();
        ImGui::EndFrame();
        return position;
    };

    const ImVec2 initial_position = draw_frame();
    LayoutConfig tab_layout = tab.layout().config();
    tab_layout.placement.anchor = Anchor::Center;
    tab_layout.in_flow = false;
    tab.set_layout(tab_layout);
    draw_frame();
    tab_layout.in_flow = true;
    tab.set_layout(tab_layout);
    const ImVec2 restored_position = draw_frame();

    REQUIRE(restored_position.x == Catch::Approx(initial_position.x));
    REQUIRE(restored_position.y == Catch::Approx(initial_position.y));
}

TEST_CASE("node screen rectangles follow scrollable child windows") {
    class ScrollProbeNode final : public Node {
    public:
        explicit ScrollProbeNode(std::string node_id) : Node(std::move(node_id)) {
            set_size({px(40.0F), px(20.0F)});
        }

        bool on_draw() override {
            actual_position = ImGui::GetCursorScreenPos();
            ImGui::Dummy(layout().size());
            return true;
        }

        ImVec2 actual_position{};
    };

    class ScrollProbeContainer final : public Container {
    public:
        ScrollProbeContainer() : Container("scroll-probe") {
            set_size({px(100.0F), px(50.0F)});
            set_scrollable(true);
        }

        bool scroll_to_end = false;
        float current_scroll_y = 0.0F;

    protected:
        bool paint() override {
            ImGui::SetNextWindowContentSize({100.0F, 400.0F});
            return Container::paint();
        }

        void on_draw_end() override {
            if (scroll_to_end) {
                ImGui::SetScrollY(100.0F);
            }

            Container::on_draw_end();
        }

        void draw_children() override {
            current_scroll_y = ImGui::GetScrollY();
            Node::draw_children();
        }
    };

    ui_test::ImGuiContext context({240.0F, 160.0F});

    ScrollProbeContainer container;
    ScrollProbeNode* target = nullptr;
    for (int index = 0; index < 8; ++index) {
        target = &container.add<ScrollProbeNode>(std::format("item-{}", index));
    }

    const auto draw_frame = [&container] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({240.0F, 160.0F});
        ImGui::Begin("scroll-root");
        container.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();
    container.scroll_to_end = true;
    draw_frame();
    draw_frame();

    REQUIRE(container.current_scroll_y > 0.0F);
    REQUIRE(target->layout().visual_rect().min.x == Catch::Approx(target->actual_position.x));
    REQUIRE(target->layout().visual_rect().min.y == Catch::Approx(target->actual_position.y));
}

TEST_CASE("stack auto-sized axes reflow when the parent grows", "[layout][regression]") {
    ui_test::ImGuiContext context({400.0F, 180.0F});

    StackContainer stack("responsive-stack");
    stack.set_size({grow(), px(80.0F)});
    Node& hidden = stack.add<Node>("hidden-child");
    hidden.set_size({px(40.0F), px(40.0F)});
    hidden.set_visible(false);
    Container& child = stack.add<Container>("stretching-child");
    child.set_size({grow(), px(20.0F)});

    const auto draw_frame = [&stack](float width) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({width, 160.0F});
        ImGui::Begin("responsive-stack-test");
        stack.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame(220.0F);
    const float initial_stack_width = stack.layout().size().x;
    const float initial_child_width = child.layout().size().x;

    draw_frame(360.0F);

    REQUIRE(child.layout().local_rect().min.y == Catch::Approx(child.layout().parent_content_rect().min.y));
    REQUIRE(stack.layout().size().x > initial_stack_width);
    REQUIRE(child.layout().size().x > initial_child_width);
    REQUIRE(child.layout().size().x == Catch::Approx(stack.layout().size().x - stack.style().padding().x * 2.0F));
}

class VirtualRow : public ui::Node {
public:
    VirtualRow(int index, std::vector<int>& drawn) : m_index(index), m_drawn(drawn) {
        set_input_target();
    }

private:
    bool on_draw() override {
        m_drawn.push_back(m_index);
        ImGui::Dummy(layout().size());
        return true;
    }

    int m_index;
    std::vector<int>& m_drawn;
};

class VirtualListProbe : public ui::VirtualLayout {
public:
    VirtualListProbe() : VirtualLayout("virtual-list", 20.0F) {
        set_size({ui::px(180.0F), ui::px(100.0F)});
    }

    float scroll = 0.0F;
    float max_scroll = 0.0F;
    float requested_scroll = 0.0F;

private:
    void draw_children() override {
        scroll = ImGui::GetScrollY();
        max_scroll = ImGui::GetScrollMaxY();
        VirtualLayout::draw_children();
        ImGui::SetScrollY(requested_scroll);
    }
};

void draw_virtual_list(ui::VirtualLayout& list) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0.0F, 0.0F});
    ImGui::SetNextWindowSize({240.0F, 180.0F});
    ImGui::Begin("virtual-layout-test", nullptr, ImGuiWindowFlags_NoSavedSettings);
    list.update(1.0F);
    list.draw();
    ImGui::End();
    ImGui::EndFrame();
}

void set_virtual_items(ui::VirtualLayout& list, size_t count, std::vector<int>& drawn, std::map<size_t, VirtualRow*>& cache) {
    list.set_items(count, [&list, &drawn, &cache](size_t index) -> ui::Node& {
        const auto found = cache.find(index);
        if (found != cache.end()) return *found->second;

        auto& row = list.add<VirtualRow>(static_cast<int>(index), drawn);
        cache.emplace(index, &row);
        return row;
    });
}

TEST_CASE("virtual layout creates visible rows lazily and reuses the caller cache", "[layout][virtual-layout]") {
    ui_test::ImGuiContext context({240.0F, 180.0F});
    ui::InputRouter router;
    VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, VirtualRow*> cache;
    list.set_input_router(&router);
    list.set_spacing(3.0F);
    list.configure_all_styles([](ui::Style& style) { style.padding({7.0F, 5.0F}); });
    set_virtual_items(list, 1000, drawn, cache);
    REQUIRE(list.item_count() == 1000);
    REQUIRE(list.children().empty());
    REQUIRE(cache.empty());

    const auto frame = [&] {
        drawn.clear();
        router.begin_frame();
        draw_virtual_list(list);
    };
    frame();
    frame();

    REQUIRE_FALSE(drawn.empty());
    REQUIRE(drawn.front() == 0);
    REQUIRE(drawn.size() <= 6);
    REQUIRE(cache.size() <= 6);
    REQUIRE(list.max_scroll == Catch::Approx(1000.0F * 23.0F - 3.0F + 10.0F - 100.0F));
    REQUIRE(router.stats().entry_count <= 6);
    const auto& first = *cache.at(0);
    REQUIRE(first.layout().size().y == Catch::Approx(20.0F));
    const ImVec2 position = first.layout().visual_rect().min;
    REQUIRE(router.node_at({position.x + 2.0F, position.y + 2.0F}) == &first);

    list.requested_scroll = 2300.0F;
    frame();
    frame();
    REQUIRE(list.scroll == Catch::Approx(2300.0F));
    REQUIRE(drawn.front() >= 99);
    REQUIRE(std::find(drawn.begin(), drawn.end(), 100) != drawn.end());
    REQUIRE(drawn.size() <= 6);
    REQUIRE(router.stats().entry_count <= 6);
    REQUIRE(cache.size() <= 12);

    list.requested_scroll = list.max_scroll;
    frame();
    frame();
    REQUIRE(drawn.back() == 999);
    REQUIRE(drawn.size() <= 6);
    REQUIRE(cache.size() <= 18);

    const size_t cached_count = cache.size();
    list.requested_scroll = 0.0F;
    frame();
    frame();
    REQUIRE(cache.size() == cached_count);
    REQUIRE(cache.at(0) == &first);
    REQUIRE(list.children().size() == cached_count);

    auto removed = list.remove(*cache.at(0));
    cache.erase(0);
    frame();
    REQUIRE(cache.at(0) != removed.get());
    REQUIRE(cache.size() == cached_count);
}

TEST_CASE("virtual layout clips inside expanded rows and removes manual offsets", "[layout][virtual-layout]") {
    ui_test::ImGuiContext context({240.0F, 180.0F});
    VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, VirtualRow*> cache;
    set_virtual_items(list, 100, drawn, cache);
    list.set_extra_offset(2, 300.0F);
    list.set_extra_offset(20, 70.0F);
    const auto frame = [&] {
        drawn.clear();
        draw_virtual_list(list);
    };
    frame();
    frame();
    REQUIRE(list.max_scroll == Catch::Approx(2270.0F));
    REQUIRE(cache.at(2)->layout().size().y == Catch::Approx(320.0F));
    REQUIRE(cache.count(20) == 0);

    list.requested_scroll = 150.0F;
    frame();
    frame();
    REQUIRE(std::find(drawn.begin(), drawn.end(), 2) != drawn.end());
    const ui::Rect expanded = cache.at(2)->layout().visual_rect();
    REQUIRE(expanded.min.y < list.layout().visual_rect().min.y);
    REQUIRE(expanded.max.y > list.layout().visual_rect().max.y);
    REQUIRE(drawn.size() < 10);

    list.requested_scroll = 380.0F;
    frame();
    frame();
    REQUIRE(std::find(drawn.begin(), drawn.end(), 4) != drawn.end());
    REQUIRE(cache.at(4)->layout().local_rect().min.y == Catch::Approx(380.0F));

    list.set_extra_offset(2, 0.0F);
    frame();
    frame();
    REQUIRE(list.extra_offset(2) == 0.0F);
    REQUIRE(list.max_scroll == Catch::Approx(1970.0F));
    list.clear_extra_offsets();
    frame();
    frame();
    REQUIRE(list.max_scroll == Catch::Approx(1900.0F));
}

TEST_CASE("virtual layout handles empty lists hidden slots and fit measurement", "[layout][virtual-layout]") {
    ui_test::ImGuiContext context({240.0F, 180.0F});
    VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, VirtualRow*> cache;
    draw_virtual_list(list);
    REQUIRE(list.max_scroll == 0.0F);

    list.set_size({ui::px(180.0F), ui::fit()});
    list.set_spacing(4.0F);
    list.set_items(2, [&list, &drawn, &cache](size_t index) -> ui::Node& {
        const auto found = cache.find(index);
        if (found != cache.end()) return *found->second;

        auto& row = list.add<VirtualRow>(static_cast<int>(index), drawn);
        row.set_visible(index != 0);
        cache.emplace(index, &row);
        return row;
    });
    list.set_extra_offset(1, 10.0F);
    draw_virtual_list(list);
    draw_virtual_list(list);
    REQUIRE(list.layout().measured_size().y == Catch::Approx(54.0F));
    REQUIRE_FALSE(drawn.empty());
    REQUIRE(drawn.front() == 1);
    REQUIRE(cache.at(1)->layout().local_rect().min.y == Catch::Approx(24.0F));

    list.set_items(0);
    draw_virtual_list(list);
    REQUIRE(list.extra_offset(1) == 0.0F);
    list.set_items(2);
    draw_virtual_list(list);
    REQUIRE(list.layout().measured_size().y == Catch::Approx(44.0F));
    REQUIRE(cache.size() == 2);

    list.set_items(0, {});
    REQUIRE(list.item_count() == 0);
}

TEST_CASE("virtual layout rejects invalid dimensions and indices", "[layout][virtual-layout]") {
    REQUIRE_THROWS_AS(ui::VirtualLayout("invalid", 0.0F), std::invalid_argument);
    ui::VirtualLayout list("list", 20.0F);
    REQUIRE_THROWS_AS(list.set_item_height(-1.0F), std::invalid_argument);
    REQUIRE_THROWS_AS(list.set_spacing(std::numeric_limits<float>::infinity()), std::invalid_argument);
    REQUIRE_THROWS_AS(list.set_extra_offset(0, 10.0F), std::out_of_range);
    REQUIRE_THROWS_AS(list.set_items(10, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(list.set_items(std::numeric_limits<size_t>::max()), std::length_error);
    list.set_items(1, [&list](size_t) -> ui::Node& { return list.add<ui::Node>(); });
    REQUIRE_THROWS_AS(list.set_extra_offset(0, -1.0F), std::invalid_argument);
    REQUIRE_THROWS_AS(list.set_extra_offset(0, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
}

TEST_CASE("virtual layout overscan includes configurable neighbors across expanded rows", "[layout][virtual-layout]") {
    ui_test::ImGuiContext context({240.0F, 180.0F});
    VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, VirtualRow*> cache;
    set_virtual_items(list, 1000, drawn, cache);
    list.set_spacing(3.0F);
    list.set_extra_offset(100, 300.0F);
    list.set_extra_offset(103, 60.0F);

    const auto frame = [&] {
        drawn.clear();
        draw_virtual_list(list);
    };
    frame();
    frame();
    const float max_scroll = list.max_scroll;

    for (float scroll : {0.0F, 1171.0F, 2400.0F, max_scroll}) {
        list.set_overscan(0);
        list.requested_scroll = scroll;
        frame();
        frame();
        REQUIRE_FALSE(drawn.empty());
        const int first = drawn.front();
        const int last = drawn.back();

        for (int extra : {3, 7}) {
            list.set_overscan(static_cast<size_t>(extra));
            frame();
            REQUIRE(drawn.front() == std::max(0, first - extra));
            REQUIRE(drawn.back() == std::min(999, last + extra));
            REQUIRE(drawn.size() == static_cast<size_t>(drawn.back() - drawn.front() + 1));
            REQUIRE(list.max_scroll == Catch::Approx(max_scroll));
        }

        list.set_overscan(0);
        frame();
        REQUIRE(drawn.front() == first);
        REQUIRE(drawn.back() == last);
    }

    list.set_overscan(std::numeric_limits<size_t>::max());
    frame();
    REQUIRE(drawn.front() == 0);
    REQUIRE(drawn.back() == 999);
    REQUIRE(drawn.size() == 1000);
}

TEST_CASE("virtual layout measures newly created subtrees on their first draw", "[layout][virtual-layout]") {
    ui_test::ImGuiContext context({240.0F, 180.0F});
    VirtualListProbe list;
    ui::TextWidget* text = nullptr;
    list.set_items(1, [&list, &text](size_t) -> ui::Node& {
        auto& row = list.add<ui::StackContainer>("lazy-row");
        text = &row.add<ui::TextWidget>("lazy text");
        return row;
    });
    draw_virtual_list(list);
    REQUIRE(text != nullptr);
    REQUIRE(text->layout().measured_size().x > 0.0F);
    REQUIRE(text->layout().measured_size().y > 0.0F);
}
