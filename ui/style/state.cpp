#include "state.hpp"

#include <algorithm>

using namespace ui;

static AnimationValue read_style_property(const ComputedStyle& style, StyleAnimationProperty property) {
    switch (property) {
        case StyleAnimationProperty::PaddingX:
            return style.padding().x;
        case StyleAnimationProperty::PaddingY:
            return style.padding().y;
        case StyleAnimationProperty::MarginX:
            return style.margin().x;
        case StyleAnimationProperty::MarginY:
            return style.margin().y;
        case StyleAnimationProperty::Rotation:
            return style.rotation();
        case StyleAnimationProperty::Scale:
            return style.scale();
        case StyleAnimationProperty::Color:
            return style.color().value;
        case StyleAnimationProperty::BorderColor:
            return style.border_color().value;
        case StyleAnimationProperty::BackgroundColor:
            return style.background_color().value;
        case StyleAnimationProperty::Count:
            return 0.0F;
    }

    return 0.0F;
}

static void apply_style_property(Style& style, StyleAnimationProperty property, const AnimationValue& value) {
    switch (property) {
        case StyleAnimationProperty::PaddingX:
            style.padding({std::max(0.0F, std::get<float>(value)), style.padding().y});
            return;
        case StyleAnimationProperty::PaddingY:
            style.padding({style.padding().x, std::max(0.0F, std::get<float>(value))});
            return;
        case StyleAnimationProperty::MarginX:
            style.margin({std::max(0.0F, std::get<float>(value)), style.margin().y});
            return;
        case StyleAnimationProperty::MarginY:
            style.margin({style.margin().x, std::max(0.0F, std::get<float>(value))});
            return;
        case StyleAnimationProperty::Rotation:
            style.rotation(std::get<float>(value));
            return;
        case StyleAnimationProperty::Scale:
            style.scale(std::get<ImVec2>(value));
            return;
        case StyleAnimationProperty::Color:
            style.color(std::get<ImColor>(value));
            return;
        case StyleAnimationProperty::BorderColor:
            style.border_color(std::get<ImColor>(value));
            return;
        case StyleAnimationProperty::BackgroundColor:
            style.background_color(std::get<ImColor>(value));
            return;
        case StyleAnimationProperty::Count:
            return;
    }
}

static AnimationValue read_style_animation_slot(void* context) {
    return static_cast<StyleAnimationSlot*>(context)->current;
}

static AnimationValue read_style_animation_slot_base(void* context) {
    return static_cast<StyleAnimationSlot*>(context)->base;
}

static void write_style_animation_slot(void* context, const AnimationValue& value) {
    auto& slot = *static_cast<StyleAnimationSlot*>(context);
    slot.current = value;
    slot.override = value;
    if (slot.affects_layout) *slot.layout_dirty = true;
}

static void release_style_animation_slot(void* context) {
    auto& slot = *static_cast<StyleAnimationSlot*>(context);
    slot.current = slot.base;
    slot.override.reset();
    if (slot.affects_layout) *slot.layout_dirty = true;
}

VisualState::VisualState() {
    for (std::size_t index = 0; index < m_animation_slots.size(); ++index) {
        m_animation_slots[index].affects_layout = index < static_cast<std::size_t>(StyleAnimationProperty::Rotation);
        m_animation_slots[index].layout_dirty = &m_layout_dirty;
    }
    current_opacity.value = m_opacity;
    snap_to_style(StyleType::DEFAULT);
}

StyleAnimationSequence VisualState::animate() {
    m_style_animator.cancel();
    m_has_presentation_style = has_animation_overrides();
    return StyleAnimationSequence{*this, m_style_animator.animate()};
}

void VisualState::cancel_animations() {
    const auto slots = animation_slots();
    const bool layout_changed = std::any_of(slots.begin(), slots.end(), [](const auto& slot) {
        return slot.affects_layout && slot.override.has_value();
    });

    m_style_animator.cancel();
    m_animator.cancel();

    for (StyleAnimationSlot& slot : slots) {
        slot.override.reset();
    }

    m_has_presentation_style = false;

    // canceled inset tracks may have changed measured bounds and must trigger one layout pass.
    if (layout_changed && m_change_callback != nullptr) m_change_callback(m_change_owner);
}

void VisualState::update_animations(float dt) {
    m_animator.update(dt);

    const bool style_animating = m_style_animator.transitioning();
    if (!style_animating && !m_has_presentation_style && !has_animation_overrides()) return;

    // rebuild the displayed style before advancing tracks so each new track reads the value shown in the previous frame.
    m_presentation_style = style();
    for (std::size_t index = 0; index < m_animation_slots.size(); ++index) {
        StyleAnimationSlot& slot = m_animation_slots[index];
        const auto property = static_cast<StyleAnimationProperty>(index);
        slot.base = read_style_property(style(), property);
        if (slot.override.has_value()) apply_style_property(m_presentation_style, property, *slot.override);
        slot.current = read_style_property(m_presentation_style, property);
    }

    m_layout_dirty = false;
    m_style_animator.update(dt);

    // apply values written by tracks so this frame draws the updated presentation style.
    m_presentation_style = style();
    for (std::size_t index = 0; index < m_animation_slots.size(); ++index) {
        StyleAnimationSlot& slot = m_animation_slots[index];
        if (slot.override.has_value()) {
            apply_style_property(m_presentation_style, static_cast<StyleAnimationProperty>(index), *slot.override);
        }
    }

    m_has_presentation_style = has_animation_overrides();
    if (m_layout_dirty && m_change_callback != nullptr) m_change_callback(m_change_owner);
}

AnimationTarget VisualState::target(StyleAnimationSlot& slot) {
    return {
        .read = &read_style_animation_slot,
        .base = &read_style_animation_slot_base,
        .write = &write_style_animation_slot,
        .release = &release_style_animation_slot,
        .context = &slot,
        .identity = &slot,
    };
}

bool VisualState::has_animation_overrides() const {
    const auto slots = animation_slots();
    return std::any_of(slots.begin(), slots.end(), [](const auto& slot) { return slot.override.has_value(); });
}
