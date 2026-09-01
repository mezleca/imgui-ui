#include <catch2/catch_test_macros.hpp>

#include <ui/input/router.hpp>
#include <ui/layout/modal-container.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/widget.hpp>

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
        .button = ui::PointerButton::Left,
        .key = ui::Key::Unknown,
    };
}

static UiEvent click_event(ImVec2 position = {}) {
    return event_of(EventType::Click, position);
}

class EventNode final : public ui::Node {
public:
    explicit EventNode(std::string node_id, std::vector<std::string>& events) : ui::Node(std::move(node_id)), m_events(events) {
        _on_event = [this](ui::UiEvent& event) {
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

class EventWidget final : public ui::Widget {
public:
    explicit EventWidget(std::vector<std::string>& events) : ui::Widget("widget"), m_events(events) {
        _on_event = [this](ui::UiEvent&) { m_events.push_back("internal"); };
    }

private:
    std::vector<std::string>& m_events;
};

class PointerCaptureNode final : public ui::Node {
public:
    PointerCaptureNode(ui::InputRouter& router, std::vector<ui::EventType>& events)
        : ui::Node("drag"), m_router(router), m_events(events) {
        _on_event = [this](ui::UiEvent& event) {
            m_events.push_back(event.type);
            if (event.type == ui::EventType::PointerDown) {
                REQUIRE(m_router.capture_pointer(*this));
            }
            event.mark_handled();
        };
    }

private:
    ui::InputRouter& m_router;
    std::vector<ui::EventType>& m_events;
};

class PointerEventNode final : public ui::Node {
public:
    PointerEventNode(std::string node_id, std::vector<ui::EventType>& events) : ui::Node(std::move(node_id)), m_events(events) {
        _on_event = [this](ui::UiEvent& event) {
            m_events.push_back(event.type);
            event.mark_handled();
        };
    }

private:
    std::vector<ui::EventType>& m_events;
};

TEST_CASE("widget event handlers preserve internal behavior") {
    std::vector<std::string> events;
    EventWidget widget(events);
    widget.on_event = [&events](ui::UiEvent&) { events.push_back("public"); };

    ui::InputRouter router;
    ui::UiEvent event = click_event();
    REQUIRE_FALSE(router.dispatch(widget, event));
    REQUIRE(events == std::vector<std::string>{"internal", "public"});
}

TEST_CASE("ui events flows from target to parents") {
    std::vector<std::string> events;
    auto parent = std::make_unique<EventNode>("parent", events);
    auto child = std::make_unique<EventNode>("child", events);
    EventNode* child_ptr = child.get();
    parent->add(std::move(child));

    ui::InputRouter router;
    ui::UiEvent event = click_event();
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
    parent->add(std::move(child));

    ui::InputRouter router;
    ui::UiEvent event = click_event();
    const bool handled = router.dispatch(*child_ptr, event);
    REQUIRE(handled);

    REQUIRE(event.handled);
    REQUIRE(event.propagation_stopped);
    REQUIRE(events == std::vector<std::string>{"child"});
}

TEST_CASE("pointer capture keeps drag events on the original node") {
    ui::InputRouter router;
    std::vector<ui::EventType> events;
    PointerCaptureNode node(router, events);

    router.register_region(node, {.rect = {{0.0F, 0.0F}, {10.0F, 10.0F}}});

    auto down = event_of(ui::EventType::PointerDown, {5.0F, 5.0F});
    REQUIRE(router.dispatch(down));

    router.begin_frame();
    auto move = event_of(ui::EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE(router.dispatch(move));

    auto up = event_of(ui::EventType::PointerUp, {100.0F, 100.0F});
    REQUIRE(router.dispatch(up));
    REQUIRE(
        events == std::vector<ui::EventType>{
                      ui::EventType::PointerDown,
                      ui::EventType::PointerMove,
                      ui::EventType::PointerUp,
                  }
    );

    router.begin_frame();
    auto move_after_release = event_of(ui::EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE_FALSE(router.dispatch(move_after_release));
}

TEST_CASE("input router synthesizes clicks from matching pointer presses") {
    std::vector<ui::EventType> events;
    PointerEventNode node("click", events);
    ui::InputRouter router;
    router.register_region(node, {.rect = {{0.0F, 0.0F}, {10.0F, 10.0F}}});

    auto left_down = event_of(ui::EventType::PointerDown, {5.0F, 5.0F});
    left_down.button = ui::PointerButton::Left;
    REQUIRE(router.dispatch(left_down));

    auto left_up = event_of(ui::EventType::PointerUp, {5.0F, 5.0F});
    left_up.button = ui::PointerButton::Left;
    REQUIRE(router.dispatch(left_up));
    REQUIRE(events == std::vector<ui::EventType>{ui::EventType::PointerDown, ui::EventType::Click});

    events.clear();
    auto right_down = event_of(ui::EventType::PointerDown, {5.0F, 5.0F});
    right_down.button = ui::PointerButton::Right;
    REQUIRE(router.dispatch(right_down));

    auto right_up = event_of(ui::EventType::PointerUp, {5.0F, 5.0F});
    right_up.button = ui::PointerButton::Right;
    REQUIRE(router.dispatch(right_up));
    REQUIRE(events == std::vector<ui::EventType>{ui::EventType::PointerDown, ui::EventType::ContextClick});

    events.clear();
    auto drag_down = event_of(ui::EventType::PointerDown, {5.0F, 5.0F});
    drag_down.button = ui::PointerButton::Left;
    REQUIRE(router.dispatch(drag_down));

    auto drag_up = event_of(ui::EventType::PointerUp, {20.0F, 20.0F});
    drag_up.button = ui::PointerButton::Left;
    REQUIRE_FALSE(router.dispatch(drag_up));
    REQUIRE(events == std::vector<ui::EventType>{ui::EventType::PointerDown});
}

TEST_CASE("input blocker consumes only its selected event mask") {
    std::vector<ui::EventType> events;
    PointerEventNode target("target", events);
    ui::InputRouter router;
    int target_events = 0;
    router.register_region(
        target, {
                    .rect = {{0.0F, 0.0F}, {100.0F, 100.0F}},
                    .on_event = [&target_events](ui::UiEvent&) { ++target_events; },
                }
    );
    int blocked_events = 0;
    router.register_blocker({
        .rect = {{25.0F, 25.0F}, {75.0F, 75.0F}},
        .events = ui::EventMask::PointerDown,
        .on_event = [&blocked_events](ui::UiEvent&) { ++blocked_events; },
    });

    auto move = event_of(ui::EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE(events == std::vector<ui::EventType>{ui::EventType::PointerMove});
    REQUIRE(target_events == 1);

    auto down = event_of(ui::EventType::PointerDown, {50.0F, 50.0F});
    REQUIRE(router.dispatch(down));
    REQUIRE(events == std::vector<ui::EventType>{ui::EventType::PointerMove});
    REQUIRE(blocked_events == 1);
    REQUIRE(target_events == 1);
}

TEST_CASE("input router reports per-frame hit-test work") {
    std::vector<ui::EventType> events;
    PointerEventNode node("target", events);
    ui::InputRouter router;
    router.register_region(node, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    router.register_blocker({.rect = {{200.0F, 200.0F}, {300.0F, 300.0F}}});

    auto move = event_of(ui::EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));

    const ui::InputRouterStats stats = router.stats();
    REQUIRE(stats.region_count == 2);
    REQUIRE(stats.hit_test_count == 2);
    REQUIRE(stats.region_checks == 4);

    router.begin_frame();
    REQUIRE(router.stats().region_count == 0);
    REQUIRE(router.stats().hit_test_count == 0);
    REQUIRE(router.stats().region_checks == 0);
}

TEST_CASE("input router skips blocker hit testing when none are registered") {
    std::vector<ui::EventType> events;
    PointerEventNode node("target", events);
    ui::InputRouter router;
    router.register_region(node, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    auto move = event_of(ui::EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));

    const ui::InputRouterStats stats = router.stats();
    REQUIRE(stats.region_count == 1);
    REQUIRE(stats.hit_test_count == 1);
    REQUIRE(stats.region_checks == 1);
}

TEST_CASE("input router skips observer scans when none are registered") {
    std::vector<ui::EventType> events;
    PointerEventNode node("target", events);
    ui::InputRouter router;
    router.register_region(node, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    auto click = event_of(ui::EventType::Click, {50.0F, 50.0F});
    REQUIRE(router.dispatch(click));

    const ui::InputRouterStats stats = router.stats();
    REQUIRE(stats.region_count == 1);
    REQUIRE(stats.hit_test_count == 1);
    REQUIRE(stats.region_checks == 1);
}

TEST_CASE("owner-scoped blockers leave their descendants interactive") {
    std::vector<ui::EventType> events;
    ui::Node owner("overlay");
    auto child = std::make_unique<PointerEventNode>("child", events);
    auto* child_ptr = child.get();
    owner.add(std::move(child));

    ui::InputRouter router;
    router.register_region(*child_ptr, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    router.register_blocker(owner, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    auto move = event_of(ui::EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE(events == std::vector<ui::EventType>{ui::EventType::PointerMove});
}

TEST_CASE("input router invalidates inactive focus and pointer capture") {
    std::vector<std::string> events;
    EventNode node("input", events);
    node.handle_events = true;

    ui::InputRouter router;
    REQUIRE(router.set_focus(node));
    events.clear();

    node.set_visible(false);
    auto hidden_key = event_of(ui::EventType::KeyDown);
    REQUIRE_FALSE(router.dispatch(hidden_key));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(events.empty());

    node.set_visible(true);
    REQUIRE(router.set_focus(node));
    events.clear();

    node.set_enabled(false);
    auto disabled_key = event_of(ui::EventType::KeyDown);
    REQUIRE_FALSE(router.dispatch(disabled_key));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(events.empty());

    node.set_enabled(true);
    REQUIRE(router.capture_pointer(node));
    events.clear();

    node.set_enabled(false);
    auto disabled_move = event_of(ui::EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE_FALSE(router.dispatch(disabled_move));
    REQUIRE(events.empty());
}

TEST_CASE("pointer down outside a focused node clears focus") {
    std::vector<std::string> events;
    EventNode focused("focused", events);
    EventNode other("other", events);
    focused.handle_events = true;
    other.handle_events = true;

    ui::InputRouter router;
    router.register_region(focused, {.rect = {{0.0F, 0.0F}, {40.0F, 40.0F}}});
    router.register_region(other, {.rect = {{60.0F, 0.0F}, {100.0F, 40.0F}}});
    REQUIRE(router.set_focus(focused));
    events.clear();

    auto down = event_of(ui::EventType::PointerDown, {80.0F, 20.0F});
    REQUIRE(router.dispatch(down));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(events == std::vector<std::string>{"focused", "other"});
}

TEST_CASE("input router clears targets when a node is detached") {
    std::vector<std::string> events;
    ui::Node parent("parent");
    auto child = std::make_unique<EventNode>("child", events);
    EventNode* child_ptr = child.get();
    child_ptr->handle_events = true;
    parent.add(std::move(child));

    ui::InputRouter router;
    parent.set_input_router(&router);
    REQUIRE(router.set_focus(*child_ptr));
    REQUIRE(router.capture_pointer(*child_ptr));
    router.register_region(*child_ptr, {.rect = {{0.0F, 0.0F}, {10.0F, 10.0F}}});
    events.clear();

    auto detached = parent.remove(*child_ptr);
    REQUIRE(detached != nullptr);
    events.clear();

    auto key = event_of(ui::EventType::KeyDown);
    REQUIRE_FALSE(router.dispatch(key));
    REQUIRE(router.focused_node() == nullptr);
    REQUIRE(router.node_at({5.0F, 5.0F}) == nullptr);
    REQUIRE(events.empty());

    auto move = event_of(ui::EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE_FALSE(router.dispatch(move));
    REQUIRE(events.empty());
}

TEST_CASE("input routers isolate focus and pointer capture between surfaces") {
    std::vector<std::string> events;
    EventNode surface_a_node("surface-a", events);
    EventNode surface_b_node("surface-b", events);
    surface_a_node.handle_events = true;
    surface_b_node.handle_events = true;

    ui::InputRouter surface_a_router;
    ui::InputRouter surface_b_router;
    REQUIRE(surface_a_router.set_focus(surface_a_node));
    REQUIRE(surface_b_router.set_focus(surface_b_node));
    REQUIRE(surface_a_router.capture_pointer(surface_a_node));
    REQUIRE(surface_b_router.capture_pointer(surface_b_node));

    events.clear();
    auto surface_a_key = event_of(ui::EventType::KeyDown);
    auto surface_b_key = event_of(ui::EventType::KeyDown);
    REQUIRE(surface_a_router.dispatch(surface_a_key));
    REQUIRE(surface_b_router.dispatch(surface_b_key));
    REQUIRE(events == std::vector<std::string>{"surface-a", "surface-b"});

    surface_a_router.clear_focus();
    surface_a_router.release_pointer();
    REQUIRE(surface_a_router.focused_node() == nullptr);
    REQUIRE(surface_b_router.focused_node() == &surface_b_node);

    auto surface_b_move = event_of(ui::EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE(surface_b_router.dispatch(surface_b_move));
    REQUIRE(events.back() == "surface-b");
}

TEST_CASE("blocking region consumes empty space") {
    ui::InputRouter router;
    router.register_blocker({.rect = {{0.0F, 0.0F}, {200.0F, 200.0F}}});

    ui::UiEvent move = event_of(ui::EventType::PointerMove, {100.0F, 100.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE(move.handled);
}

TEST_CASE("blocking regions clear hover behind them") {
    ui::InputRouter router;
    ui::Node target("target");
    router.register_region(target, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    ui::UiEvent move = event_of(ui::EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE_FALSE(router.dispatch(move));
    REQUIRE(target.input_state().hovered);

    router.register_blocker({.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    move = event_of(ui::EventType::PointerMove, {50.0F, 50.0F});
    REQUIRE(router.dispatch(move));
    REQUIRE_FALSE(target.input_state().hovered);
}

TEST_CASE("blocking regions consume pointer release without a retained press") {
    ui::InputRouter router;
    router.register_blocker({.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    ui::UiEvent release = event_of(ui::EventType::PointerUp, {50.0F, 50.0F});
    REQUIRE(router.dispatch(release));
    REQUIRE(release.handled);
}

TEST_CASE("observer regions do not block their target") {
    std::vector<std::string> events;
    EventNode target("target", events);
    target.handle_events = true;
    ui::Node observer_owner("observer");
    ui::InputRouter router;
    int observed = 0;

    router.register_region(target, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    router.register_observer(
        observer_owner, {
                            .rect = {{0.0F, 0.0F}, {100.0F, 100.0F}},
                            .events = ui::EventMask::Click,
                            .on_event = [&observed](ui::UiEvent&) { ++observed; },
                        }
    );

    ui::UiEvent click = click_event({50.0F, 50.0F});
    REQUIRE(router.dispatch(click));
    REQUIRE(observed == 1);
    REQUIRE(events == std::vector<std::string>{"target"});
}

TEST_CASE("higher-priority regions win over later paint") {
    std::vector<std::string> events;
    EventNode popup("popup", events);
    EventNode content("content", events);
    popup.handle_events = true;
    content.handle_events = true;
    ui::InputRouter router;

    router.register_region(popup, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}, .priority = 1});
    router.register_region(content, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    ui::UiEvent click = click_event({50.0F, 50.0F});
    REQUIRE(router.dispatch(click));
    REQUIRE(events == std::vector<std::string>{"popup"});
}

TEST_CASE("hidden modal panels release the modal input policy") {
    ui::Runtime runtime;
    ui::Config config;
    UI surface(runtime, std::move(config));
    ui::ModalContainer modal(surface);

    ui::ModalPanel& panel = modal.open("panel");
    REQUIRE(modal.has_open_modal());

    panel.set_visible(false);
    modal.update(0.0F);

    ui::UiEvent key = event_of(ui::EventType::KeyDown);
    REQUIRE_FALSE(surface.input_router().dispatch(key));
    REQUIRE_FALSE(key.handled);
}

TEST_CASE("pointer blockers leave focused keyboard input available") {
    std::vector<std::string> events;
    EventNode content("content", events);
    content.handle_events = true;

    ui::InputRouter router;
    REQUIRE(router.set_focus(content));
    events.clear();

    router.register_region(content, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    router.register_blocker({.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    ui::UiEvent click = click_event({10.0F, 10.0F});
    REQUIRE(router.dispatch(click));
    REQUIRE(events.empty());

    ui::UiEvent key = event_of(ui::EventType::KeyDown);
    REQUIRE(router.dispatch(key));
    REQUIRE(events == std::vector<std::string>{"content"});
}

TEST_CASE("input router resolves the topmost node at a position") {
    ui::InputRouter router;
    ui::Node bottom("bottom");
    ui::Node top("top");

    router.begin_frame();
    router.register_region(bottom, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    router.register_region(top, {.rect = {{25.0F, 25.0F}, {75.0F, 75.0F}}});

    REQUIRE(router.node_at({50.0F, 50.0F}) == &top);
    REQUIRE(router.node_at({10.0F, 10.0F}) == &bottom);
    REQUIRE(router.node_at({150.0F, 150.0F}) == nullptr);
}

TEST_CASE("input router uses registration order for overlapping unrelated nodes") {
    ui::InputRouter router;
    ui::Node first("first");
    ui::Node second("second");

    router.begin_frame();
    router.register_region(first, {.rect = {{0.0F, 0.0F}, {20.0F, 20.0F}}});
    router.register_region(second, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    REQUIRE(router.node_at({10.0F, 10.0F}) == &second);
}

TEST_CASE("input router ignores disabled and stale regions") {
    ui::InputRouter router;
    ui::Node disabled("disabled");
    ui::Node hidden("hidden");

    router.begin_frame();
    router.register_region(disabled, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    router.register_region(hidden, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    disabled.set_enabled(false);
    hidden.set_visible(false);

    REQUIRE(router.node_at({50.0F, 50.0F}) == nullptr);

    router.begin_frame();
    REQUIRE(router.node_at({50.0F, 50.0F}) == nullptr);
}

TEST_CASE("debug tree includes passive containers") {
    class DebugNode final : public ui::Node {
    public:
        explicit DebugNode(std::string id) : Node(std::move(id)) {}

        bool debug_selectable() const override {
            return true;
        }

        void set_rect(ui::Rect rect) {
            set_screen_rect(rect);
        }
    };

    class TransparentNode final : public ui::Node {
    public:
        explicit TransparentNode(std::string id) : Node(std::move(id)) {}

        void set_rect(ui::Rect rect) {
            set_screen_rect(rect);
        }
    };

    DebugNode root("root");
    root.set_rect({{0.0F, 0.0F}, {200.0F, 200.0F}});
    auto overlay = std::make_unique<TransparentNode>("overlay");
    overlay->set_rect({{0.0F, 0.0F}, {200.0F, 200.0F}});
    auto child = std::make_unique<DebugNode>("container");
    DebugNode* child_ptr = child.get();
    child_ptr->set_rect({{20.0F, 20.0F}, {180.0F, 180.0F}});
    overlay->add(std::move(child));
    root.add(std::move(overlay));

    REQUIRE(root.debug_node_at({100.0F, 100.0F}) == child_ptr);
    REQUIRE(root.debug_node_at({10.0F, 10.0F}) == &root);
}

TEST_CASE("closed modal containers are skipped by debugger selection") {
    ui::Runtime runtime;
    ui::Config config;
    UI surface(runtime, std::move(config));
    ui::ModalContainer modal(surface);

    REQUIRE_FALSE(modal.debug_selectable());
    modal.open("panel");
    REQUIRE(modal.debug_selectable());
}

TEST_CASE("input router prefers an overlapping child over its parent") {
    ui::InputRouter router;
    ui::Node parent("parent");
    auto child = std::make_unique<ui::Node>("child");
    ui::Node* child_ptr = child.get();
    parent.add(std::move(child));

    router.begin_frame();
    router.register_region(*child_ptr, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});
    router.register_region(parent, {.rect = {{0.0F, 0.0F}, {100.0F, 100.0F}}});

    REQUIRE(router.node_at({50.0F, 50.0F}) == child_ptr);
}

TEST_CASE("focused node receives keyboard events") {
    std::vector<std::string> events;
    EventNode content("content", events);
    EventNode modal("modal", events);
    content.handle_events = true;
    modal.handle_events = true;

    ui::InputRouter router;
    REQUIRE(router.set_focus(content));
    events.clear();

    ui::UiEvent key = event_of(ui::EventType::KeyDown);
    REQUIRE(router.dispatch(key));
    REQUIRE(events == std::vector<std::string>{"content"});

    REQUIRE(router.set_focus(modal));
    events.clear();
    ui::UiEvent text = event_of(ui::EventType::TextInput);
    text.text = "osu";
    REQUIRE(router.dispatch(text));
    REQUIRE(events == std::vector<std::string>{"modal"});

    router.clear_focus();
    REQUIRE(router.focused_node() == nullptr);
}
