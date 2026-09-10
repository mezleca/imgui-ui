#include "file-dialog.hpp"

#include <utility>

using namespace ui;

FileDialog::FileDialog(std::unique_ptr<FileDialogBackend> backend) : m_backend(std::move(backend)) {
    if (m_backend == nullptr) m_backend = make_default_backend();
}

#if !defined(IMGUI_UI_USE_NFD)
std::unique_ptr<FileDialogBackend> FileDialog::make_default_backend() {
    return {};
}
#endif

FileDialogResult FileDialog::open_file(const FileDialogOptions& options) {
    if (m_backend != nullptr) return m_backend->open_file(options);
    return {.status = FileDialogStatus::Unavailable, .paths = {}, .error = "file dialog backend is unavailable"};
}

FileDialogResult FileDialog::open_files(const FileDialogOptions& options) {
    if (m_backend != nullptr) return m_backend->open_files(options);
    return {.status = FileDialogStatus::Unavailable, .paths = {}, .error = "file dialog backend is unavailable"};
}

FileDialogResult FileDialog::select_folder(const std::filesystem::path& default_path) {
    if (m_backend != nullptr) return m_backend->select_folder(default_path);
    return {.status = FileDialogStatus::Unavailable, .paths = {}, .error = "file dialog backend is unavailable"};
}
