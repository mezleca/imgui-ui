#pragma once

#include "container.hpp"

#include <cstdint>

namespace ui {
    enum class LayerMode : uint8_t {
        Inline,
        Window,
    };

    class LayerContainer : public Container {
    public:
        explicit LayerContainer(std::string id, LayerMode mode = LayerMode::Window);

    protected:
        LayerContainer(std::string id, LayerMode mode, std::string_view type_name);
        void resolve_layout() override;
        bool paint() override;
        void on_draw_end() override;

    private:
        LayerMode m_mode;
    };
} // namespace ui
