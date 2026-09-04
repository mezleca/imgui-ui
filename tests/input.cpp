#include <catch2/catch_test_macros.hpp>

#include <ui/input/router.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/widget.hpp>
#include "imgui-context.hpp"

#include <memory>
#include <string>
#include <vector>

using namespace ui;

static UiEvent event_of(EventType type, ImVec2 position = {}) {
    return {
        .position = position,
        .scroll = {},
        .text = {},
        .handled = false,
        .propagation_stopped = false,
        .default_prevented = false,
        .type = type,
        .button = PointerButton::Left,
        .key = Key::Unknown,
    };
}

static UiEvent click_event(ImVec2 position = {}) {
    return event_of(EventType::Click, position);
}

class EventNode final : public Node {
public:
    explicit EventNode(std::string node_id, std::vector<std::string>& events) : Node(std::move(node_id)), m_events(events) {
        _on_event = [this](UiEvent& event) {
            m_events.push_back(id());
            if (stop_events) {
                event.stop_propagation();
            } else if (handle_events) {
                event.mark_handled();
            }
        };
    }

    bool handle_events = false;
    bool stop_events = false;

private:
    std::vector<std::string>& m_events;
};

class EventWidget final : public Widget {
public:
    explicit EventWidget(std::vector<std::string>& events) : Widget("widget"), m_events(events) {
        _on_event = [this](UiEvent&) { m_events.push_back("internal"); };
    }

private:
    std::vector<std::string>& m_events;
};

class PointerCaptureNode final : public Node {
public:
    PointerCaptureNode(InputRouter& router, std::vector<EventType>& events) : Node("drag"), m_router(router), m_events(events) {
        _on_event = [this](UiEvent& event) {
            m_events.push_back(event.type);
            if (event.type == EventType::PointerDown) {
                REQUIRE(m_router.capture_pointer(*this));
            }
            event.mark_handled();
        };
    }

private:
    InputRouter& m_router;
    std::vector<EventType>& m_events;
};

class PointerEventNode final : public Node {
public:
    PointerEventNode(std::string node_id, std::vector<EventType>& events) : Node(std::move(node_id)), m_events(events) {
        _on_event = [this](UiEvent& event) {
            m_events.push_back(event.type);
            event.mark_handled();
        };
    }

private:
    std::vector<EventType>& m_events;
};

TEST_CASE("widget event handlers preserve internal behavior") {
    std::vector<std::string> events;
    EventWidget widget(events);
    widget.on_event = [&events](UiEvent&) { events.push_back("public"); };

    InputRouter router;
    UiEvent event = click_event();
    REQUIRE_FALSE(router.dispatch(widget, event));
    REQUIRE(events == std::vector<std::string>{"internal", "public"});
}

TEST_CASE("ui events flows from target to parents") {
    std::vector<std::string> events;
    auto parent = std::make_unique<EventNode>("parent", events);
    auto child = std::make_unique<EventNode>("child", events);
    EventNode* child_ptr = child.get();
    parent->attach(std::move(child));

    InputRouter router;
    UiEvent event = click_event();
    const bool handled = router.dispatch(*child_ptr, event);
    REQUIRE_FALSE(handled);

    REQUIRE(events == std::vector<std::string>{"child", "parent"});
}

TEST_CASE("ui events can stop propagation") {
    std::vector<std::string> events;
    auto parent = std::make_unique<EventNode>("parent", events);
    auto child = std::make_unique<EventNode>("child", events);
    EventNode* child_ptr = child.get();
    child_ptr->stop_events = true;
    parent->attach(std::move(child));

    InputRouter router;
    UiEvent event = click_event();
    const bool handled = router.dispatch(*child_ptr, event);
    REQUIRE(handled);

    REQUIRE(event.handled);
    REQUIRE(event.propagation_stopped);
    REQUIRE(events == std::vector<std::string>{"child"});
}

TEST_CASE("pointer capture keeps drag events on the original node") {
    InputRouter router;
    std::vector<EventType> events;
    PointerCaptureNode node(router, events);

    router.target(node, {{0.0F, 0.0F}, {10.0F, 10.0F}});

    auto down = event_of(EventType::PointerDown, {5.0F, 5.0F});
    REQUIRE(router.dispatch(down));

    router.begin_frame();
    auto move = event_of(EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE(router.dispatch(move));

    auto up = event_of(EventType::PointerUp, {100.0F, 100.0F});
    REQUIRE(router.dispatch(up));
    REQUIRE(
        events == std::vector<EventType>{
                      EventType::PointerDown,
                      EventType::PointerMove,
                      EventType::PointerUp,
                  }
    );

    router.begin_frame();
    auto move_after_release = event_of(EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE_FALSE(router.dispatch(move_after_release));
}

TEST_CASE("input router synthesizes clicks from matching pointer presses") {
    std::vector<EventType> events;
    PointerEventNode node("click", events);
    InputRouter router;
    router.target(node, {{0.0F, 0.0F}, {10.0F, 10.0F}});

    auto left_down = event_of(EventType::PointerDown, {5.0F, 5.0F});
    left_down.button = PointerButton::Left;
    REQUIRE(router.dispatch(left_down));

    auto left_up = event_of(EventType::PointerUp, {5.0F, 5.0F});
    left_up.button = PointerButton::Left;
    REQUIRE(router.dispatch(left_up));
    REQUIRE(events == std::vector<EventType>{EventType::PointerDown, EventType::Click});

    events.clear();
    auto right_down = event_of(EventType::PointerDown, {5.0F, 5.0F});
    right_down.button = PointerButton::Right;
    REQUIRE(router.dispatch(right_down));

    auto right_up = event_of(EventType::PointerUp, {5.0F, 5.0F});
    right_up.button = PointerButton::Right;
    REQUIRE(router.dispatch(right_up));
    REQUIRE(events == std::vector<EventType>{EventType::PointerDown, EventType::ContextClick});

    events.clear();
    auto drag_down = event_of(EventType::PointerDown, {5.0F, 5.0F});
    drag_down.button = PointerButton::Left;
    REQUIRE(router.dispatch(drag_down));

    auto drag_up = event_of(EventType::PointerUp, {20.0F, 20.0F});
    drag_up.button = PointerButton::Left;
    REQUIRE_FALSE(router.dispatch(drag_up));
    REQUIRE(events == std::vector<EventType>{EventType::PointerDown});
}

TEST_CASE("input blocker consumes only its selected event mask") {
    std::vector<EventType> events;
    PointerEventNode target("target", events);
    InputRouter router;
    int target_events = 0;
    router.target(target, {{0.0F, 0.0F}, {100.0F, 100.0F}}, [&target_events](UiEvent&) { ++target_events; });
    int blocked_events = 0;
    router.block({{25.0F, 25.0F}, {75.0F, 75.0F}}, [&blocked_events](UiEvent&) { ++blocked_events; }, EventMask::PointerDown);

    auto move = event_of(EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE(events == std::vector<EventType>{EventType::PointerMove});
    REQUIRE(target_events == 1);

    auto down = event_of(EventType::PointerDown, {50.0F, 50.0F});
    REQUIRE(router.dispatch(down));
    REQUIRE(events == std::vector<EventType>{EventType::PointerMove});
    REQUIRE(blocked_events == 1);
    REQUIRE(target_events == 1);
}

TEST_CASE("input router reports per-frame hit-test work") {
    std::vector<EventType> events;
    PointerEventNode node("target", events);
    InputRouter router;
    router.target(node, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.block({{200.0F, 200.0F}, {300.0F, 300.0F}});

    auto move = event_of(EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));

    const InputRouterStats stats = router.stats();
    REQUIRE(stats.entry_count == 2);
    REQUIRE(stats.hit_test_count == 2);
    REQUIRE(stats.entry_checks == 4);

    router.begin_frame();
    REQUIRE(router.stats().entry_count == 0);
    REQUIRE(router.stats().hit_test_count == 0);
    REQUIRE(router.stats().entry_checks == 0);
}

TEST_CASE("input router skips blocker hit testing when none are registered") {
    std::vector<EventType> events;
    PointerEventNode node("target", events);
    InputRouter router;
    router.target(node, {{0.0F, 0.0F}, {100.0F, 100.0F}});

    auto move = event_of(EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));

    const InputRouterStats stats = router.stats();
    REQUIRE(stats.entry_count == 1);
    REQUIRE(stats.hit_test_count == 1);
    REQUIRE(stats.entry_checks == 1);
}

TEST_CASE("input router skips observer scans when none are registered") {
    std::vector<EventType> events;
    PointerEventNode node("target", events);
    InputRouter router;
    router.target(node, {{0.0F, 0.0F}, {100.0F, 100.0F}});

    auto click = event_of(EventType::Click, {50.0F, 50.0F});
    REQUIRE(router.dispatch(click));

    const InputRouterStats stats = router.stats();
    REQUIRE(stats.entry_count == 1);
    REQUIRE(stats.hit_test_count == 1);
    REQUIRE(stats.entry_checks == 1);
}

TEST_CASE("owner-scoped blockers leave their descendants interactive") {
    std::vector<EventType> events;
    Node owner("overlay");
    auto child = std::make_unique<PointerEventNode>("child", events);
    auto* child_ptr = child.get();
    owner.attach(std::move(child));

    InputRouter router;
    router.target(*child_ptr, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.block(owner, {{0.0F, 0.0F}, {100.0F, 100.0F}});

    auto move = event_of(EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE(events == std::vector<EventType>{EventType::PointerMove});
}

TEST_CASE("input router invalidates inactive focus and pointer capture") {
    std::vector<std::string> events;
    EventNode node("input", events);
    node.handle_events = true;

    InputRouter router;
    REQUIRE(router.set_focus(node));
    events.clear();

    node.set_visible(false);
    auto hidden_key = event_of(EventType::KeyDown);
    REQUIRE_FALSE(router.dispatch(hidden_key));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(events.empty());

    node.set_visible(true);
    REQUIRE(router.set_focus(node));
    events.clear();

    node.set_enabled(false);
    auto disabled_key = event_of(EventType::KeyDown);
    REQUIRE_FALSE(router.dispatch(disabled_key));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(events.empty());

    node.set_enabled(true);
    REQUIRE(router.capture_pointer(node));
    events.clear();

    node.set_enabled(false);
    auto disabled_move = event_of(EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE_FALSE(router.dispatch(disabled_move));
    REQUIRE(events.empty());
}

TEST_CASE("pointer down outside a focused node clears focus") {
    std::vector<std::string> events;
    EventNode focused("focused", events);
    EventNode other("other", events);
    focused.handle_events = true;
    other.handle_events = true;

    InputRouter router;
    router.target(focused, {{0.0F, 0.0F}, {40.0F, 40.0F}});
    router.target(other, {{60.0F, 0.0F}, {100.0F, 40.0F}});
    REQUIRE(router.set_focus(focused));
    events.clear();

    auto down = event_of(EventType::PointerDown, {80.0F, 20.0F});
    REQUIRE(router.dispatch(down));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(events == std::vector<std::string>{"focused", "other"});
}

TEST_CASE("input router clears targets when a node is detached") {
    std::vector<std::string> events;
    Node parent("parent");
    auto child = std::make_unique<EventNode>("child", events);
    EventNode* child_ptr = child.get();
    child_ptr->handle_events = true;
    parent.attach(std::move(child));

    InputRouter router;
    parent.set_input_router(&router);
    REQUIRE(router.set_focus(*child_ptr));
    REQUIRE(router.capture_pointer(*child_ptr));
    router.target(*child_ptr, {{0.0F, 0.0F}, {10.0F, 10.0F}});
    events.clear();

    auto detached = parent.remove(*child_ptr);
    REQUIRE(detached != nullptr);
    events.clear();

    auto key = event_of(EventType::KeyDown);
    REQUIRE_FALSE(router.dispatch(key));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(router.node_at({5.0F, 5.0F}) == nullptr);
    REQUIRE(events.empty());

    auto move = event_of(EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE_FALSE(router.dispatch(move));
    REQUIRE(events.empty());
}

TEST_CASE("node input attachment survives router destruction") {
    Node node("node");

    {
        InputRouter router;
        node.set_input_router(&router);
        REQUIRE(router.set_focus(node));
        REQUIRE(router.capture_pointer(node));
        router.target(node, {{0.0F, 0.0F}, {10.0F, 10.0F}});
    }

    node.set_visible(false);
    node.set_input_target();
    REQUIRE_FALSE(node.input_state().focused);
    node.set_visible(true);

    InputRouter replacement;
    node.set_input_router(&replacement);
    REQUIRE(replacement.set_focus(node));
}

TEST_CASE("input routers isolate focus and pointer capture between surfaces") {
    std::vector<std::string> events;
    EventNode surface_a_node("surface-a", events);
    EventNode surface_b_node("surface-b", events);
    surface_a_node.handle_events = true;
    surface_b_node.handle_events = true;

    InputRouter surface_a_router;
    InputRouter surface_b_router;
    REQUIRE(surface_a_router.set_focus(surface_a_node));
    REQUIRE(surface_b_router.set_focus(surface_b_node));
    REQUIRE(surface_a_router.capture_pointer(surface_a_node));
    REQUIRE(surface_b_router.capture_pointer(surface_b_node));

    events.clear();
    auto surface_a_key = event_of(EventType::KeyDown);
    auto surface_b_key = event_of(EventType::KeyDown);
    REQUIRE(surface_a_router.dispatch(surface_a_key));
    REQUIRE(surface_b_router.dispatch(surface_b_key));
    REQUIRE(events == std::vector<std::string>{"surface-a", "surface-b"});

    surface_a_router.clear_focus();
    surface_a_router.release_pointer();
    REQUIRE(surface_a_router.focused_node() == nullptr);
    REQUIRE(surface_b_router.focused_node() == &surface_b_node);

    auto surface_b_move = event_of(EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE(surface_b_router.dispatch(surface_b_move));
    REQUIRE(events.back() == "surface-b");
}

TEST_CASE("blocking entry consumes empty space") {
    InputRouter router;
    router.block({{0.0F, 0.0F}, {200.0F, 200.0F}});

    UiEvent move = event_of(EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE(move.handled);
}

TEST_CASE("blocking entries clear hover behind them") {
    InputRouter router;
    Node target("target");
    router.target(target, {{0.0F, 0.0F}, {100.0F, 100.0F}});

    UiEvent move = event_of(EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE_FALSE(router.dispatch(move));
    REQUIRE(target.input_state().hovered);

    router.block({{0.0F, 0.0F}, {100.0F, 100.0F}});
    move = event_of(EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE_FALSE(target.input_state().hovered);
}

TEST_CASE("blocking entries consume pointer release without a retained press") {
    InputRouter router;
    router.block({{0.0F, 0.0F}, {100.0F, 100.0F}});

    UiEvent release = event_of(EventType::PointerUp, {50.0F, 50.0F});
    REQUIRE(router.dispatch(release));
    REQUIRE(release.handled);
}

TEST_CASE("observer entries do not block their target") {
    std::vector<std::string> events;
    EventNode target("target", events);
    target.handle_events = true;
    InputRouter router;
    int observed = 0;

    router.target(target, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.observe({{0.0F, 0.0F}, {100.0F, 100.0F}}, [&observed](UiEvent&) { ++observed; }, EventMask::Click);

    UiEvent click = click_event({50.0F, 50.0F});
    REQUIRE(router.dispatch(click));
    REQUIRE(observed == 1);
    REQUIRE(events == std::vector<std::string>{"target"});
}

TEST_CASE("later targets win over earlier paint") {
    std::vector<std::string> events;
    EventNode popup("popup", events);
    EventNode content("content", events);
    popup.handle_events = true;
    content.handle_events = true;
    InputRouter router;

    router.target(content, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.target(popup, {{0.0F, 0.0F}, {100.0F, 100.0F}});

    UiEvent click = click_event({50.0F, 50.0F});
    REQUIRE(router.dispatch(click));
    REQUIRE(events == std::vector<std::string>{"popup"});
}

TEST_CASE("hidden layers release focus") {
    Runtime runtime;
    UI surface(runtime);
    LayerContainer layer("layer", LayerMode::Inline);
    layer.set_input_router(&surface.input_router());

    REQUIRE(surface.input_router().set_focus(layer));
    REQUIRE(surface.input_router().focused_node() == &layer);

    layer.set_visible(false);

    UiEvent key = event_of(EventType::KeyDown);
    REQUIRE_FALSE(surface.input_router().dispatch(key));
    REQUIRE_FALSE(key.handled);
    REQUIRE(surface.input_router().focused_node() == nullptr);
}

TEST_CASE("pointer blockers leave focused keyboard input available") {
    std::vector<std::string> events;
    EventNode content("content", events);
    content.handle_events = true;

    InputRouter router;
    REQUIRE(router.set_focus(content));
    events.clear();

    router.target(content, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.block({{0.0F, 0.0F}, {100.0F, 100.0F}});

    UiEvent click = click_event({10.0F, 10.0F});
    REQUIRE(router.dispatch(click));
    REQUIRE(events.empty());

    UiEvent key = event_of(EventType::KeyDown);
    REQUIRE(router.dispatch(key));
    REQUIRE(events == std::vector<std::string>{"content"});
}

TEST_CASE("input router resolves overlapping targets") {
    InputRouter router;
    Node bottom("bottom");
    Node top("top");

    router.begin_frame();
    router.target(bottom, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.target(top, {{25.0F, 25.0F}, {75.0F, 75.0F}});

    REQUIRE(router.node_at({50.0F, 50.0F}) == &top);
    REQUIRE(router.node_at({10.0F, 10.0F}) == &bottom);
    REQUIRE(router.node_at({150.0F, 150.0F}) == nullptr);

    Node first("first");
    Node second("second");

    router.begin_frame();
    router.target(first, {{0.0F, 0.0F}, {20.0F, 20.0F}});
    router.target(second, {{0.0F, 0.0F}, {100.0F, 100.0F}});

    REQUIRE(router.node_at({10.0F, 10.0F}) == &second);

    Node parent("parent");
    auto child = std::make_unique<Node>("child");
    Node* child_ptr = child.get();
    parent.attach(std::move(child));

    router.begin_frame();
    router.target(*child_ptr, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.target(parent, {{0.0F, 0.0F}, {100.0F, 100.0F}});

    REQUIRE(router.node_at({50.0F, 50.0F}) == child_ptr);
}

TEST_CASE("input router ignores disabled and stale entries") {
    InputRouter router;
    Node disabled("disabled");
    Node hidden("hidden");

    router.begin_frame();
    router.target(disabled, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    router.target(hidden, {{0.0F, 0.0F}, {100.0F, 100.0F}});
    disabled.set_enabled(false);
    hidden.set_visible(false);

    REQUIRE(router.node_at({50.0F, 50.0F}) == nullptr);

    router.begin_frame();
    REQUIRE(router.node_at({50.0F, 50.0F}) == nullptr);
}

TEST_CASE("focused node receives keyboard events") {
    std::vector<std::string> events;
    EventNode content("content", events);
    EventNode modal("modal", events);
    content.handle_events = true;
    modal.handle_events = true;

    InputRouter router;
    REQUIRE(router.set_focus(content));
    events.clear();

    UiEvent key = event_of(EventType::KeyDown);
    REQUIRE(router.dispatch(key));
    REQUIRE(events == std::vector<std::string>{"content"});

    REQUIRE(router.set_focus(modal));
    events.clear();
    UiEvent text = event_of(EventType::TextInput);
    text.text = "osu";
    REQUIRE(router.dispatch(text));
    REQUIRE(events == std::vector<std::string>{"modal"});

    router.clear_focus();
    REQUIRE(router.focused_node() == nullptr);
}
