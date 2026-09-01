#include <ui/style/state.hpp>
#include <ui/runtime.hpp>
#include <ui/layout/child-container.hpp>
#include <ui/layout/overlay-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/layout/stack-container.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/context-menu.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/number-input.hpp>
#include <ui/widgets/text.hpp>
#include <ui/widgets/text-input.hpp>
#include <ui/widgets/widget.hpp>
#include "../examples/demo.hpp"
#include "imgui-context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <cfloat>
#include <string>
#include <vector>

using namespace ui;

TEST_CASE("number input supports typed value updates", "[NumberInputWidget][value]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {400.0F, 180.0F}});
    int integer = 0;
    double decimal = 0.0;
    ui::NumberInputWidget integer_input(surface, integer);
    ui::NumberInputWidget decimal_input(surface, decimal);

    REQUIRE(integer_input.set_value(42));
    REQUIRE(integer == 42);
    REQUIRE(decimal_input.set_value(1.25));
    REQUIRE(decimal == Catch::Approx(1.25));
}

TEST_CASE("checkbox measurement includes style padding and remeasures after changes", "[CheckboxWidget][layout][style]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {400.0F, 180.0F}});
    bool value = false;
    ui::StackContainer stack("checkbox-padding-stack");
    stack.fit_content();
    stack.style().padding({});
    auto& checkbox = stack.add_child<ui::CheckboxWidget>(surface, value, "custom checkbox");
    checkbox.configure_all_styles([](ui::Style& style) { style.padding({10.0F, 6.0F}); });

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();

    const auto draw_frame = [&surface, &stack] {
        ImGui::GetIO().DisplaySize = {400.0F, 180.0F};
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({400.0F, 180.0F});
        ImGui::Begin("checkbox-padding-test");
        stack.draw();
        ImGui::End();
        surface.end_frame();
    };

    draw_frame();
    const ImVec2 initial_size = checkbox.layout().size();

    checkbox.style().padding({20.0F, 12.0F});
    draw_frame();

    REQUIRE(checkbox.layout().size().x == Catch::Approx(initial_size.x + 20.0F));
    REQUIRE(checkbox.layout().size().y == Catch::Approx(initial_size.y + 12.0F));
    REQUIRE(stack.layout().size().x == Catch::Approx(checkbox.layout().size().x));
    REQUIRE(stack.layout().size().y == Catch::Approx(checkbox.layout().size().y));
}

TEST_CASE("nested containers keep default padding empty and route checkbox clicks", "[container][input][regression]") {
    ui::RuntimeConfig config;
    config.theme.content_padding = 20.0F;
    ui::Runtime runtime(std::move(config));
    UI surface(runtime, {.size = {400.0F, 180.0F}});
    bool checked = false;

    auto& page = surface.root().add_child<ui::StackContainer>("page");
    page.set_size({320.0F, 120.0F});
    auto& section = page.add_child<ui::ChildContainer>("section");
    auto& form = section.add_child<ui::StackContainer>("form");
    auto& checkbox = form.add_child<ui::CheckboxWidget>(surface, checked, "enabled");

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 180.0F};

    const auto draw_frame = [&surface] {
        surface.begin_input_frame();
        surface.begin_frame();
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        surface.end_frame();
    };

    draw_frame();
    REQUIRE(runtime.theme().content_padding == Catch::Approx(20.0F));
    REQUIRE(page.style().padding().x == 0.0F);
    REQUIRE(page.style().padding().y == 0.0F);
    REQUIRE(section.style().padding().x == 0.0F);
    REQUIRE(section.style().padding().y == 0.0F);

    const ui::Rect rect = checkbox.layout().screen_rect();
    const ImVec2 position = {(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
    REQUIRE(surface.input_router().node_at(position) == &checkbox);

    ui::UiEvent down = ui::UiEvent::make(ui::EventType::PointerDown);
    down.position = position;
    down.button = ui::PointerButton::Left;
    surface.dispatch(down);

    ui::UiEvent up = ui::UiEvent::make(ui::EventType::PointerUp);
    up.position = position;
    up.button = ui::PointerButton::Left;
    surface.dispatch(up);
    REQUIRE(checked);
}

TEST_CASE("dropdown opens from a nested container without extending its parent", "[dropdown][container][regression]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {400.0F, 240.0F}});
    std::string value = "light";

    auto& page = surface.root().add_child<ui::StackContainer>("page");
    page.set_size({360.0F, 200.0F});
    auto& section = page.add_child<ui::ChildContainer>("section");
    auto& form = section.add_child<ui::StackContainer>("form");
    auto& dropdown = form.add_child<ui::DropdownWidget>(
        surface, value, std::vector<ui::DropdownOption>{{"light", "light"}, {"dark", "dark"}}, "theme"
    );
    dropdown.set_size({180.0F, 32.0F});
    bool checked = false;
    auto& checkbox = surface.root().add_child<ui::CheckboxWidget>(surface, checked, "enabled");
    checkbox.set_size({180.0F, 32.0F});

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 240.0F};

    const auto draw_frame = [&surface] {
        surface.begin_input_frame();
        surface.begin_frame();
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        surface.end_frame();
    };

    draw_frame();
    const ui::Rect trigger_rect = dropdown.trigger().layout().screen_rect();
    const ImVec2 trigger_center = {
        (trigger_rect.min.x + trigger_rect.max.x) * 0.5F,
        (trigger_rect.min.y + trigger_rect.max.y) * 0.5F,
    };

    ui::UiEvent down = ui::UiEvent::make(ui::EventType::PointerDown);
    down.position = trigger_center;
    down.button = ui::PointerButton::Left;
    surface.dispatch(down);

    ui::UiEvent up = ui::UiEvent::make(ui::EventType::PointerUp);
    up.position = trigger_center;
    up.button = ui::PointerButton::Left;
    surface.dispatch(up);

    draw_frame();
    REQUIRE(dropdown.is_open());
    const ui::Rect body_rect = dropdown.body().layout().screen_rect();
    REQUIRE(body_rect.min.y >= trigger_rect.max.y);
    surface.input_router().register_region(checkbox, {.rect = body_rect});

    const ImVec2 option_position = {
        (body_rect.min.x + body_rect.max.x) * 0.5F,
        (body_rect.min.y + body_rect.max.y) * 0.5F,
    };
    REQUIRE(surface.input_router().node_at(option_position) == &checkbox);

    down.position = option_position;
    surface.dispatch(down);
    up.position = option_position;
    surface.dispatch(up);
    REQUIRE_FALSE(checked);
}

TEST_CASE("text measurement and drawing include style padding", "[TextWidget][layout][style]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {400.0F, 180.0F}});
    ui::StackContainer stack("text-padding-stack");
    stack.fit_content();
    stack.style().padding({});
    auto& text = stack.add_child<ui::TextWidget>("padded text");
    text.configure_all_styles([](ui::Style& style) {
        style.padding({5.0F, 3.0F}).background_color(ImColor{10, 20, 30, 255}).border(ui::BORDER_ALL);
    });

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 180.0F};

    surface.begin_frame();
    ImFont* font = ImGui::GetFont();
    const ImVec2 raw_size = font->CalcTextSizeA(font->LegacySize, FLT_MAX, 0.0F, "padded text");
    ImGui::Begin("text-padding-test");
    stack.draw();
    ImGui::End();
    surface.end_frame();

    REQUIRE(text.layout().size().x == Catch::Approx(raw_size.x + 10.0F));
    REQUIRE(text.layout().size().y == Catch::Approx(raw_size.y + 6.0F));
}

TEST_CASE("unwrapped text keeps its explicit width for overflow", "[TextWidget][layout]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {400.0F, 180.0F}});
    ui::TextWidget clipped("this text exceeds the explicit width");
    ui::TextWidget ellipsized("this text exceeds the explicit width");
    clipped.set_size({80.0F, 24.0F});
    ellipsized.set_size({80.0F, 24.0F}).set_overflow(ui::TextOverflow::Ellipsis);

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 180.0F};

    surface.begin_frame();
    ImGui::Begin("text-overflow-test");
    clipped.draw();
    ellipsized.draw();
    ImGui::End();
    surface.end_frame();

    REQUIRE(clipped.overflow() == ui::TextOverflow::Clip);
    REQUIRE(ellipsized.overflow() == ui::TextOverflow::Ellipsis);
    REQUIRE(clipped.layout().size().x == Catch::Approx(80.0F));
    REQUIRE(ellipsized.layout().size().x == Catch::Approx(80.0F));
}

TEST_CASE("text line height scales multi-line text layout", "[TextWidget][layout][style]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {400.0F, 180.0F}});
    ui::TextWidget text("first line\nsecond line");
    text.configure_all_styles([](ui::Style& style) { style.padding({}).line_height(1.5F); });

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 180.0F};

    surface.begin_frame();
    const float native_line_height = ImGui::GetTextLineHeight();
    ImGui::Begin("text-line-height-test");
    text.draw();
    ImGui::End();
    surface.end_frame();

    REQUIRE(text.layout().size().y == Catch::Approx(native_line_height * 3.0F));
}

TEST_CASE("text line height interpolates between visual states", "[TextWidget][style]") {
    ui::TextWidget text("line");
    text.configure_style(ui::StyleType::HOVER, [](ui::Style& style) { style.line_height(2.0F, 1.0F); });

    text.set_interaction_style(true, false);
    text.update(0.5F);

    REQUIRE(text.style().line_height() == Catch::Approx(1.5F));
}

TEST_CASE("value widgets notify changes", "[Widget][change]") {
    Runtime runtime;
    UI surface(runtime, Config{});
    bool checked = false;
    int number = 1;
    std::string choice = "one";
    std::string text = "before";
    int changes = 0;

    CheckboxWidget checkbox(surface, checked, "checked");
    NumberInputWidget input(surface, number);
    DropdownWidget dropdown(surface, choice, {{"one", "one"}, {"two", "two"}});
    TextInputWidget text_input(surface, text);

    checkbox.on_change = [&changes] { ++changes; };
    input.on_change = [&changes] { ++changes; };
    dropdown.on_change = [&changes] { ++changes; };
    text_input.on_change = [&changes] { ++changes; };

    checkbox.set_checked(true);
    REQUIRE(changes == 1);
    REQUIRE(input.set_value(2));
    REQUIRE(changes == 2);
    REQUIRE(dropdown.select_value("two"));
    REQUIRE(changes == 3);
    REQUIRE(text_input.set_value("after"));
    REQUIRE(changes == 4);

    checkbox.set_checked(true);
    REQUIRE_FALSE(input.set_value(2));
    REQUIRE_FALSE(dropdown.select_value("two"));
    REQUIRE_FALSE(text_input.set_value("after"));
    REQUIRE(changes == 4);
}

TEST_CASE("text input follows a resized parent width", "[TextInputWidget][layout][regression]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {400.0F, 180.0F}});
    std::string value;
    ui::ResizableContainer parent("resizable");
    parent.set_size({180.0F, 80.0F});
    auto& input = parent.add_child<ui::TextInputWidget>(surface, value, "input");

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();

    const auto draw_frame = [&surface, &parent] {
        ImGui::GetIO().DisplaySize = {400.0F, 180.0F};
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({400.0F, 180.0F});
        ImGui::Begin("text-input-resize-test");
        parent.draw();
        ImGui::End();
        surface.end_frame();
    };

    draw_frame();
    const float initial_width = input.layout().size().x;
    parent.set_size({280.0F, 80.0F});
    draw_frame();
    const float expanded_width = input.layout().size().x;

    parent.set_size({140.0F, 80.0F});
    draw_frame();

    REQUIRE(expanded_width > initial_width);
    REQUIRE(input.layout().size().x < initial_width);
    REQUIRE(input.layout().size().y < parent.layout().size().y);
}

TEST_CASE("pointer block prevents hover and clicks on content controls", "[input][regression]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {900.0F, 600.0F}});
    setup_demo(surface, "test");

    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {900.0F, 600.0F};
    ui_test::ImGuiContext::build_fonts();

    const auto draw_frame = [&surface](ImVec2 mouse_position, bool mouse_down = false) {
        ImGui::SetCurrentContext(surface.imgui_context());
        ImGui::GetIO().MousePos = mouse_position;
        ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] = mouse_down;
        surface.begin_input_frame();
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({900.0F, 600.0F});
        ImGui::Begin("demo-input-test");
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        ImGui::End();
        surface.end_frame();
    };

    draw_frame({0.0F, 0.0F});

    auto* blocker = surface.root().find("##input-blocker");
    auto* controls = surface.root().find("dynamic-node-controls");
    auto* dynamic_nodes = surface.root().find("dynamic-nodes");
    auto* blocker_overlay = dynamic_cast<ui::OverlayNode*>(blocker);
    REQUIRE(blocker != nullptr);
    REQUIRE(blocker_overlay != nullptr);
    REQUIRE(controls != nullptr);
    REQUIRE(dynamic_nodes != nullptr);
    REQUIRE_FALSE(controls->children().empty());

    blocker_overlay->set_visible(true);
    blocker_overlay->set_blocks_pointer_input(true);
    draw_frame({0.0F, 0.0F});

    auto* add_button = dynamic_cast<ui::ButtonWidget*>(controls->children().front().get());
    REQUIRE(add_button != nullptr);
    const ui::Rect add_button_rect = add_button->layout().screen_rect();
    const ImVec2 add_button_center = {
        (add_button_rect.min.x + add_button_rect.max.x) * 0.5F,
        (add_button_rect.min.y + add_button_rect.max.y) * 0.5F,
    };

    draw_frame(add_button_center, true);
    draw_frame(add_button_center, false);
    REQUIRE(add_button->style_type() == ui::StyleType::DEFAULT);

    ui::UiEvent down = ui::UiEvent::make(ui::EventType::PointerDown);
    down.position = add_button_center;
    down.button = ui::PointerButton::Left;
    surface.dispatch(down);

    ui::UiEvent up = ui::UiEvent::make(ui::EventType::PointerUp);
    up.position = add_button_center;
    up.button = ui::PointerButton::Left;
    surface.dispatch(up);

    REQUIRE(dynamic_nodes->children().empty());
}

TEST_CASE("pointer block rejects clicks on another overlay control", "[input][regression]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {900.0F, 600.0F}});
    setup_demo(surface, "test");

    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {900.0F, 600.0F};
    ui_test::ImGuiContext::build_fonts();

    const auto draw_frame = [&surface](ImVec2 mouse_position) {
        ImGui::SetCurrentContext(surface.imgui_context());
        ImGui::GetIO().MousePos = mouse_position;
        surface.begin_input_frame();
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({900.0F, 600.0F});
        ImGui::Begin("demo-overlay-input-test");
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        ImGui::End();
        surface.end_frame();
    };

    draw_frame({0.0F, 0.0F});

    auto* overlay = surface.root().find("##demo-overlay");
    auto* blocker = surface.root().find("##input-blocker");
    auto* blocker_overlay = dynamic_cast<ui::OverlayNode*>(blocker);
    REQUIRE(overlay != nullptr);
    REQUIRE(blocker != nullptr);
    REQUIRE(blocker_overlay != nullptr);
    REQUIRE(overlay->children().size() >= 2);

    auto* panel = overlay->children().front().get();
    auto* show_button = dynamic_cast<ui::ButtonWidget*>(overlay->children().back().get());
    REQUIRE(panel != nullptr);
    REQUIRE(show_button != nullptr);
    REQUIRE_FALSE(panel->visible());

    blocker_overlay->set_visible(true);
    blocker_overlay->set_blocks_pointer_input(true);
    draw_frame({0.0F, 0.0F});

    const ui::Rect button_rect = show_button->layout().screen_rect();
    const ImVec2 button_center = {
        (button_rect.min.x + button_rect.max.x) * 0.5F,
        (button_rect.min.y + button_rect.max.y) * 0.5F,
    };

    ui::UiEvent down = ui::UiEvent::make(ui::EventType::PointerDown);
    down.position = button_center;
    down.button = ui::PointerButton::Left;
    surface.dispatch(down);

    ui::UiEvent up = ui::UiEvent::make(ui::EventType::PointerUp);
    up.position = button_center;
    up.button = ui::PointerButton::Left;
    surface.dispatch(up);

    REQUIRE_FALSE(panel->visible());
}

TEST_CASE("dropdown opens after fading out and fades after selection", "[DropdownWidget][regression]") {
    ui::Runtime runtime;
    UI surface(runtime, {.size = {320.0F, 220.0F}});
    std::string value = "blue";
    int changes = 0;
    ui::StackContainer stack("dropdown-stack");
    stack.set_input_router(&surface.input_router());
    stack.set_size({280.0F, 180.0F});
    stack.set_spacing(16.0F);
    auto& dropdown = stack.add_child<ui::DropdownWidget>(
        surface, value, std::vector<ui::DropdownOption>{{"blue", "blue"}, {"high contrast", "contrast"}}, "theme"
    );
    dropdown.set_size({180.0F, 62.0F});
    dropdown.set_label("theme");
    dropdown.on_change = [&changes] { ++changes; };
    auto& status = stack.add_child<ui::TextWidget>("no clicks yet");

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();

    bool previous_mouse_down = false;
    const auto draw_frame = [&surface, &stack, &dropdown, &previous_mouse_down](ImVec2 mouse_position, bool mouse_down) {
        ImGui::SetCurrentContext(surface.imgui_context());
        ImGui::GetIO().DisplaySize = {320.0F, 220.0F};
        ImGui::GetIO().MousePos = mouse_position;
        ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] = mouse_down;

        if (mouse_down && !previous_mouse_down) {
            ui::UiEvent event = ui::UiEvent::make(ui::EventType::PointerDown);
            event.position = mouse_position;
            event.button = ui::PointerButton::Left;
            surface.dispatch(event);
        } else if (!mouse_down && previous_mouse_down) {
            ui::UiEvent event = ui::UiEvent::make(ui::EventType::PointerUp);
            event.position = mouse_position;
            event.button = ui::PointerButton::Left;
            surface.dispatch(event);
        }
        previous_mouse_down = mouse_down;

        surface.begin_input_frame();
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({320.0F, 220.0F});
        ImGui::Begin("dropdown-test");
        stack.update(ImGui::GetIO().DeltaTime);
        stack.draw();
        ImGui::End();
        surface.end_frame();
        return dropdown.is_open();
    };

    for (int frame = 0; frame < 12; ++frame) {
        REQUIRE_FALSE(draw_frame({0.0F, 0.0F}, false));
    }
    REQUIRE_FALSE(dropdown.body().visually_visible());
    REQUIRE(status.layout().screen_rect().min.y >= dropdown.trigger().layout().screen_rect().max.y + stack.spacing());

    const ui::Rect trigger_rect = dropdown.trigger().layout().screen_rect();
    const ImVec2 trigger_center = {
        (trigger_rect.min.x + trigger_rect.max.x) * 0.5F,
        (trigger_rect.min.y + trigger_rect.max.y) * 0.5F,
    };

    REQUIRE_FALSE(draw_frame(trigger_center, true));
    REQUIRE(draw_frame(trigger_center, false));
    REQUIRE(draw_frame(trigger_center, false));
    REQUIRE(draw_frame({0.0F, 0.0F}, false));
    REQUIRE(dropdown.is_open());

    REQUIRE(draw_frame(trigger_center, true));
    REQUIRE_FALSE(draw_frame(trigger_center, false));
    REQUIRE_FALSE(dropdown.is_open());
    REQUIRE(dropdown.body().visually_visible());
    REQUIRE_FALSE(dropdown.body().accepts_input());
    REQUIRE_FALSE(draw_frame(trigger_center, true));
    REQUIRE_FALSE(draw_frame(trigger_center, false));
    REQUIRE_FALSE(dropdown.is_open());
    const float trigger_closing_opacity = dropdown.body().opacity();
    REQUIRE_FALSE(draw_frame({0.0F, 0.0F}, false));
    REQUIRE(dropdown.body().opacity() < trigger_closing_opacity);

    for (int frame = 0; frame < 12; ++frame) {
        REQUIRE_FALSE(draw_frame({0.0F, 0.0F}, false));
    }

    REQUIRE_FALSE(draw_frame(trigger_center, true));
    REQUIRE(draw_frame(trigger_center, false));
    REQUIRE(draw_frame({0.0F, 0.0F}, false));
    REQUIRE(dropdown.is_open());

    const ui::Rect body_rect = dropdown.body().layout().screen_rect();
    const float item_height = ImGui::GetTextLineHeight() + 8.0F;
    const ImVec2 second_option = {
        (body_rect.min.x + body_rect.max.x) * 0.5F,
        body_rect.min.y + item_height * 1.5F,
    };
    REQUIRE(draw_frame(second_option, true));
    REQUIRE_FALSE(draw_frame(second_option, false));
    REQUIRE(value == "contrast");
    REQUIRE(changes == 1);
    REQUIRE_FALSE(dropdown.body().accepts_input());
    const float closing_opacity = dropdown.body().opacity();
    REQUIRE_FALSE(draw_frame(second_option, false));
    REQUIRE(dropdown.body().opacity() < closing_opacity);

    for (int frame = 0; frame < 12; ++frame) {
        draw_frame({0.0F, 0.0F}, false);
    }
    REQUIRE_FALSE(dropdown.body().visually_visible());
}

TEST_CASE("runtime owns shared theme and explicitly registered assets", "[Runtime]") {
    ui::RuntimeConfig config;
    config.theme.content_padding = 20.0F;
    config.theme.box_rounding = 8.0F;
    ui::Runtime runtime(std::move(config));
    ui::Font* font = runtime.add_font(ui::FontType::REGULAR, "fonts/regular.ttf");

    REQUIRE(runtime.theme().content_padding == 20.0F);
    REQUIRE(font == runtime.find_font(ui::FontType::REGULAR));
    REQUIRE(runtime.find_resource("default") == nullptr);

    ui::Runtime other_runtime;

    REQUIRE(runtime.theme().box_rounding == 8.0F);
    REQUIRE(other_runtime.theme().content_padding == ui::Theme::defaults().content_padding);
}

TEST_CASE("style transition duration uses seconds", "[VisualState][transition]") {
    ui::VisualState state;

    state.style(ui::StyleType::DEFAULT).color({0.0f, 0.0f, 0.0f, 1.0f});
    state.style(ui::StyleType::HOVER).color({1.0f, 0.0f, 0.0f, 1.0f}, 0.2F);

    state.style(ui::StyleType::HOVER).variables().set("rounding", ui::FloatValue{10.0f, 0.2f});
    state.style(ui::StyleType::DEFAULT).variables().set("rounding", ui::FloatValue{0.0f, 0.0f});

    state.style(ui::StyleType::HOVER).variables().set("enabled", ui::BoolValue{true});
    state.style(ui::StyleType::DEFAULT).variables().set("enabled", ui::BoolValue{false});

    ui::Vec2Value hover_offset;
    hover_offset.value = {5.0f, 5.0f};
    hover_offset.duration = 0.2f;
    state.style(ui::StyleType::HOVER).variables().set("offset", hover_offset);

    ui::Vec2Value default_offset;
    default_offset.value = {0.0f, 0.0f};
    state.style(ui::StyleType::DEFAULT).variables().set("offset", default_offset);

    state.style(ui::StyleType::HOVER).variables().set("count", ui::IntValue{100, 0.2f});
    state.style(ui::StyleType::DEFAULT).variables().set("count", ui::IntValue{0, 0.0f});

    state.snap_to_style(ui::StyleType::DEFAULT);
    state.set_style(ui::StyleType::HOVER);

    state.update(0.1F);
    REQUIRE(state.style().color().get().x == Catch::Approx(0.5F));
    REQUIRE(state.style().variables().get<ui::FloatValue>("rounding")->value == Catch::Approx(5.0F));
    REQUIRE(state.style().variables().get<ui::Vec2Value>("offset")->value.x == Catch::Approx(2.5F));
    REQUIRE(state.style().variables().get<ui::IntValue>("count")->value == 50);
    REQUIRE(state.style().variables().get<ui::BoolValue>("enabled")->value);

    state.update(0.1F);

    SECTION("discrete type snaps immediately") {
        REQUIRE(state.style().variables().get<ui::BoolValue>("enabled")->value);
    }

    SECTION("color converges") {
        REQUIRE(state.style().color().get().x == Catch::Approx(1.0f).margin(0.01f));
    }

    SECTION("float var converges") {
        REQUIRE(state.style().variables().get<ui::FloatValue>("rounding")->value == Catch::Approx(10.0f).margin(0.1f));
    }

    SECTION("vec2 var converges") {
        REQUIRE(state.style().variables().get<ui::Vec2Value>("offset")->value.x == Catch::Approx(5.0f).margin(0.1f));
    }

    SECTION("int var reaches exact target") {
        REQUIRE(state.style().variables().get<ui::IntValue>("count")->value == 100);
    }
}

TEST_CASE("interaction style precedence is active focus hover default", "[VisualState][style]") {
    ui::VisualState state;

    state.set_item_state(false, false, false);
    REQUIRE(state.style_type() == ui::StyleType::DEFAULT);

    state.set_item_state(true, false, false);
    REQUIRE(state.style_type() == ui::StyleType::HOVER);

    state.set_item_state(true, false, true);
    REQUIRE(state.style_type() == ui::StyleType::FOCUS);

    state.set_item_state(true, true, true);
    REQUIRE(state.style_type() == ui::StyleType::ACTIVE);
}

TEST_CASE("style cursor follows hovered nodes", "[Style][cursor]") {
    ui_test::ImGuiContext context({320.0F, 180.0F});
    ui::InputRouter router;
    ui::Widget widget("cursor-widget");
    widget.configure_style(ui::StyleType::HOVER, [](ui::Style& style) { style.cursor(ImGuiMouseCursor_Hand); });

    ImGui::NewFrame();
    router.begin_frame();
    router.register_region(widget, {.rect = {{0.0F, 0.0F}, {40.0F, 20.0F}}});

    ui::UiEvent move = ui::UiEvent::make(ui::EventType::PointerMove);
    move.position = {10.0F, 10.0F};
    router.dispatch(move);
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);

    move.position = {100.0F, 100.0F};
    router.dispatch(move);
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow);
    ImGui::EndFrame();
}

TEST_CASE("border alpha fades out when a hover state is cleared", "[VisualState][transition]") {
    ui::VisualState state;
    const ImColor accent = ImColor(233, 30, 115, 255);
    const ImColor hidden_accent = ui::with_alpha(accent, 0.0F);

    state.configure_all_styles([&](ui::Style& style) { style.border_color(hidden_accent, 0.2F); });
    state.configure_style(ui::StyleType::HOVER, [&](ui::Style& style) { style.border_color(accent); });

    state.set_style(ui::StyleType::HOVER);
    state.update(0.2F);
    const float visible_alpha = state.style().border_color().get().w;

    state.set_style(ui::StyleType::DEFAULT);
    state.update(0.1F);
    const ImVec4 fading_color = state.style().border_color().get();

    REQUIRE(visible_alpha > 0.0F);
    REQUIRE(fading_color.w > 0.0F);
    REQUIRE(fading_color.w < visible_alpha);
    REQUIRE(fading_color.x == Catch::Approx(accent.Value.x));
    REQUIRE(fading_color.y == Catch::Approx(accent.Value.y));
    REQUIRE(fading_color.z == Catch::Approx(accent.Value.z));

    state.update(0.1F);
    REQUIRE(state.style().border_color().get().w == Catch::Approx(0.0F));
}

TEST_CASE("opacity ticks towards target and drives visibility", "[widget_state][opacity]") {
    ui::VisualState state;
    state.set_opacity(0.0f);

    state.update(0.075F);
    REQUIRE(state.opacity() == Catch::Approx(0.5F));
    state.update(0.075F);
    REQUIRE(state.opacity() == Catch::Approx(0.0F));
    REQUIRE_FALSE(state.is_visible());
}

TEST_CASE("fade transitions control input independently from drawing", "[widget_state][opacity]") {
    ui::VisualState state;
    state.update(1.0f / 60.0f);
    REQUIRE(state.accepts_input());

    state.fade_out();
    REQUIRE_FALSE(state.accepts_input());
    REQUIRE(state.is_visible());

    state.fade_in();
    REQUIRE(state.accepts_input());
}

TEST_CASE("widget input requires both node and visual state to accept input", "[Widget][input]") {
    ui::Widget widget("widget");
    ui::InputRouter router;
    router.register_region(widget, {.rect = {{0.0F, 0.0F}, {10.0F, 10.0F}}});

    REQUIRE(widget.accepts_input());
    REQUIRE(router.node_at({5.0F, 5.0F}) == &widget);

    widget.set_enabled(false);
    REQUIRE_FALSE(widget.accepts_input());
    REQUIRE(router.node_at({5.0F, 5.0F}) == nullptr);

    widget.set_enabled(true);
    widget.set_visible(false);
    REQUIRE_FALSE(widget.accepts_input());

    widget.set_visible(true);
    widget.fade_out();
    REQUIRE_FALSE(widget.accepts_input());
    REQUIRE(router.node_at({5.0F, 5.0F}) == nullptr);
}

TEST_CASE("styled nodes apply their effective font during draw", "[Widget][style][regression]") {
    class FontProbeWidget final : public ui::Widget {
    public:
        FontProbeWidget() : ui::Widget("font-probe") {}

        ImFont* observed_font = nullptr;

    private:
        bool paint() override {
            observed_font = ImGui::GetFont();
            ImGui::Dummy({10.0F, 10.0F});
            return true;
        }
    };

    ui_test::ImGuiContext context({160.0F, 120.0F});
    ImFontConfig font_config;
    font_config.SizePixels = 24.0F;
    ImFont* large_font = ImGui::GetIO().Fonts->AddFontDefault(&font_config);
    ui_test::ImGuiContext::build_fonts();

    FontProbeWidget widget;
    widget.set_font(large_font);

    ImGui::NewFrame();
    ImGui::Begin("styled-font-test");
    widget.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(widget.observed_font == large_font);
}

TEST_CASE("styled nodes keep borders out of imgui style scope", "[Widget][style][regression]") {
    class StyleProbeWidget final : public ui::Widget {
    public:
        StyleProbeWidget() : ui::Widget("style-probe") {}

        ImVec2 observed_padding{};
        float observed_rounding = 0.0F;
        float observed_border_size = 0.0F;
        float observed_alpha = 0.0F;
        ImVec4 observed_text{};
        ImVec4 observed_background{};

    private:
        bool paint() override {
            const ImGuiStyle& imgui_style = ImGui::GetStyle();
            observed_padding = imgui_style.FramePadding;
            observed_rounding = imgui_style.FrameRounding;
            observed_border_size = imgui_style.FrameBorderSize;
            observed_alpha = imgui_style.Alpha;
            observed_text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            observed_background = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
            ImGui::Dummy({10.0F, 10.0F});
            return true;
        }
    };

    ui_test::ImGuiContext context({160.0F, 120.0F});
    StyleProbeWidget widget;

    widget.configure_all_styles([](ui::Style& style) {
        style.color(ImColor{51, 102, 153, 255})
            .background_color(ImColor{26, 77, 128, 255})
            .padding({7.0F, 9.0F})
            .border(ui::BORDER_ALL)
            .border_radius(6.0F)
            .border_thickness(3.0F)
            .alpha(0.5F);
    });

    widget.update(0.0F);

    const ImGuiStyle before = ImGui::GetStyle();
    ImGui::NewFrame();
    ImGui::Begin("styled-scope-test");
    widget.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(widget.observed_padding.x == Catch::Approx(7.0F));
    REQUIRE(widget.observed_padding.y == Catch::Approx(9.0F));
    REQUIRE(widget.observed_rounding == Catch::Approx(6.0F));
    REQUIRE(widget.observed_border_size == Catch::Approx(0.0F));
    REQUIRE(widget.observed_alpha == Catch::Approx(before.Alpha * 0.5F));
    REQUIRE(widget.observed_text.x == Catch::Approx(0.2F).margin(0.01F));
    REQUIRE(widget.observed_background.y == Catch::Approx(0.3F).margin(0.01F));
    REQUIRE(ImGui::GetStyle().FramePadding.x == Catch::Approx(before.FramePadding.x));
    REQUIRE(ImGui::GetStyle().Alpha == Catch::Approx(before.Alpha));
}

TEST_CASE("widgets skip drawing after their fade becomes invisible", "[Widget][opacity][regression]") {
    class DrawProbeWidget final : public ui::Widget {
    public:
        DrawProbeWidget() : ui::Widget("draw-probe") {}

        int draws = 0;

    private:
        bool paint() override {
            ++draws;
            ImGui::Dummy({10.0F, 10.0F});
            return true;
        }
    };

    ui_test::ImGuiContext context({160.0F, 120.0F});
    DrawProbeWidget widget;

    widget.update(0.0F);

    const auto draw_frame = [&widget] {
        ImGui::NewFrame();
        ImGui::Begin("widget-visibility-test");
        widget.draw();
        ImGui::End();
        ImGui::EndFrame();
    };

    draw_frame();
    REQUIRE(widget.draws == 1);

    widget.fade_out();
    widget.update(ui::OPACITY_TRANSITION_DURATION);
    draw_frame();
    REQUIRE(widget.draws == 1);
}

TEST_CASE("styled widgets advance visual state during update", "[Widget][style]") {
    ui_test::ImGuiContext context({160.0F, 120.0F});

    ui::Widget widget("widget");
    widget.configure_all_styles([](ui::Style& style) { style.color(ImColor{0, 0, 0, 255}, 0.2F); });
    widget.configure_style(ui::StyleType::HOVER, [](ui::Style& style) { style.color(ImColor{255, 0, 0, 255}, 0.2F); });
    widget.set_visual_style(ui::StyleType::HOVER);

    ui::VisualState expected;
    expected.configure_all_styles([](ui::Style& style) { style.color(ImColor{0, 0, 0, 255}, 0.2F); });
    expected.configure_style(ui::StyleType::HOVER, [](ui::Style& style) { style.color(ImColor{255, 0, 0, 255}, 0.2F); });
    expected.set_style(ui::StyleType::HOVER);
    expected.update(ImGui::GetIO().DeltaTime);

    widget.update(ImGui::GetIO().DeltaTime);
    const float color_after_update = widget.style().color().get().x;

    ImGui::NewFrame();
    ImGui::Begin("style-tick-test");
    widget.draw();
    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(widget.style().color().get().x == Catch::Approx(expected.style().color().get().x));
    REQUIRE(widget.style().color().get().x == Catch::Approx(color_after_update));

    widget.configure_style(ui::StyleType::FOCUS, [](ui::Style& style) { style.border_radius(12.0F); });
    REQUIRE(widget.style(ui::StyleType::FOCUS).border_radius() == Catch::Approx(12.0F));
}

TEST_CASE("custom update hooks cannot skip visual state advancement", "[Widget][style][regression]") {
    class UpdatingWidget final : public ui::Widget {
    public:
        UpdatingWidget() : ui::Widget("updating-widget") {}

        int updates = 0;

    private:
        void on_update(float) override {
            ++updates;
        }
    };

    UpdatingWidget widget;
    widget.configure_all_styles([](ui::Style& style) { style.alpha(0.0F); });
    widget.configure_style(ui::StyleType::HOVER, [](ui::Style& style) { style.alpha(1.0F); });
    widget.set_visual_style(ui::StyleType::HOVER);

    widget.update(1.0F / 60.0F);

    REQUIRE(widget.updates == 1);
    REQUIRE(widget.style().alpha() == Catch::Approx(1.0F));
}

TEST_CASE("fade in starts new visual states transparent", "[widget_state][opacity]") {
    ui::VisualState state;

    state.fade_in();
    REQUIRE(state.opacity() == Catch::Approx(0.0F));

    state.update(1.0F / 60.0F);
    REQUIRE(state.opacity() > 0.0F);
    REQUIRE(state.opacity() < 1.0F);
}

TEST_CASE("a var introduced only on the target style still appears after transition", "[widget_state][regression]") {
    ui::VisualState state;

    state.style(ui::StyleType::DEFAULT).variables().set("line_alpha", ui::FloatValue{0.0f, 0.15f});
    state.style(ui::StyleType::HOVER).variables().set("line_alpha", ui::FloatValue{1.0f, 0.15f});

    const ui::FloatValue* default_alpha = state.style().variables().get<ui::FloatValue>("line_alpha");
    REQUIRE(default_alpha != nullptr);
    REQUIRE(default_alpha->value == Catch::Approx(0.0F));

    state.set_style(ui::StyleType::HOVER);
    state.update(1.0f / 60.0f); // first transition frame

    REQUIRE(state.style().variables().get<ui::FloatValue>("line_alpha") != nullptr);
}

TEST_CASE("style variables stay local to their declared state", "[VisualState][variables]") {
    ui::VisualState state;
    state.style(ui::StyleType::HOVER).variables().set("line_width", ui::FloatValue{2.0F, 0.15F});

    state.set_style(ui::StyleType::HOVER);
    state.update(1.0F / 60.0F);
    REQUIRE(state.style().variables().get<ui::FloatValue>("line_width") != nullptr);

    state.set_style(ui::StyleType::ACTIVE);
    state.update(1.0F / 60.0F);
    REQUIRE(state.style().variables().get<ui::FloatValue>("line_width") == nullptr);
}

TEST_CASE("editing the selected style updates its effective appearance") {
    ui::VisualState state;
    state.style(ui::StyleType::DEFAULT).color(ImColor{0, 0, 0, 255});
    state.style(ui::StyleType::ACTIVE).color(ImColor{255, 0, 0, 255});
    state.snap_to_style(ui::StyleType::DEFAULT);
    state.set_style(ui::StyleType::ACTIVE);
    state.update(1.0F / 60.0F);

    state.style(ui::StyleType::ACTIVE).color().set(ImColor{0, 255, 0, 255});

    const ui::VisualState& const_state = state;
    REQUIRE(const_state.style().color().get().x == Catch::Approx(0.0F));
    REQUIRE(const_state.style().color().get().y == Catch::Approx(1.0F));
}

namespace context_menu_test {
    void draw_frame(UI& surface, float dt = 0.2F) {
        surface.begin_input_frame();
        surface.begin_frame();
        surface.root().update(dt);
        surface.root().draw();
        surface.end_frame();
    }

    UiEvent pointer_event(EventType type, ImVec2 position) {
        UiEvent event = UiEvent::make(type);
        event.position = position;
        event.button = PointerButton::Left;
        return event;
    }
} // namespace context_menu_test

TEST_CASE("context menu clamps its position and fades out", "[ContextMenuWidget]") {
    Runtime runtime;
    UI surface(runtime, {.size = {320.0F, 240.0F}});
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {320.0F, 240.0F};
    ui_test::ImGuiContext::build_fonts();

    ContextMenuItems items;
    items.push_back({.label = "item"});
    auto& menu = surface.root().add_child<ContextMenuWidget>(surface, std::move(items));

    REQUIRE_FALSE(menu.visible());
    menu.show({300.0F, 220.0F});
    context_menu_test::draw_frame(surface);

    REQUIRE(menu.is_open());
    REQUIRE(menu.layout().screen_rect().min.x == Catch::Approx(136.0F));
    REQUIRE(menu.layout().screen_rect().min.y == Catch::Approx(204.0F));

    ImGui::GetIO().MousePos = {140.0F, 208.0F};
    context_menu_test::draw_frame(surface, 0.01F);

    ImGui::GetIO().MousePos = {0.0F, 0.0F};
    context_menu_test::draw_frame(surface, 0.78F);
    REQUIRE(menu.is_open());

    context_menu_test::draw_frame(surface, 0.02F);
    REQUIRE_FALSE(menu.is_open());
    context_menu_test::draw_frame(surface);
    REQUIRE_FALSE(menu.visible());
}

TEST_CASE("context menu item callbacks can keep the root menu open", "[ContextMenuWidget]") {
    Runtime runtime;
    UI surface(runtime, {.size = {320.0F, 240.0F}});
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {320.0F, 240.0F};
    ui_test::ImGuiContext::build_fonts();

    bool callback_called = false;
    ContextMenuItems items;
    items.push_back({
        .label = "keep open",
        .on_click = [&callback_called](ContextMenuWidget& menu) {
            callback_called = true;
            menu.cancel_close_request();
        },
    });
    auto& menu = surface.root().add_child<ContextMenuWidget>(surface, std::move(items));
    menu.show({20.0F, 20.0F});
    context_menu_test::draw_frame(surface);

    const Rect item_rect = menu.children().front()->layout().screen_rect();
    const ImVec2 item_position = {item_rect.min.x + 4.0F, item_rect.min.y + 4.0F};
    auto down = context_menu_test::pointer_event(EventType::PointerDown, item_position);
    auto up = context_menu_test::pointer_event(EventType::PointerUp, item_position);
    REQUIRE_FALSE(surface.dispatch(down));
    REQUIRE(surface.dispatch(up));

    REQUIRE(callback_called);
    REQUIRE(menu.is_open());
    REQUIRE(menu.visible());
}

TEST_CASE("context menu blocks and closes on outside pointer input", "[ContextMenuWidget]") {
    Runtime runtime;
    UI surface(runtime, {.size = {320.0F, 240.0F}});
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {320.0F, 240.0F};
    ui_test::ImGuiContext::build_fonts();

    int click_count = 0;
    auto& button = surface.root().add_child<ButtonWidget>(surface, "under menu", ImVec2{100.0F, 32.0F});
    button.set_placement({.anchor = Anchor::TopLeft, .origin = Origin::TopLeft, .offset = {8.0F, 8.0F}});
    button.on_click([&click_count] { ++click_count; });

    ContextMenuItems items;
    items.push_back({.label = "item"});
    auto& menu = surface.root().add_child<ContextMenuWidget>(surface, std::move(items));
    menu.show({160.0F, 120.0F});
    context_menu_test::draw_frame(surface);

    auto down = context_menu_test::pointer_event(EventType::PointerDown, {20.0F, 20.0F});
    auto up = context_menu_test::pointer_event(EventType::PointerUp, {20.0F, 20.0F});
    REQUIRE(surface.dispatch(down));
    REQUIRE(surface.dispatch(up));
    REQUIRE_FALSE(menu.is_open());
    REQUIRE(click_count == 0);

    context_menu_test::draw_frame(surface);
    REQUIRE_FALSE(menu.visible());
}

TEST_CASE("context menu opens a submenu when its parent is hovered", "[ContextMenuWidget]") {
    Runtime runtime;
    UI surface(runtime, {.size = {480.0F, 240.0F}});
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {480.0F, 240.0F};
    ui_test::ImGuiContext::build_fonts();

    ContextMenuItems children;
    children.push_back({.label = "child"});
    ContextMenuItems items;
    items.push_back({.label = "parent", .children = std::move(children)});
    auto& menu = surface.root().add_child<ContextMenuWidget>(surface, std::move(items));
    menu.show({20.0F, 20.0F});
    context_menu_test::draw_frame(surface);

    const Rect item_rect = menu.children().front()->layout().screen_rect();
    const ImVec2 item_position = {item_rect.min.x + 4.0F, item_rect.min.y + 4.0F};
    auto move = context_menu_test::pointer_event(EventType::PointerMove, item_position);
    surface.dispatch(move);
    ImGui::GetIO().MousePos = item_position;
    context_menu_test::draw_frame(surface);

    auto* submenu = dynamic_cast<ContextMenuWidget*>(menu.children()[1].get());
    REQUIRE(submenu != nullptr);
    REQUIRE(submenu->visible());
    REQUIRE(submenu->layout().screen_rect().min.x == Catch::Approx(item_rect.max.x + 6.0F));

    auto cross_gap = context_menu_test::pointer_event(
        EventType::PointerMove, {(item_rect.max.x + submenu->layout().screen_rect().min.x) * 0.5F, item_rect.min.y + 4.0F}
    );
    surface.dispatch(cross_gap);
    REQUIRE(submenu->is_open());

    auto enter_submenu = context_menu_test::pointer_event(
        EventType::PointerMove, {submenu->layout().screen_rect().min.x + 4.0F, submenu->layout().screen_rect().min.y + 4.0F}
    );
    surface.dispatch(enter_submenu);
    REQUIRE(submenu->is_open());

    auto leave_item = context_menu_test::pointer_event(
        EventType::PointerMove, {menu.layout().screen_rect().min.x + 1.0F, menu.layout().screen_rect().min.y + 1.0F}
    );
    surface.dispatch(leave_item);
    ImGui::GetIO().MousePos = leave_item.position;
    context_menu_test::draw_frame(surface, 0.81F);
    context_menu_test::draw_frame(surface, 0.0F);
    REQUIRE_FALSE(submenu->is_open());

    surface.dispatch(move);
    ImGui::GetIO().MousePos = item_position;
    context_menu_test::draw_frame(surface);
    REQUIRE(submenu->is_open());

    ImGui::GetIO().MousePos = {460.0F, 220.0F};
    context_menu_test::draw_frame(surface);
    REQUIRE_FALSE(submenu->is_open());
    REQUIRE_FALSE(menu.is_open());
}
