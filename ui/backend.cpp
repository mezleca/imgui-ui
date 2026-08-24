#include "backend.hpp"

namespace ui {
    static BackendFactory backend_factory = nullptr;

    void set_backend(BackendFactory factory) {
        backend_factory = factory;
    }

    std::unique_ptr<Backend> create_backend(const Config& config) {
        return backend_factory == nullptr ? nullptr : backend_factory(config);
    }
} // namespace ui
