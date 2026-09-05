#include <ui/style/state.hpp>
#include <ui/runtime.hpp>
#include <ui/layout/container.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/layout/stack-container.hpp>
#include <ui/layout/virtual-layout.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/context-menu.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/number-input.hpp>
#include <ui/widgets/text.hpp>
#include <ui/widgets/text-input.hpp>
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
    Runtime runtime;
    UI surface(runtime);
    int integer = 0;
    double decimal = 0.0;
    NumberInputWidget integer_input(surface, integer);
    NumberInputWidget decimal_input(surface, decimal);

    REQUIRE(integer_input.set_value(42));
    REQUIRE(integer == 42);
    REQUIRE(decimal_input.set_value(1.25));
    REQUIRE(decimal == Catch::Approx(1.25));
}

TEST_CASE("checkbox measurement includes style padding and remeasures after changes", "[CheckboxWidget][layout][style]") {
    Runtime runtime;
    UI surface(runtime);
    bool value = false;
    StackContainer stack("checkbox-padding-stack");
    stack.set_size({fit(), fit()});
    stack.style().padding({});
    auto& checkbox = stack.add<CheckboxWidget>(surface, value, "custom checkbox");
    checkbox.configure_all_styles([](Style& style) { style.padding({10.0F, 6.0F}); });

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

TEST_CASE("checkbox input is limited to its box", "[CheckboxWidget][input][regression]") {
    Runtime runtime;
    UI surface(runtime);
    bool checked = false;
    auto& checkbox = surface.root().add<CheckboxWidget>(surface, checked, "checkbox");

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 180.0F};

    surface.begin_frame();
    surface.root().update(ImGui::GetIO().DeltaTime);
    surface.root().draw();
    surface.end_frame();

    const Rect widget_rect = checkbox.layout().visual_rect();
    const ImVec2 padding = checkbox.style().padding();
    const Rect frame_rect = Rect::from_position_size(
        {widget_rect.min.x + padding.x, widget_rect.min.y + padding.y}, checkbox.frame().layout().size()
    );
    REQUIRE(widget_rect.valid());
    REQUIRE(frame_rect.valid());
    REQUIRE(frame_rect.max.x < widget_rect.max.x);
    REQUIRE(checkbox.frame().layout().visual_rect().min.x == Catch::Approx(frame_rect.min.x));
    REQUIRE(checkbox.frame().layout().visual_rect().min.y == Catch::Approx(frame_rect.min.y));

    const ImVec2 frame_center = {
        (frame_rect.min.x + frame_rect.max.x) * 0.5F,
        (frame_rect.min.y + frame_rect.max.y) * 0.5F,
    };
    REQUIRE(surface.input_router().node_at(frame_center) == &checkbox);

    const ImVec2 label_position = {
        (frame_rect.max.x + widget_rect.max.x) * 0.5F,
        (widget_rect.min.y + widget_rect.max.y) * 0.5F,
    };
    REQUIRE(widget_rect.contains(label_position));
    REQUIRE_FALSE(frame_rect.contains(label_position));
    REQUIRE(surface.input_router().node_at(label_position) != &checkbox);
}

TEST_CASE("nested containers keep default padding empty and route checkbox clicks", "[container][input][regression]") {
    RuntimeConfig config;
    config.theme.content_padding = 20.0F;
    Runtime runtime(std::move(config));
    UI surface(runtime);
    bool checked = false;

    auto& page = surface.root().add<StackContainer>("page");
    page.set_size({px(320.0F), px(120.0F)});
    auto& section = page.add<Container>("section");
    auto& form = section.add<StackContainer>("form");
    auto& checkbox = form.add<CheckboxWidget>(surface, checked, "enabled");

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 180.0F};

    const auto draw_frame = [&surface] {
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

    const Rect widget_rect = checkbox.layout().visual_rect();
    const ImVec2 padding = checkbox.style().padding();
    const Rect rect = Rect::from_position_size(
        {widget_rect.min.x + padding.x, widget_rect.min.y + padding.y}, checkbox.frame().layout().size()
    );
    const ImVec2 position = {(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
    REQUIRE(surface.input_router().node_at(position) == &checkbox);

    UiEvent down = UiEvent::make(EventType::PointerDown);
    down.position = position;
    down.button = PointerButton::Left;
    surface.dispatch(down);

    UiEvent up = UiEvent::make(EventType::PointerUp);
    up.position = position;
    up.button = PointerButton::Left;
    surface.dispatch(up);
    REQUIRE(checked);
}

TEST_CASE("dropdown opens from a nested container without extending its parent", "[dropdown][container][regression]") {
    Runtime runtime;
    UI surface(runtime);
    std::string value = "light";

    auto& page = surface.root().add<StackContainer>("page");
    page.set_size({px(360.0F), px(200.0F)});
    auto& section = page.add<Container>("section");
    auto& form = section.add<StackContainer>("form");
    auto& dropdown =
        form.add<DropdownWidget>(surface, value, std::vector<DropdownOption>{{"light", "light"}, {"dark", "dark"}}, "theme");
    dropdown.set_size({px(180.0F), px(32.0F)});
    bool checked = false;
    auto& checkbox = surface.root().add<CheckboxWidget>(surface, checked, "enabled");
    checkbox.set_size({px(180.0F), px(32.0F)});

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 240.0F};

    const auto draw_frame = [&surface] {
        surface.begin_frame();
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        surface.end_frame();
    };

    draw_frame();
    const Rect trigger_rect = dropdown.trigger().layout().visual_rect();
    const ImVec2 trigger_center = {
        (trigger_rect.min.x + trigger_rect.max.x) * 0.5F,
        (trigger_rect.min.y + trigger_rect.max.y) * 0.5F,
    };

    UiEvent down = UiEvent::make(EventType::PointerDown);
    down.position = trigger_center;
    down.button = PointerButton::Left;
    surface.dispatch(down);

    UiEvent up = UiEvent::make(EventType::PointerUp);
    up.position = trigger_center;
    up.button = PointerButton::Left;
    surface.dispatch(up);

    draw_frame();
    REQUIRE(dropdown.is_open());
    const Rect body_rect = dropdown.body().layout().visual_rect();
    REQUIRE(body_rect.min.y >= trigger_rect.max.y);
    surface.input_router().target(checkbox, body_rect);

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
    Runtime runtime;
    UI surface(runtime);
    StackContainer stack("text-padding-stack");
    stack.set_size({fit(), fit()});
    stack.style().padding({});
    auto& text = stack.add<TextWidget>("padded text");
    text.configure_all_styles([](Style& style) {
        style.padding({5.0F, 3.0F}).background_color(ImColor{10, 20, 30, 255}).border(BORDER_ALL);
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
    Runtime runtime;
    UI surface(runtime);
    TextWidget clipped("this text exceeds the explicit width");
    TextWidget ellipsized("this text exceeds the explicit width");
    clipped.set_size({px(80.0F), px(24.0F)});
    ellipsized.set_size({px(80.0F), px(24.0F)}).set_overflow(TextOverflow::Ellipsis);

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();
    ImGui::GetIO().DisplaySize = {400.0F, 180.0F};

    surface.begin_frame();
    ImGui::Begin("text-overflow-test");
    clipped.draw();
    ellipsized.draw();
    ImGui::End();
    surface.end_frame();

    REQUIRE(clipped.overflow() == TextOverflow::Clip);
    REQUIRE(ellipsized.overflow() == TextOverflow::Ellipsis);
    REQUIRE(clipped.layout().size().x == Catch::Approx(80.0F));
    REQUIRE(ellipsized.layout().size().x == Catch::Approx(80.0F));
}

TEST_CASE("text line height scales multi-line text layout", "[TextWidget][layout][style]") {
    Runtime runtime;
    UI surface(runtime);
    TextWidget text("first line\nsecond line");
    text.configure_all_styles([](Style& style) { style.padding({}).line_height(1.5F); });

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
    TextWidget text("line");
    text.configure_style(StyleType::HOVER, [](Style& style) { style.line_height(2.0F, 1.0F); });

    text.set_interaction_style(true, false);
    text.update(0.5F);

    REQUIRE(text.style().line_height() == Catch::Approx(1.5F));
}

TEST_CASE("value widgets notify changes", "[Widget][change]") {
    Runtime runtime;
    UI surface(runtime);
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
    Runtime runtime;
    UI surface(runtime);
    std::string value;
    ResizableContainer parent("resizable");
    parent.set_size({px(180.0F), px(80.0F)});
    auto& input = parent.add<TextInputWidget>(surface, value, "input");

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
    parent.set_size({px(280.0F), px(80.0F)});
    draw_frame();
    const float expanded_width = input.layout().size().x;

    parent.set_size({px(140.0F), px(80.0F)});
    draw_frame();

    REQUIRE(expanded_width > initial_width);
    REQUIRE(input.layout().size().x < initial_width);
    REQUIRE(input.layout().size().y < parent.layout().size().y);
}

TEST_CASE("pointer block prevents hover and clicks on content controls", "[input][regression]") {
    Runtime runtime;
    UI surface(runtime);
    setup_demo(surface, "test");

    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {900.0F, 600.0F};
    ui_test::ImGuiContext::build_fonts();

    const auto draw_frame = [&surface](ImVec2 mouse_position, bool mouse_down = false) {
        ImGui::SetCurrentContext(surface.imgui_context());
        ImGui::GetIO().MousePos = mouse_position;
        ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] = mouse_down;
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
    auto* blocker_overlay = dynamic_cast<LayerContainer*>(blocker);
    REQUIRE(blocker != nullptr);
    REQUIRE(blocker_overlay != nullptr);
    REQUIRE(controls != nullptr);
    REQUIRE(dynamic_nodes != nullptr);
    REQUIRE_FALSE(controls->children().empty());

    blocker_overlay->set_visible(true);
    blocker_overlay->set_input_blocker();
    draw_frame({0.0F, 0.0F});

    auto* add_button = dynamic_cast<ButtonWidget*>(controls->children().front().get());
    REQUIRE(add_button != nullptr);
    const Rect add_button_rect = add_button->layout().visual_rect();
    const ImVec2 add_button_center = {
        (add_button_rect.min.x + add_button_rect.max.x) * 0.5F,
        (add_button_rect.min.y + add_button_rect.max.y) * 0.5F,
    };

    draw_frame(add_button_center, true);
    draw_frame(add_button_center, false);
    REQUIRE(add_button->style_type() == StyleType::DEFAULT);

    UiEvent down = UiEvent::make(EventType::PointerDown);
    down.position = add_button_center;
    down.button = PointerButton::Left;
    surface.dispatch(down);

    UiEvent up = UiEvent::make(EventType::PointerUp);
    up.position = add_button_center;
    up.button = PointerButton::Left;
    surface.dispatch(up);

    REQUIRE(dynamic_nodes->children().empty());
}

TEST_CASE("resizable dynamic list keeps its allocated box", "[ResizableContainer][layout][regression]") {
    Runtime runtime;
    UI surface(runtime);
    setup_demo(surface, "test");

    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {900.0F, 600.0F};
    ui_test::ImGuiContext::build_fonts();

    const auto draw_frame = [&surface] {
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({900.0F, 600.0F});
        ImGui::Begin("resizable-dynamic-test");
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        ImGui::End();
        surface.end_frame();
    };

    draw_frame();

    auto* dynamic_nodes = dynamic_cast<ResizableContainer*>(surface.root().find("dynamic-nodes"));
    auto* dynamic_section = surface.root().find("dynamic-section");
    auto* controls = surface.root().find("dynamic-node-controls");
    auto* dynamic_list = surface.root().find("dynamic-list");
    REQUIRE(dynamic_nodes != nullptr);
    REQUIRE(dynamic_section != nullptr);
    REQUIRE(controls != nullptr);
    REQUIRE(dynamic_list != nullptr);
    REQUIRE(dynamic_nodes->layout().visual_rect().valid());
    REQUIRE(dynamic_nodes->layout().size().y > 0.0F);

    const Rect section_rect = dynamic_section->layout().visual_rect();
    const Rect controls_rect = controls->layout().visual_rect();
    const Rect list_rect = dynamic_list->layout().visual_rect();
    REQUIRE(section_rect.valid());
    REQUIRE(controls_rect.valid());
    REQUIRE(list_rect.valid());
    REQUIRE(section_rect.min.x < controls_rect.min.x);
    REQUIRE(controls_rect.min.x < list_rect.min.x);
    REQUIRE(section_rect.min.y < controls_rect.min.y);

    auto* add_button = dynamic_cast<ButtonWidget*>(controls->children().front().get());
    REQUIRE(add_button != nullptr);
    UiEvent click = UiEvent::make(EventType::Click);
    click.button = PointerButton::Left;
    surface.input_router().dispatch(*add_button, click);
    REQUIRE(dynamic_nodes->children().size() == 1);

    draw_frame();

    REQUIRE(dynamic_nodes->children().size() == 1);
    REQUIRE(dynamic_nodes->layout().visual_rect().valid());
    REQUIRE(dynamic_nodes->children().front()->layout().visual_rect().valid());
}

TEST_CASE("pointer block rejects clicks on another overlay control", "[input][regression]") {
    Runtime runtime;
    UI surface(runtime);
    setup_demo(surface, "test");

    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {900.0F, 600.0F};
    ui_test::ImGuiContext::build_fonts();

    const auto draw_frame = [&surface](ImVec2 mouse_position) {
        ImGui::SetCurrentContext(surface.imgui_context());
        ImGui::GetIO().MousePos = mouse_position;
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
    auto* blocker_overlay = dynamic_cast<LayerContainer*>(blocker);
    REQUIRE(overlay != nullptr);
    REQUIRE(blocker != nullptr);
    REQUIRE(blocker_overlay != nullptr);
    REQUIRE(overlay->children().size() >= 2);

    auto* panel = overlay->children().front().get();
    auto* show_button = dynamic_cast<ButtonWidget*>(overlay->children().back().get());
    REQUIRE(panel != nullptr);
    REQUIRE(show_button != nullptr);
    REQUIRE_FALSE(panel->visible());

    blocker_overlay->set_visible(true);
    blocker_overlay->set_input_blocker();
    draw_frame({0.0F, 0.0F});

    const Rect button_rect = show_button->layout().visual_rect();
    const ImVec2 button_center = {
        (button_rect.min.x + button_rect.max.x) * 0.5F,
        (button_rect.min.y + button_rect.max.y) * 0.5F,
    };

    UiEvent down = UiEvent::make(EventType::PointerDown);
    down.position = button_center;
    down.button = PointerButton::Left;
    surface.dispatch(down);

    UiEvent up = UiEvent::make(EventType::PointerUp);
    up.position = button_center;
    up.button = PointerButton::Left;
    surface.dispatch(up);

    REQUIRE_FALSE(panel->visible());
}

TEST_CASE("dropdown opens after fading out and fades after selection", "[DropdownWidget][regression]") {
    Runtime runtime;
    UI surface(runtime);
    std::string value = "blue";
    int changes = 0;
    StackContainer stack("dropdown-stack");
    stack.set_input_router(&surface.input_router());
    stack.set_size({px(280.0F), px(180.0F)});
    stack.set_spacing(16.0F);
    auto& dropdown = stack.add<DropdownWidget>(
        surface, value, std::vector<DropdownOption>{{"blue", "blue"}, {"high contrast", "contrast"}}, "theme"
    );
    dropdown.set_size({px(180.0F), px(62.0F)});
    dropdown.set_label("theme");
    dropdown.on_change = [&changes] { ++changes; };
    auto& status = stack.add<TextWidget>("no clicks yet");

    ImGui::SetCurrentContext(surface.imgui_context());
    ui_test::ImGuiContext::build_fonts();

    bool previous_mouse_down = false;
    const auto draw_frame = [&surface, &stack, &dropdown, &previous_mouse_down](ImVec2 mouse_position, bool mouse_down) {
        ImGui::SetCurrentContext(surface.imgui_context());
        ImGui::GetIO().DisplaySize = {320.0F, 220.0F};
        ImGui::GetIO().MousePos = mouse_position;
        ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] = mouse_down;

        if (mouse_down && !previous_mouse_down) {
            UiEvent event = UiEvent::make(EventType::PointerDown);
            event.position = mouse_position;
            event.button = PointerButton::Left;
            surface.dispatch(event);
        } else if (!mouse_down && previous_mouse_down) {
            UiEvent event = UiEvent::make(EventType::PointerUp);
            event.position = mouse_position;
            event.button = PointerButton::Left;
            surface.dispatch(event);
        }
        previous_mouse_down = mouse_down;

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
    REQUIRE(status.layout().visual_rect().min.y >= dropdown.trigger().layout().visual_rect().max.y + stack.spacing());

    const Rect trigger_rect = dropdown.trigger().layout().visual_rect();
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

    const Rect body_rect = dropdown.body().layout().visual_rect();
    const float item_height = ImGui::GetTextLineHeight() + 8.0F;
    const ImVec2 second_option = {
        (body_rect.min.x + body_rect.max.x) * 0.5F,
        body_rect.min.y + item_height * 1.5F,
    };
    const ImU32 hover_background = static_cast<ImU32>(dropdown.trigger().style(StyleType::HOVER).background_color().value);
    const auto count_color = [](const ImDrawData* draw_data, ImU32 color) {
        int count = 0;
        if (draw_data == nullptr) {
            return count;
        }

        for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
            const ImDrawList* draw_list = draw_data->CmdLists[list_index];
            for (const ImDrawVert& vertex : draw_list->VtxBuffer) {
                count += vertex.col == color;
            }
        }

        return count;
    };
    const int unhovered_background_vertices = count_color(ImGui::GetDrawData(), hover_background);
    REQUIRE(draw_frame(second_option, false));
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);
    REQUIRE(count_color(ImGui::GetDrawData(), hover_background) > unhovered_background_vertices);
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
    RuntimeConfig config;
    config.theme.content_padding = 20.0F;
    config.theme.box_rounding = 8.0F;
    Runtime runtime(std::move(config));
    Font* font = runtime.fonts().add("regular", "fonts/regular.ttf");
    Font* semibold = runtime.fonts().add("semibold", "fonts/semibold.ttf");

    REQUIRE(runtime.theme().content_padding == 20.0F);
    REQUIRE(font == runtime.fonts().find("regular"));
    REQUIRE(semibold == runtime.fonts().find("semibold"));
    REQUIRE(semibold != font);
    REQUIRE(runtime.textures().find("default") == nullptr);

    Runtime other_runtime;

    REQUIRE(runtime.theme().box_rounding == 8.0F);
    REQUIRE(other_runtime.theme().content_padding == Theme{}.content_padding);
}

TEST_CASE("style transition duration uses seconds", "[VisualState][transition]") {
    VisualState state;

    state.style(StyleType::DEFAULT).color({0.0f, 0.0f, 0.0f, 1.0f});
    state.style(StyleType::HOVER).color({1.0f, 0.0f, 0.0f, 1.0f}, 0.2F);

    state.style(StyleType::HOVER).variables().set("rounding", FloatValue{10.0f, 0.2f});
    state.style(StyleType::DEFAULT).variables().set("rounding", FloatValue{0.0f, 0.0f});

    state.style(StyleType::HOVER).variables().set("enabled", BoolValue{true});
    state.style(StyleType::DEFAULT).variables().set("enabled", BoolValue{false});

    Vec2Value hover_offset;
    hover_offset.value = {5.0f, 5.0f};
    hover_offset.duration = 0.2f;
    state.style(StyleType::HOVER).variables().set("offset", hover_offset);

    Vec2Value default_offset;
    default_offset.value = {0.0f, 0.0f};
    state.style(StyleType::DEFAULT).variables().set("offset", default_offset);

    state.style(StyleType::HOVER).variables().set("count", IntValue{100, 0.2f});
    state.style(StyleType::DEFAULT).variables().set("count", IntValue{0, 0.0f});

    state.snap_to_style(StyleType::DEFAULT);
    state.set_style(StyleType::HOVER);

    state.update(0.1F);
    REQUIRE(state.style().color().get().x == Catch::Approx(0.5F));
    REQUIRE(state.style().variables().get<FloatValue>("rounding")->value == Catch::Approx(5.0F));
    REQUIRE(state.style().variables().get<Vec2Value>("offset")->value.x == Catch::Approx(2.5F));
    REQUIRE(state.style().variables().get<IntValue>("count")->value == 50);
    REQUIRE(state.style().variables().get<BoolValue>("enabled")->value);

    state.update(0.1F);

    SECTION("discrete type snaps immediately") {
        REQUIRE(state.style().variables().get<BoolValue>("enabled")->value);
    }

    SECTION("color converges") {
        REQUIRE(state.style().color().get().x == Catch::Approx(1.0f).margin(0.01f));
    }

    SECTION("float var converges") {
        REQUIRE(state.style().variables().get<FloatValue>("rounding")->value == Catch::Approx(10.0f).margin(0.1f));
    }

    SECTION("vec2 var converges") {
        REQUIRE(state.style().variables().get<Vec2Value>("offset")->value.x == Catch::Approx(5.0f).margin(0.1f));
    }

    SECTION("int var reaches exact target") {
        REQUIRE(state.style().variables().get<IntValue>("count")->value == 100);
    }
}

TEST_CASE("interaction style precedence is active focus hover default", "[VisualState][style]") {
    VisualState state;

    state.set_item_state(false, false, false);
    REQUIRE(state.style_type() == StyleType::DEFAULT);

    state.set_item_state(true, false, false);
    REQUIRE(state.style_type() == StyleType::HOVER);

    state.set_item_state(true, false, true);
    REQUIRE(state.style_type() == StyleType::FOCUS);

    state.set_item_state(true, true, true);
    REQUIRE(state.style_type() == StyleType::ACTIVE);
}

TEST_CASE("style cursor follows hovered nodes", "[Style][cursor]") {
    ui_test::ImGuiContext context({320.0F, 180.0F});
    InputRouter router;
    Widget widget("cursor-widget");
    widget.configure_style(StyleType::HOVER, [](Style& style) { style.cursor(ImGuiMouseCursor_Hand); });

    ImGui::NewFrame();
    router.begin_frame();
    router.target(widget, {{0.0F, 0.0F}, {40.0F, 20.0F}});

    UiEvent move = UiEvent::make(EventType::PointerMove);
    move.position = {10.0F, 10.0F};
    router.dispatch(move);
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);

    move.position = {100.0F, 100.0F};
    router.dispatch(move);
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow);
    ImGui::EndFrame();
}

TEST_CASE("border alpha fades out when a hover state is cleared", "[VisualState][transition]") {
    VisualState state;
    const ImColor accent = ImColor(233, 30, 115, 255);
    const ImColor hidden_accent = with_alpha(accent, 0.0F);

    state.configure_all_styles([&](Style& style) { style.border_color(hidden_accent, 0.2F); });
    state.configure_style(StyleType::HOVER, [&](Style& style) { style.border_color(accent); });

    state.set_style(StyleType::HOVER);
    state.update(0.2F);
    const float visible_alpha = state.style().border_color().get().w;

    state.set_style(StyleType::DEFAULT);
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
    VisualState state;
    state.set_opacity(0.0f);

    state.update(0.075F);
    REQUIRE(state.opacity() == Catch::Approx(0.5F));
    state.update(0.075F);
    REQUIRE(state.opacity() == Catch::Approx(0.0F));
    REQUIRE_FALSE(state.is_visible());
}

TEST_CASE("fade transitions control input independently from drawing", "[widget_state][opacity]") {
    VisualState state;
    state.update(1.0f / 60.0f);
    REQUIRE(state.accepts_input());

    state.fade_out();
    REQUIRE_FALSE(state.accepts_input());
    REQUIRE(state.is_visible());

    state.fade_in();
    REQUIRE(state.accepts_input());
}

TEST_CASE("widget input requires both node and visual state to accept input", "[Widget][input]") {
    Widget widget("widget");
    InputRouter router;
    router.target(widget, {{0.0F, 0.0F}, {10.0F, 10.0F}});

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
    class FontProbeWidget final : public Widget {
    public:
        FontProbeWidget() : Widget("font-probe") {}

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
    class StyleProbeWidget final : public Widget {
    public:
        StyleProbeWidget() : Widget("style-probe") {}

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

    widget.configure_all_styles([](Style& style) {
        style.color(ImColor{51, 102, 153, 255})
            .background_color(ImColor{26, 77, 128, 255})
            .padding({7.0F, 9.0F})
            .border(BORDER_ALL)
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
    class DrawProbeWidget final : public Widget {
    public:
        DrawProbeWidget() : Widget("draw-probe") {}

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
    widget.update(OPACITY_TRANSITION_DURATION);
    draw_frame();
    REQUIRE(widget.draws == 1);
}

TEST_CASE("styled widgets advance visual state during update", "[Widget][style]") {
    ui_test::ImGuiContext context({160.0F, 120.0F});

    Widget widget("widget");
    widget.configure_all_styles([](Style& style) { style.color(ImColor{0, 0, 0, 255}, 0.2F); });
    widget.configure_style(StyleType::HOVER, [](Style& style) { style.color(ImColor{255, 0, 0, 255}, 0.2F); });
    widget.set_visual_style(StyleType::HOVER);

    VisualState expected;
    expected.configure_all_styles([](Style& style) { style.color(ImColor{0, 0, 0, 255}, 0.2F); });
    expected.configure_style(StyleType::HOVER, [](Style& style) { style.color(ImColor{255, 0, 0, 255}, 0.2F); });
    expected.set_style(StyleType::HOVER);
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

    widget.configure_style(StyleType::FOCUS, [](Style& style) { style.border_radius(12.0F); });
    REQUIRE(widget.style(StyleType::FOCUS).border_radius() == Catch::Approx(12.0F));
}

TEST_CASE("custom update hooks cannot skip visual state advancement", "[Widget][style][regression]") {
    class UpdatingWidget final : public Widget {
    public:
        UpdatingWidget() : Widget("updating-widget") {}

        int updates = 0;

    private:
        void on_update(float) override {
            ++updates;
        }
    };

    UpdatingWidget widget;
    widget.configure_all_styles([](Style& style) { style.alpha(0.0F); });
    widget.configure_style(StyleType::HOVER, [](Style& style) { style.alpha(1.0F); });
    widget.set_visual_style(StyleType::HOVER);

    widget.update(1.0F / 60.0F);

    REQUIRE(widget.updates == 1);
    REQUIRE(widget.style().alpha() == Catch::Approx(1.0F));
}

TEST_CASE("fade in starts new visual states transparent", "[widget_state][opacity]") {
    VisualState state;

    state.fade_in();
    REQUIRE(state.opacity() == Catch::Approx(0.0F));

    state.update(1.0F / 60.0F);
    REQUIRE(state.opacity() > 0.0F);
    REQUIRE(state.opacity() < 1.0F);
}

TEST_CASE("a var introduced only on the target style still appears after transition", "[widget_state][regression]") {
    VisualState state;

    state.style(StyleType::DEFAULT).variables().set("line_alpha", FloatValue{0.0f, 0.15f});
    state.style(StyleType::HOVER).variables().set("line_alpha", FloatValue{1.0f, 0.15f});

    const FloatValue* default_alpha = state.style().variables().get<FloatValue>("line_alpha");
    REQUIRE(default_alpha != nullptr);
    REQUIRE(default_alpha->value == Catch::Approx(0.0F));

    state.set_style(StyleType::HOVER);
    state.update(1.0f / 60.0f); // first transition frame

    REQUIRE(state.style().variables().get<FloatValue>("line_alpha") != nullptr);
}

TEST_CASE("style variables stay local to their declared state", "[VisualState][variables]") {
    VisualState state;
    state.style(StyleType::HOVER).variables().set("line_width", FloatValue{2.0F, 0.15F});

    state.set_style(StyleType::HOVER);
    state.update(1.0F / 60.0F);
    REQUIRE(state.style().variables().get<FloatValue>("line_width") != nullptr);

    state.set_style(StyleType::ACTIVE);
    state.update(1.0F / 60.0F);
    REQUIRE(state.style().variables().get<FloatValue>("line_width") == nullptr);
}

TEST_CASE("editing the selected style updates its effective appearance") {
    VisualState state;
    state.style(StyleType::DEFAULT).color(ImColor{0, 0, 0, 255});
    state.style(StyleType::ACTIVE).color(ImColor{255, 0, 0, 255});
    state.snap_to_style(StyleType::DEFAULT);
    state.set_style(StyleType::ACTIVE);
    state.update(1.0F / 60.0F);

    state.style(StyleType::ACTIVE).color().set(ImColor{0, 255, 0, 255});

    const VisualState& const_state = state;
    REQUIRE(const_state.style().color().get().x == Catch::Approx(0.0F));
    REQUIRE(const_state.style().color().get().y == Catch::Approx(1.0F));
}

namespace context_menu_test {
    void draw_frame(UI& surface, float dt = 0.2F) {
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
    UI surface(runtime);
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {320.0F, 240.0F};
    ui_test::ImGuiContext::build_fonts();

    ContextMenuItems items;
    items.push_back({.label = "item"});
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));

    REQUIRE_FALSE(menu.visible());
    menu.show({300.0F, 220.0F});
    context_menu_test::draw_frame(surface);

    REQUIRE(menu.is_open());
    REQUIRE(menu.layout().visual_rect().min.x == Catch::Approx(136.0F));
    REQUIRE(menu.layout().visual_rect().min.y == Catch::Approx(204.0F));

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
    UI surface(runtime);
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
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));
    menu.show({20.0F, 20.0F});
    context_menu_test::draw_frame(surface);

    const Rect item_rect = menu.children().front()->layout().visual_rect();
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
    UI surface(runtime);
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {320.0F, 240.0F};
    ui_test::ImGuiContext::build_fonts();

    int click_count = 0;
    auto& button = surface.root().add<ButtonWidget>(surface, "under menu", LayoutSize{px(100.0F), px(32.0F)});
    button.set_layout({
        .size = {px(100.0F), px(32.0F)},
        .placement = {.offset = {8.0F, 8.0F}},
        .in_flow = false,
    });
    button.on_click([&click_count] { ++click_count; });

    ContextMenuItems items;
    items.push_back({.label = "item"});
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));
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
    UI surface(runtime);
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {480.0F, 240.0F};
    ui_test::ImGuiContext::build_fonts();

    ContextMenuItems children;
    children.push_back({.label = "child"});
    ContextMenuItems items;
    items.push_back({.label = "parent", .children = std::move(children)});
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));
    menu.show({20.0F, 20.0F});
    context_menu_test::draw_frame(surface);

    const Rect item_rect = menu.children().front()->layout().visual_rect();
    const ImVec2 item_position = {item_rect.min.x + 4.0F, item_rect.min.y + 4.0F};
    auto move = context_menu_test::pointer_event(EventType::PointerMove, item_position);
    surface.dispatch(move);
    ImGui::GetIO().MousePos = item_position;
    context_menu_test::draw_frame(surface);

    auto* submenu = dynamic_cast<ContextMenuWidget*>(menu.children()[1].get());
    REQUIRE(submenu != nullptr);
    REQUIRE(submenu->visible());
    REQUIRE(submenu->layout().visual_rect().min.x == Catch::Approx(item_rect.max.x + 6.0F));

    auto cross_gap = context_menu_test::pointer_event(
        EventType::PointerMove, {(item_rect.max.x + submenu->layout().visual_rect().min.x) * 0.5F, item_rect.min.y + 4.0F}
    );
    surface.dispatch(cross_gap);
    REQUIRE(submenu->is_open());

    auto enter_submenu = context_menu_test::pointer_event(
        EventType::PointerMove, {submenu->layout().visual_rect().min.x + 4.0F, submenu->layout().visual_rect().min.y + 4.0F}
    );
    surface.dispatch(enter_submenu);
    REQUIRE(submenu->is_open());

    auto leave_item = context_menu_test::pointer_event(
        EventType::PointerMove, {menu.layout().visual_rect().min.x + 1.0F, menu.layout().visual_rect().min.y + 1.0F}
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

TEST_CASE("demo virtual rows expand and collapse independently", "[layout][demo]") {
    Runtime runtime;
    UI surface(runtime);
    setup_demo(surface, "test");
    auto* list = dynamic_cast<VirtualLayout*>(surface.root().find("demo-virtual-list"));
    REQUIRE(list != nullptr);
    REQUIRE(list->item_count() == 100000);
    REQUIRE(list->children().empty());

    auto detached = list->parent()->remove(*list);
    ImGui::SetCurrentContext(surface.imgui_context());
    ImGui::GetIO().DisplaySize = {240.0F, 180.0F};
    ui_test::ImGuiContext::build_fonts();
    list->set_size({px(180.0F), px(100.0F)});
    const auto draw_frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({240.0F, 180.0F});
        ImGui::Begin("demo-virtual-layout-test", nullptr, ImGuiWindowFlags_NoSavedSettings);
        list->update(1.0F);
        list->draw();
        ImGui::End();
        ImGui::EndFrame();
    };
    draw_frame();
    draw_frame();
    REQUIRE(list->children().size() < 10);
    auto* first = list->find("virtual-row-0");
    auto* second = list->find("virtual-row-1");
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    UiEvent click = UiEvent::make(EventType::Click);
    surface.input_router().dispatch(*first, click);
    REQUIRE(list->extra_offset(0) == 64.0F);
    surface.input_router().dispatch(*second, click);
    REQUIRE(list->extra_offset(1) == 64.0F);
    surface.input_router().dispatch(*first, click);
    REQUIRE(list->extra_offset(0) == 0.0F);
    REQUIRE(list->extra_offset(1) == 64.0F);
}
