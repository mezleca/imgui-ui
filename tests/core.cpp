#include <catch2/catch_test_macros.hpp>

#include "imgui-context.hpp"
#include <ui/layout/child-container.hpp>
#include <ui/layout/geometry.hpp>
#include <ui/layout/overlay-container.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>

#include <imgui_internal.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace ui;

TEST_CASE("ui nodes draw children between their begin and end hooks") {
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

        std::vector<std::string>& m_events;
        bool m_skip = false;
    };

    std::vector<std::string> events;
    DrawNode root("root", events);
    root.add(std::make_unique<DrawNode>("child", events));

    root.draw();
    REQUIRE(
        events == std::vector<std::string>{"root:layout", "root:begin", "child:layout", "child:begin", "child:end", "root:end"}
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

TEST_CASE("leaf nodes do not capture an imgui item produced before their draw") {
    class NoItemNode final : public ui::Node {
    public:
        using ui::Node::Node;
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

    ManualRectNode manual;
    manual.draw();

    ImGui::End();
    ImGui::EndFrame();

    const ui::Rect no_item_rect = no_item.layout().screen_rect();
    const ui::Rect manual_rect = manual.layout().screen_rect();

    REQUIRE_FALSE(no_item_rect.valid());
    REQUIRE(manual_rect.min.x == 40.0F);
    REQUIRE(manual_rect.min.y == 50.0F);
    REQUIRE(manual_rect.max.x == 70.0F);
    REQUIRE(manual_rect.max.y == 80.0F);
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
    skipped.set_placement(ui::Anchor::TopLeft, ui::Origin::TopLeft, {120.0F, 0.0F});

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

TEST_CASE("nodes can expose custom type names without core registration") {
    class AppNode final : public ui::Node {
    public:
        std::string_view type_name() const override {
            return "AppNode";
        }
    };

    AppNode node;
    ui::Widget widget("widget", "AppWidget");

    REQUIRE(node.type_name() == "AppNode");
    REQUIRE(widget.type_name() == "AppWidget");
}
