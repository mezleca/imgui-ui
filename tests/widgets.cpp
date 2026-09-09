#include <ui/animation.hpp>
#include <ui/style/state.hpp>
#include <ui/runtime.hpp>
#include <ui/diagnostics/debugger.hpp>
#include <ui/layout/container.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/layout/stack-container.hpp>
#include <ui/layout/virtual-layout.hpp>
#include <ui/resources/texture-registry.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/color-picker.hpp>
#include <ui/widgets/context-menu.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/image.hpp>
#include <ui/widgets/number-input.hpp>
#include <ui/widgets/text.hpp>
#include <ui/widgets/text-input.hpp>
#include "../examples/demo.hpp"
#include "imgui-context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>
#include <limits>
#include <string>
#include <vector>

using namespace ui;

TEST_CASE("checkbox input is limited to its box", "[CheckboxWidget][input][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    bool checked = false;
    auto& checkbox = surface.root().add<CheckboxWidget>(surface, checked, "checkbox");

    ui_test::prepare_surface(surface, {400.0F, 180.0F});

    ui_test::draw_surface(surface);

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

    const ImVec2 frame_center = ui_test::center(frame_rect);
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
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    bool checked = false;

    auto& page = surface.root().add<StackContainer>("page");
    page.set_size({px(320.0F), px(120.0F)});
    auto& section = page.add<Container>("section");
    auto& form = section.add<StackContainer>("form");
    auto& checkbox = form.add<CheckboxWidget>(surface, checked, "enabled");

    ui_test::prepare_surface(surface, {400.0F, 180.0F});

    ui_test::draw_surface(surface);

    const Rect widget_rect = checkbox.layout().visual_rect();
    const ImVec2 padding = checkbox.style().padding();
    const Rect rect = Rect::from_position_size(
        {widget_rect.min.x + padding.x, widget_rect.min.y + padding.y}, checkbox.frame().layout().size()
    );
    const ImVec2 position = ui_test::center(rect);
    REQUIRE(surface.input_router().node_at(position) == &checkbox);

    UiEvent down = ui_test::pointer_event(EventType::PointerDown, position);
    surface.dispatch(down);

    UiEvent up = ui_test::pointer_event(EventType::PointerUp, position);
    surface.dispatch(up);
    REQUIRE(checked);
}

TEST_CASE("buttons flash their active background after click", "[ButtonWidget][animation]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    auto& button = surface.root().add<ButtonWidget>(surface, "button", LayoutSize{px(120.0F), px(36.0F)});

    ui_test::prepare_surface(surface, {400.0F, 180.0F});
    ui_test::draw_surface(surface);

    const ImVec2 position = ui_test::center(button.layout().visual_rect());
    UiEvent down = ui_test::pointer_event(EventType::PointerDown, position);
    surface.dispatch(down);

    UiEvent up = ui_test::pointer_event(EventType::PointerUp, position);
    surface.dispatch(up);
    button.update(0.0F);

    const ImColor active_background = button.style(StyleType::ACTIVE).background_color().value;
    REQUIRE(button.computed_style().background_color().value.Value.x == Catch::Approx(active_background.Value.x));
}

TEST_CASE("image fit preserves the texture aspect ratio", "[ImageWidget][fit]") {
    class ProbeTexture final : public Texture {
    public:
        ImVec2 size() const override {
            return {200.0F, 100.0F};
        }

        ImTextureID get(ImVec2 size) override {
            requested_size = size;
            return {};
        }

        void release_context(ImGuiContext*) override {}

        ImVec2 requested_size{};
    };

    ui_test::ImGuiContext context({240.0F, 180.0F});
    const auto draw = [](ImageFit fit) {
        ProbeTexture texture;
        ImageWidget image(&texture);
        image.set_size({px(100.0F), px(100.0F)});
        image.set_fit(fit);

        ImGui::NewFrame();
        ImGui::Begin("image-fit-test");
        image.draw();
        ImGui::End();
        ImGui::EndFrame();
        return texture.requested_size;
    };

    const ImVec2 contain_size = draw(ImageFit::Contain);
    REQUIRE(contain_size.x == Catch::Approx(100.0F));
    REQUIRE(contain_size.y == Catch::Approx(50.0F));

    const ImVec2 cover_size = draw(ImageFit::Cover);
    REQUIRE(cover_size.x == Catch::Approx(200.0F));
    REQUIRE(cover_size.y == Catch::Approx(100.0F));
}

TEST_CASE("dropdown opens from a nested container without extending its parent", "[dropdown][container][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
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

    ui_test::prepare_surface(surface, {400.0F, 240.0F});

    ui_test::draw_surface(surface);
    const Rect trigger_rect = dropdown.trigger().layout().visual_rect();
    const ImVec2 trigger_center = ui_test::center(trigger_rect);

    UiEvent down = ui_test::pointer_event(EventType::PointerDown, trigger_center);
    surface.dispatch(down);

    UiEvent up = ui_test::pointer_event(EventType::PointerUp, trigger_center);
    surface.dispatch(up);

    ui_test::draw_surface(surface);
    REQUIRE(dropdown.is_open());
    const Rect body_rect = dropdown.body().layout().visual_rect();
    REQUIRE(body_rect.min.y >= trigger_rect.max.y);
    surface.input_router().register_target(checkbox, body_rect);

    const ImVec2 option_position = ui_test::center(body_rect);
    REQUIRE(surface.input_router().node_at(option_position) == &checkbox);

    down.position = option_position;
    surface.dispatch(down);
    up.position = option_position;
    surface.dispatch(up);
    REQUIRE_FALSE(checked);
}

TEST_CASE("color picker opens outside its parent and blocks content input", "[color-picker][container][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    ImColor color = {0.26F, 0.59F, 0.98F, 1.0F};
    bool checked = false;

    auto& page = surface.root().add<StackContainer>("page");
    page.set_size({px(360.0F), px(200.0F)});
    auto& section = page.add<Container>("section");
    auto& picker = section.add<ColorPickerWidget>(surface, color, "color");
    auto& checkbox = surface.root().add<CheckboxWidget>(surface, checked, "enabled");

    ui_test::prepare_surface(surface, {400.0F, 300.0F});
    ui_test::draw_surface(surface);

    const ImVec2 preview_center = ui_test::center(picker.layout().visual_rect());
    UiEvent down = ui_test::pointer_event(EventType::PointerDown, preview_center);
    UiEvent up = ui_test::pointer_event(EventType::PointerUp, preview_center);
    surface.dispatch(down);
    surface.dispatch(up);

    ui_test::draw_surface(surface);
    REQUIRE(picker.is_open());
    const Rect popup_rect = picker.popup().layout().visual_rect();
    REQUIRE(popup_rect.valid());
    REQUIRE(popup_rect.min.y >= picker.layout().visual_rect().max.y);

    surface.input_router().register_target(checkbox, popup_rect);
    const ImVec2 popup_center = ui_test::center(popup_rect);
    down.position = popup_center;
    up.position = popup_center;
    surface.dispatch(down);
    surface.dispatch(up);
    REQUIRE_FALSE(checked);

    const ImVec2 outside_position = {popup_rect.max.x + 2.0F, popup_rect.min.y};
    down.position = outside_position;
    up.position = outside_position;
    REQUIRE(surface.dispatch(down));
    surface.dispatch(up);
    REQUIRE_FALSE(picker.is_open());

    picker.open();
    ui_test::draw_surface(surface);
    down.position = preview_center;
    up.position = preview_center;
    REQUIRE(surface.dispatch(down));
    surface.dispatch(up);
    REQUIRE_FALSE(picker.is_open());
}

TEST_CASE("color pickers do not replace each other", "[color-picker][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    ImColor first_color = {0.26F, 0.59F, 0.98F, 1.0F};
    ImColor second_color = {0.98F, 0.59F, 0.26F, 1.0F};
    auto& first = surface.root().add<ColorPickerWidget>(surface, first_color, "first");
    auto& second = surface.root().add<ColorPickerWidget>(surface, second_color, "second");

    ui_test::prepare_surface(surface, {640.0F, 480.0F});
    first.open();
    second.open();
    ui_test::draw_surface(surface);

    REQUIRE(first.is_open());
    REQUIRE(second.is_open());
}

TEST_CASE("dropdown options use framework input and select their value", "[DropdownWidget][input][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    std::string value = "light";
    int changes = 0;
    auto& dropdown = surface.root().add<DropdownWidget>(
        surface, value, std::vector<DropdownOption>{{"light", "light"}, {"dark", "dark"}}, "theme"
    );
    dropdown.set_size({px(180.0F), px(32.0F)});
    dropdown.set_on_change([&changes] { ++changes; });

    ui_test::prepare_surface(surface, {400.0F, 240.0F});

    ui_test::draw_surface(surface);
    const Rect trigger_rect = dropdown.trigger().layout().visual_rect();
    const ImVec2 trigger_center = ui_test::center(trigger_rect);

    UiEvent down = ui_test::pointer_event(EventType::PointerDown, trigger_center);
    surface.dispatch(down);
    REQUIRE(down.native_input_blocked);

    UiEvent up = ui_test::pointer_event(EventType::PointerUp, trigger_center);
    surface.dispatch(up);
    REQUIRE(up.native_input_blocked);
    ui_test::draw_surface(surface);

    const Rect body_rect = dropdown.body().layout().visual_rect();
    const float item_height = body_rect.size().y * 0.5F;
    const ImVec2 option_position = {
        ui_test::center(body_rect).x,
        body_rect.min.y + item_height * 1.5F,
    };
    const ui::Node* option = surface.input_router().node_at(option_position);
    REQUIRE(option != nullptr);
    REQUIRE(option->type_name() == "DropdownOption");

    UiEvent move = ui_test::pointer_event(EventType::PointerMove, option_position);
    surface.dispatch(move);
    REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);

    down.position = option_position;
    surface.dispatch(down);
    up.position = option_position;
    surface.dispatch(up);
    REQUIRE(value == "dark");
    REQUIRE_FALSE(dropdown.is_open());

    ui_test::draw_surface(surface);
    REQUIRE(changes == 1);
}

TEST_CASE("demo dropdown rows expose their complete visual hit boxes", "[DropdownWidget][input][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    setup_demo(surface, "test");

    ui_test::prepare_surface(surface, {900.0F, 1200.0F});

    auto* dropdown = dynamic_cast<DropdownWidget*>(surface.root().find("theme"));
    REQUIRE(dropdown != nullptr);

    const auto draw_frame = [&surface] {
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({900.0F, 1200.0F});
        ImGui::Begin("demo-dropdown-hover-test");
        surface.update(ImGui::GetIO().DeltaTime);
        surface.draw();
        ImGui::End();
        surface.end_frame();
    };

    draw_frame();
    const Rect trigger_rect = dropdown->trigger().layout().visual_rect();
    const ImVec2 trigger_center = ui_test::center(trigger_rect);

    UiEvent down = ui_test::pointer_event(EventType::PointerDown, trigger_center);
    surface.dispatch(down);

    UiEvent up = ui_test::pointer_event(EventType::PointerUp, trigger_center);
    surface.dispatch(up);
    draw_frame();

    for (const auto& child : dropdown->body().children()) {
        const Rect option_rect = child->layout().visual_rect();
        const ImVec2 option_center = ui_test::center(option_rect);

        UiEvent move = ui_test::pointer_event(EventType::PointerMove, option_center);
        surface.dispatch(move);
        REQUIRE(child->input_state().hovered);
        REQUIRE(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);
    }
}

TEST_CASE("inline layer centers inside content beside the debugger", "[LayerContainer][Debugger][layout][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend(), .enable_debugger = true});
    setup_demo(surface, "test");
    surface.debugger()->set_open(true);

    ui_test::prepare_surface(surface, {900.0F, 600.0F});

    auto* layer = dynamic_cast<LayerContainer*>(surface.root().find("##modal-layer"));
    auto* modal = surface.root().find("demo-modal");
    REQUIRE(layer != nullptr);
    REQUIRE(modal != nullptr);
    layer->set_visible(true);
    modal->set_visible(true);

    ui_test::draw_surface(surface);

    const Rect content_rect = surface.root().layout().visual_rect();
    const Rect modal_rect = modal->layout().visual_rect();
    REQUIRE(content_rect.valid());
    REQUIRE(modal_rect.valid());

    const ImVec2 content_center = ui_test::center(content_rect);
    const ImVec2 modal_center = ui_test::center(modal_rect);
    REQUIRE(modal_center.x == Catch::Approx(content_center.x));
    REQUIRE(modal_center.y == Catch::Approx(content_center.y));
}

TEST_CASE("text measurement and drawing include style padding", "[TextWidget][layout][style]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    StackContainer stack("text-padding-stack");
    stack.set_size({fit(), fit()});
    stack.style().padding({});
    auto& text = stack.add<TextWidget>("padded text");
    text.configure_all_styles([](Style& style) {
        style.padding({5.0F, 3.0F}).background_color(ImColor{10, 20, 30, 255}).border(BORDER_ALL);
    });

    ui_test::prepare_surface(surface, {400.0F, 180.0F});

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

TEST_CASE("animated padding updates text measurement", "[TextWidget][layout][animation]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    TextWidget text("animated text");
    text.configure_all_styles([](Style& style) { style.padding({}); });

    ui_test::prepare_surface(surface, {400.0F, 180.0F});

    surface.begin_frame();
    const ImVec2 raw_size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFont()->LegacySize, FLT_MAX, 0.0F, "animated text");
    text.animate().padding_x(10.0F, {0.1F, easing::linear});
    text.update(0.05F);
    ImGui::Begin("animated-text-padding-test");
    text.draw();
    ImGui::End();
    surface.end_frame();

    REQUIRE(text.layout().size().x == Catch::Approx(raw_size.x + 10.0F));
}

TEST_CASE("text line height scales multi-line text layout", "[TextWidget][layout][style]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    TextWidget text("first line\nsecond line");
    text.configure_all_styles([](Style& style) { style.padding({}).line_height(1.5F); });

    ui_test::prepare_surface(surface, {400.0F, 180.0F});

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
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    bool checked = false;
    int number = 1;
    std::string choice = "one";
    std::string text = "before";
    int changes = 0;

    CheckboxWidget checkbox(surface, checked, "checked");
    NumberInputWidget input(surface, number);
    DropdownWidget dropdown(surface, choice, {{"one", "one"}, {"two", "two"}});
    TextInputWidget text_input(surface, text);

    checkbox.set_on_change([&changes] { ++changes; });
    input.set_on_change([&changes] { ++changes; });
    dropdown.set_on_change([&changes] { ++changes; });
    text_input.set_on_change([&changes] { ++changes; });

    REQUIRE(checkbox.set_checked(true));
    REQUIRE(changes == 1);
    REQUIRE(input.set_value(2));
    REQUIRE(changes == 2);
    REQUIRE(dropdown.select_value("two"));
    REQUIRE(changes == 3);
    REQUIRE(text_input.set_value("after"));
    REQUIRE(changes == 4);

    REQUIRE_FALSE(checkbox.set_checked(true));
    REQUIRE_FALSE(input.set_value(2));
    REQUIRE_FALSE(dropdown.select_value("two"));
    REQUIRE_FALSE(text_input.set_value("after"));
    REQUIRE(changes == 4);
}

TEST_CASE("text input follows a resized parent width", "[TextInputWidget][layout][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    std::string value;
    ResizableContainer parent("resizable");
    parent.set_size({px(180.0F), px(80.0F)});
    auto& input = parent.add<TextInputWidget>(surface, value, "input");

    ui_test::prepare_surface(surface, {400.0F, 180.0F});

    const auto draw_frame = [&surface, &parent] {
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
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    setup_demo(surface, "test");

    ui_test::prepare_surface(surface, {900.0F, 600.0F});

    const auto draw_frame = [&surface](ImVec2 mouse_position, bool mouse_down = false) {
        ImGui::GetIO().MousePos = mouse_position;
        ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] = mouse_down;
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({900.0F, 600.0F});
        ImGui::Begin("demo-input-test");
        surface.update(ImGui::GetIO().DeltaTime);
        surface.draw();
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
    blocker_overlay->set_input_mode(InputMode::Blocker);
    draw_frame({0.0F, 0.0F});

    auto* add_button = dynamic_cast<ButtonWidget*>(controls->children().front().get());
    REQUIRE(add_button != nullptr);
    const Rect add_button_rect = add_button->layout().visual_rect();
    const ImVec2 add_button_center = ui_test::center(add_button_rect);

    draw_frame(add_button_center, true);
    draw_frame(add_button_center, false);
    REQUIRE(add_button->style_type() == StyleType::DEFAULT);

    UiEvent down = ui_test::pointer_event(EventType::PointerDown, add_button_center);
    surface.dispatch(down);

    UiEvent up = ui_test::pointer_event(EventType::PointerUp, add_button_center);
    surface.dispatch(up);

    REQUIRE(dynamic_nodes->children().empty());
}

TEST_CASE("resizable dynamic list keeps its allocated box", "[ResizableContainer][layout][regression]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    setup_demo(surface, "test");

    ui_test::prepare_surface(surface, {900.0F, 600.0F});

    const auto draw_frame = [&surface] {
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({900.0F, 600.0F});
        ImGui::Begin("resizable-dynamic-test");
        surface.update(ImGui::GetIO().DeltaTime);
        surface.draw();
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
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    setup_demo(surface, "test");

    ui_test::prepare_surface(surface, {900.0F, 600.0F});

    const auto draw_frame = [&surface](ImVec2 mouse_position) {
        ImGui::GetIO().MousePos = mouse_position;
        surface.begin_frame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({900.0F, 600.0F});
        ImGui::Begin("demo-overlay-input-test");
        surface.update(ImGui::GetIO().DeltaTime);
        surface.draw();
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
    blocker_overlay->set_input_mode(InputMode::Blocker);
    draw_frame({0.0F, 0.0F});

    const Rect button_rect = show_button->layout().visual_rect();
    const ImVec2 button_center = ui_test::center(button_rect);

    UiEvent down = ui_test::pointer_event(EventType::PointerDown, button_center);
    surface.dispatch(down);

    UiEvent up = ui_test::pointer_event(EventType::PointerUp, button_center);
    surface.dispatch(up);

    REQUIRE_FALSE(panel->visible());
}

TEST_CASE("style transitions apply the configured easing function", "[VisualState][transition]") {
    VisualState state;
    state.style(StyleType::DEFAULT).color({0.0F, 0.0F, 0.0F, 1.0F});
    state.style(StyleType::HOVER).color({1.0F, 0.0F, 0.0F, 1.0F}, {0.2F, easing::out_quad});

    state.snap_to_style(StyleType::DEFAULT);
    state.set_style(StyleType::HOVER);
    state.update(0.1F);

    REQUIRE(state.style().color().get().x == Catch::Approx(0.75F));
}

TEST_CASE("style transitions remain active until their duration ends", "[VisualState][transition]") {
    VisualState state;
    state.style(StyleType::HOVER).line_height(2.0F, {0.5F, easing::out_cubic});

    state.snap_to_style(StyleType::DEFAULT);
    state.set_style(StyleType::HOVER);
    state.update(0.45F);

    REQUIRE(state.style().line_height() < 2.0F);
    REQUIRE(state.transitioning());

    state.update(0.05F);

    REQUIRE(state.style().line_height() == Catch::Approx(2.0F));
    REQUIRE_FALSE(state.transitioning());
}

TEST_CASE("animation sequences run parallel steps before advancing", "[VisualState][animation]") {
    VisualState state;
    state.configure_all_styles([](Style& style) {
        style.padding({2.0F, 4.0F});
        style.background_color(ImColor{0.0F, 0.0F, 0.0F, 1.0F});
    });

    state.animate()
        .padding_y(12.0F, {0.1F, easing::linear})
        .background_color(ImColor{1.0F, 0.0F, 0.0F, 1.0F}, {0.1F, easing::linear})
        .then(0.05F)
        .padding_x(20.0F, {0.1F, easing::linear});

    state.update(0.05F);

    REQUIRE(state.computed_style().padding().x == Catch::Approx(2.0F));
    REQUIRE(state.computed_style().padding().y == Catch::Approx(8.0F));
    REQUIRE(state.computed_style().background_color().value.Value.x == Catch::Approx(0.5F));

    state.update(0.1F);

    REQUIRE(state.computed_style().padding().x == Catch::Approx(2.0F));
    REQUIRE(state.computed_style().padding().y == Catch::Approx(12.0F));
    REQUIRE(state.computed_style().background_color().value.Value.x == Catch::Approx(1.0F));
    REQUIRE(state.transitioning());

    state.update(0.05F);

    REQUIRE(state.computed_style().padding().x == Catch::Approx(11.0F));
    REQUIRE(state.transitioning());

    state.update(0.05F);

    REQUIRE(state.computed_style().padding().x == Catch::Approx(20.0F));
    REQUIRE_FALSE(state.transitioning());

    state.cancel_animations();

    REQUIRE(state.computed_style().padding().x == Catch::Approx(2.0F));
}

TEST_CASE("released animation properties return to the active style", "[VisualState][animation]") {
    VisualState state;
    const ImColor default_color{0.0F, 0.0F, 0.0F, 1.0F};
    const ImColor hover_color{0.0F, 1.0F, 0.0F, 1.0F};
    const ImColor flash_color{1.0F, 0.0F, 0.0F, 1.0F};
    state.style(StyleType::DEFAULT).background_color(default_color);
    state.style(StyleType::HOVER).background_color(hover_color);
    state.animate().background_color(flash_color).then(0.05F).release_background_color({0.1F, easing::linear});

    state.update(0.0F);
    REQUIRE(state.computed_style().background_color().value.Value.x == Catch::Approx(1.0F));

    state.set_style(StyleType::HOVER);
    state.update(0.1F);

    REQUIRE(state.computed_style().background_color().value.Value.x == Catch::Approx(0.5F));
    REQUIRE(state.computed_style().background_color().value.Value.y == Catch::Approx(0.5F));

    state.update(0.05F);

    REQUIRE(state.computed_style().background_color().value.Value.x == Catch::Approx(0.0F));
    REQUIRE(state.computed_style().background_color().value.Value.y == Catch::Approx(1.0F));
    REQUIRE_FALSE(state.transitioning());
}

TEST_CASE("interrupted animations release from their displayed value", "[VisualState][animation]") {
    VisualState state;
    state.configure_all_styles([](Style& style) { style.padding({2.0F, 0.0F}); });
    state.animate().padding_x(20.0F, {0.2F, easing::linear});
    state.update(0.1F);

    state.animate().release_padding_x({0.1F, easing::linear});
    state.update(0.05F);

    REQUIRE(state.computed_style().padding().x == Catch::Approx(6.5F));

    state.update(0.05F);

    REQUIRE(state.computed_style().padding().x == Catch::Approx(2.0F));
    REQUIRE_FALSE(state.transitioning());
}

TEST_CASE("animation sequences transform style presentation values", "[VisualState][animation][transform]") {
    VisualState state;
    state.animate().rotation(40.0F, {0.2F, easing::linear}).scale({1.4F, 0.8F}, {0.2F, easing::linear});
    state.update(0.1F);

    REQUIRE(state.computed_style().rotation() == Catch::Approx(20.0F));
    REQUIRE(state.computed_style().scale().x == Catch::Approx(1.2F));
    REQUIRE(state.computed_style().scale().y == Catch::Approx(0.9F));

    state.animate().rotation_by(30.0F, {0.1F, easing::linear});
    state.update(0.1F);

    REQUIRE(state.computed_style().rotation() == Catch::Approx(50.0F));

    state.animate().release_all({0.1F, easing::linear});
    state.update(0.1F);

    REQUIRE(state.computed_style().rotation() == Catch::Approx(0.0F));
    REQUIRE(state.computed_style().scale().x == Catch::Approx(1.0F));
    REQUIRE(state.computed_style().scale().y == Catch::Approx(1.0F));
}

TEST_CASE("animation sequence callbacks run after their timeline", "[VisualState][animation]") {
    VisualState state;
    bool ended = false;
    state.animate().rotation(90.0F, {0.1F, easing::linear}).then(0.1F).end([&ended] { ended = true; });

    state.update(0.1F);
    REQUIRE_FALSE(ended);

    state.update(0.1F);
    REQUIRE(ended);
}

TEST_CASE("animation sequence steps continue from the preceding track", "[VisualState][animation]") {
    VisualState state;
    state.animate().scale(2.0F, {0.1F, easing::linear}).then().scale(3.0F, {0.1F, easing::linear});

    state.update(0.1F);
    REQUIRE(state.computed_style().scale().x == Catch::Approx(2.0F));

    state.update(0.05F);
    REQUIRE(state.computed_style().scale().x == Catch::Approx(2.5F));
}

TEST_CASE("animator sequences update arbitrary references", "[Animator]") {
    Animator animator;
    float line_length = 0.0F;
    bool ended = false;

    animator.animate()
        .to(line_length, 10.0F, {0.2F, easing::linear})
        .then()
        .by(line_length, -4.0F, {0.1F, easing::linear})
        .end([&ended] { ended = true; });

    animator.update(0.1F);
    REQUIRE(line_length == Catch::Approx(5.0F));

    animator.update(0.15F);
    REQUIRE(line_length == Catch::Approx(8.0F));
    REQUIRE_FALSE(ended);

    animator.update(0.05F);
    REQUIRE(line_length == Catch::Approx(6.0F));
    REQUIRE(ended);
    REQUIRE_FALSE(animator.transitioning());
}

TEST_CASE("styled nodes advance their generic animator", "[Animator][StyledNode]") {
    TextWidget text{"animated-node"};
    float reveal = 0.0F;

    text.animator().animate().to(reveal, 1.0F, {0.2F, easing::linear});
    text.update(0.1F);

    REQUIRE(reveal == Catch::Approx(0.5F));
}

TEST_CASE("styled nodes rotate their generated vertices without changing layout", "[Widget][style][transform]") {
    class TransformProbeWidget final : public Widget {
    public:
        TransformProbeWidget() : Widget("transform-probe") {}

    private:
        bool paint() override {
            const Rect rect = Rect::from_position_size(ImGui::GetCursorScreenPos(), layout().size());
            ImGui::Dummy(rect.size());
            ImGui::GetWindowDrawList()->AddRectFilled(rect.min, rect.max, IM_COL32_WHITE);
            return true;
        }
    };

    ui_test::ImGuiContext context({240.0F, 160.0F});
    TransformProbeWidget widget;
    widget.set_size({px(40.0F), px(20.0F)});
    widget.configure_all_styles([](Style& style) { style.rotation(90.0F); });

    ImGui::NewFrame();
    ImGui::Begin("styled-transform-test");
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const int first_vertex = draw_list->VtxBuffer.Size;
    widget.draw();

    const Rect layout_rect = widget.layout().visual_rect();
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    for (int index = first_vertex; index < draw_list->VtxBuffer.Size; ++index) {
        const ImVec2 position = draw_list->VtxBuffer[index].pos;
        min_x = std::min(min_x, position.x);
        max_x = std::max(max_x, position.x);
        min_y = std::min(min_y, position.y);
        max_y = std::max(max_y, position.y);
    }

    ImGui::End();
    ImGui::EndFrame();

    REQUIRE(layout_rect.size().x == Catch::Approx(40.0F));
    REQUIRE(layout_rect.size().y == Catch::Approx(20.0F));
    REQUIRE(max_x - min_x == Catch::Approx(20.0F));
    REQUIRE(max_y - min_y == Catch::Approx(40.0F));
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
    router.register_target(widget, {{0.0F, 0.0F}, {40.0F, 20.0F}});

    UiEvent move = ui_test::pointer_event(EventType::PointerMove, {10.0F, 10.0F});
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
    router.register_target(widget, {{0.0F, 0.0F}, {10.0F, 10.0F}});

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

TEST_CASE("context menu clamps its position and fades out", "[ContextMenuWidget]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    ui_test::prepare_surface(surface, {320.0F, 240.0F});

    ContextMenuItems items;
    items.push_back({.label = "item"});
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));

    REQUIRE_FALSE(menu.visible());
    menu.open_at({300.0F, 220.0F});
    ui_test::draw_surface(surface, 0.2F);

    REQUIRE(menu.is_open());
    REQUIRE(menu.layout().visual_rect().min.x == Catch::Approx(136.0F));
    REQUIRE(menu.layout().visual_rect().min.y == Catch::Approx(212.0F));

    ImGui::GetIO().MousePos = {140.0F, 216.0F};
    ui_test::draw_surface(surface, 0.01F);

    ImGui::GetIO().MousePos = {0.0F, 0.0F};
    ui_test::draw_surface(surface, 0.78F);
    REQUIRE(menu.is_open());

    ui_test::draw_surface(surface, 0.02F);
    REQUIRE_FALSE(menu.is_open());
    ui_test::draw_surface(surface, 0.2F);
    REQUIRE_FALSE(menu.visible());
}

TEST_CASE("context menu item callbacks can keep the root menu open", "[ContextMenuWidget]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    ui_test::prepare_surface(surface, {320.0F, 240.0F});

    bool callback_called = false;
    ContextMenuItems items;
    items.push_back({
        .label = "keep open",
        .callback = [&callback_called](ContextMenuWidget& menu) {
            callback_called = true;
            menu.cancel_close();
        },
    });
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));
    menu.open_at({20.0F, 20.0F});
    ui_test::draw_surface(surface, 0.2F);

    const Rect item_rect = menu.children().front()->layout().visual_rect();
    const ImVec2 item_position = {item_rect.min.x + 4.0F, item_rect.min.y + 4.0F};
    auto down = ui_test::pointer_event(EventType::PointerDown, item_position);
    auto up = ui_test::pointer_event(EventType::PointerUp, item_position);
    REQUIRE_FALSE(surface.dispatch(down));
    REQUIRE(surface.dispatch(up));

    REQUIRE(callback_called);
    REQUIRE(menu.is_open());
    REQUIRE(menu.visible());
}

TEST_CASE("context menu blocks and closes on outside pointer input", "[ContextMenuWidget]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    ui_test::prepare_surface(surface, {320.0F, 240.0F});

    int click_count = 0;
    auto& button = surface.root().add<ButtonWidget>(surface, "under menu", LayoutSize{px(100.0F), px(32.0F)});
    button.set_layout({
        .size = {px(100.0F), px(32.0F)},
        .placement = {.offset = {8.0F, 8.0F}},
        .in_flow = false,
    });
    button.set_on_click([&click_count] { ++click_count; });

    ContextMenuItems items;
    items.push_back({.label = "item"});
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));
    menu.open_at({160.0F, 120.0F});
    ui_test::draw_surface(surface, 0.2F);

    auto down = ui_test::pointer_event(EventType::PointerDown, {20.0F, 20.0F});
    auto up = ui_test::pointer_event(EventType::PointerUp, {20.0F, 20.0F});
    REQUIRE(surface.dispatch(down));
    REQUIRE(surface.dispatch(up));
    REQUIRE_FALSE(menu.is_open());
    REQUIRE(click_count == 0);

    ui_test::draw_surface(surface, 0.2F);
    REQUIRE_FALSE(menu.visible());
}

TEST_CASE("context menu opens a submenu when its parent is hovered", "[ContextMenuWidget]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    ui_test::prepare_surface(surface, {480.0F, 240.0F});

    ContextMenuItems children;
    children.push_back({.label = "child"});
    ContextMenuItems items;
    items.push_back({.label = "parent", .children = std::move(children)});
    auto& menu = surface.root().add<ContextMenuWidget>(surface, std::move(items));
    menu.open_at({20.0F, 20.0F});
    ui_test::draw_surface(surface, 0.2F);

    const Rect item_rect = menu.children().front()->layout().visual_rect();
    const ImVec2 item_position = {item_rect.min.x + 4.0F, item_rect.min.y + 4.0F};
    auto move = ui_test::pointer_event(EventType::PointerMove, item_position);
    surface.dispatch(move);
    ImGui::GetIO().MousePos = item_position;
    ui_test::draw_surface(surface, 0.2F);

    auto* submenu = dynamic_cast<ContextMenuWidget*>(menu.children()[1].get());
    REQUIRE(submenu != nullptr);
    REQUIRE(submenu->visible());
    REQUIRE(submenu->layout().visual_rect().min.x == Catch::Approx(item_rect.max.x + 6.0F));

    auto cross_gap = ui_test::pointer_event(
        EventType::PointerMove, {(item_rect.max.x + submenu->layout().visual_rect().min.x) * 0.5F, item_rect.min.y + 4.0F}
    );
    surface.dispatch(cross_gap);
    REQUIRE(submenu->is_open());

    auto enter_submenu = ui_test::pointer_event(
        EventType::PointerMove, {submenu->layout().visual_rect().min.x + 4.0F, submenu->layout().visual_rect().min.y + 4.0F}
    );
    surface.dispatch(enter_submenu);
    REQUIRE(submenu->is_open());

    auto leave_item = ui_test::pointer_event(
        EventType::PointerMove, {menu.layout().visual_rect().min.x + 1.0F, menu.layout().visual_rect().min.y + 1.0F}
    );
    surface.dispatch(leave_item);
    ImGui::GetIO().MousePos = leave_item.position;
    ui_test::draw_surface(surface, 0.81F);
    ui_test::draw_surface(surface, 0.0F);
    REQUIRE_FALSE(submenu->is_open());

    surface.dispatch(move);
    ImGui::GetIO().MousePos = item_position;
    ui_test::draw_surface(surface, 0.2F);
    REQUIRE(submenu->is_open());

    ImGui::GetIO().MousePos = {460.0F, 220.0F};
    ui_test::draw_surface(surface, 0.2F);
    REQUIRE_FALSE(submenu->is_open());
    REQUIRE_FALSE(menu.is_open());
}

TEST_CASE("virtual rows expand and collapse independently", "[layout][demo]") {
    Runtime runtime;
    ui::UI surface(runtime, {.backend = ui_test::make_backend()});
    setup_demo(surface, "test");
    auto* list = dynamic_cast<VirtualLayout*>(surface.root().find("demo-virtual-list"));
    REQUIRE(list != nullptr);
    REQUIRE(list->item_count() == 100000);
    REQUIRE(list->children().empty());

    auto detached = list->parent()->remove(*list);
    ui_test::prepare_surface(surface, {240.0F, 180.0F});
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
