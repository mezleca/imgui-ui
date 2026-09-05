#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <ui/input/router.hpp>
#include <ui/layout/virtual-layout.hpp>
#include <ui/layout/stack-container.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/text.hpp>
#include "../examples/demo.hpp"
#include "imgui-context.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace ui_test {
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
} // namespace ui_test

TEST_CASE("virtual layout creates visible rows lazily and reuses the caller cache", "[layout][virtual-layout]") {
    ui_test::ImGuiContext context({240.0F, 180.0F});
    ui::InputRouter router;
    ui_test::VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, ui_test::VirtualRow*> cache;
    list.set_input_router(&router);
    list.set_spacing(3.0F);
    list.configure_all_styles([](ui::Style& style) { style.padding({7.0F, 5.0F}); });
    ui_test::set_virtual_items(list, 1000, drawn, cache);
    REQUIRE(list.item_count() == 1000);
    REQUIRE(list.children().empty());
    REQUIRE(cache.empty());

    const auto frame = [&] {
        drawn.clear();
        router.begin_frame();
        ui_test::draw_virtual_list(list);
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
    ui_test::VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, ui_test::VirtualRow*> cache;
    ui_test::set_virtual_items(list, 100, drawn, cache);
    list.set_extra_offset(2, 300.0F);
    list.set_extra_offset(20, 70.0F);
    const auto frame = [&] {
        drawn.clear();
        ui_test::draw_virtual_list(list);
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
    ui_test::VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, ui_test::VirtualRow*> cache;
    ui_test::draw_virtual_list(list);
    REQUIRE(list.max_scroll == 0.0F);

    list.set_size({ui::px(180.0F), ui::fit()});
    list.set_spacing(4.0F);
    list.set_items(2, [&list, &drawn, &cache](size_t index) -> ui::Node& {
        const auto found = cache.find(index);
        if (found != cache.end()) return *found->second;

        auto& row = list.add<ui_test::VirtualRow>(static_cast<int>(index), drawn);
        row.set_visible(index != 0);
        cache.emplace(index, &row);
        return row;
    });
    list.set_extra_offset(1, 10.0F);
    ui_test::draw_virtual_list(list);
    ui_test::draw_virtual_list(list);
    REQUIRE(list.layout().measured_size().y == Catch::Approx(54.0F));
    REQUIRE_FALSE(drawn.empty());
    REQUIRE(drawn.front() == 1);
    REQUIRE(cache.at(1)->layout().local_rect().min.y == Catch::Approx(24.0F));

    list.set_items(0);
    ui_test::draw_virtual_list(list);
    REQUIRE(list.extra_offset(1) == 0.0F);
    list.set_items(2);
    ui_test::draw_virtual_list(list);
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
    ui_test::VirtualListProbe list;
    std::vector<int> drawn;
    std::map<size_t, ui_test::VirtualRow*> cache;
    ui_test::set_virtual_items(list, 1000, drawn, cache);
    list.set_spacing(3.0F);
    list.set_extra_offset(100, 300.0F);
    list.set_extra_offset(103, 60.0F);

    const auto frame = [&] {
        drawn.clear();
        ui_test::draw_virtual_list(list);
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
    ui_test::VirtualListProbe list;
    ui::TextWidget* text = nullptr;
    list.set_items(1, [&list, &text](size_t) -> ui::Node& {
        auto& row = list.add<ui::StackContainer>("lazy-row");
        text = &row.add<ui::TextWidget>("lazy text");
        return row;
    });
    ui_test::draw_virtual_list(list);
    REQUIRE(text != nullptr);
    REQUIRE(text->layout().measured_size().x > 0.0F);
    REQUIRE(text->layout().measured_size().y > 0.0F);
}

TEST_CASE("demo virtual rows expand and collapse independently", "[layout][virtual-layout][demo]") {
    ui::Runtime runtime;
    UI surface(runtime);
    setup_demo(surface, "test");
    auto* list = dynamic_cast<ui::VirtualLayout*>(surface.root().find("demo-virtual-list"));
    REQUIRE(list != nullptr);
    REQUIRE(list->item_count() == 1000);
    REQUIRE(list->children().empty());

    auto detached = list->parent()->remove(*list);
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {240.0F, 180.0F};
    ui_test::ImGuiContext::build_fonts();
    list->set_size({ui::px(180.0F), ui::px(100.0F)});
    ui_test::draw_virtual_list(*list);
    ui_test::draw_virtual_list(*list);
    REQUIRE(list->children().size() < 10);
    auto* first = list->find("virtual-row-0");
    auto* second = list->find("virtual-row-1");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    ui::UiEvent click = ui::UiEvent::make(ui::EventType::Click);
    surface.input_router().dispatch(*first, click);
    REQUIRE(list->extra_offset(0) == 64.0F);
    surface.input_router().dispatch(*second, click);
    REQUIRE(list->extra_offset(1) == 64.0F);
    surface.input_router().dispatch(*first, click);
    REQUIRE(list->extra_offset(0) == 0.0F);
    REQUIRE(list->extra_offset(1) == 64.0F);
}
