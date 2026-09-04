#pragma once

#include <imgui.h>

#include <cstdint>
#include <vector>

namespace ui {
    using EffectId = uint32_t;
    using EffectInitialize = bool (*)(void* user_data);
    using EffectFrame = void (*)(void* user_data);
    using EffectShutdown = void (*)(void* user_data);

    struct EffectDefinition {
        /// called by the renderer for each command submitted with this effect.
        ImDrawCallback render = nullptr;
        /// called after the backend context is ready. return false when setup fails.
        EffectInitialize initialize = nullptr;
        /// called once at the start of each ui frame.
        EffectFrame begin_frame = nullptr;
        /// called while the backend context is still available.
        EffectShutdown shutdown = nullptr;
        void* user_data = nullptr;
    };

    class EffectRegistry {
    public:
        EffectRegistry() = default;

        EffectRegistry(const EffectRegistry&) = delete;
        EffectRegistry& operator=(const EffectRegistry&) = delete;

        EffectId register_effect(EffectDefinition definition);
        bool unregister_effect(EffectId id);

        bool initialize();
        void begin_frame();
        void shutdown();

        /// queues a callback and a render-state reset. payload must live through rendering.
        bool submit(ImDrawList& draw_list, EffectId id, void* payload) const;

        bool initialized() const {
            return m_initialized;
        }

    private:
        struct Entry {
            EffectId id = 0;
            EffectDefinition definition{};
            bool initialized = false;
        };

        Entry* find(EffectId id);
        const Entry* find(EffectId id) const;

        std::vector<Entry> m_entries;
        EffectId m_next_id = 1;
        bool m_initialized = false;
    };

} // namespace ui
