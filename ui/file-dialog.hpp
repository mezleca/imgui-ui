#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ui {
    struct FileDialogFilter {
        std::string name;
        std::string spec;
    };

    struct FileDialogOptions {
        std::span<const FileDialogFilter> filters;
        std::filesystem::path default_path;
    };

    enum class FileDialogStatus {
        Accepted,
        Cancelled,
        Unavailable,
        Error,
    };

    struct FileDialogResult {
        FileDialogStatus status = FileDialogStatus::Unavailable;
        std::vector<std::filesystem::path> paths;
        std::string error;

        [[nodiscard]] bool accepted() const {
            return status == FileDialogStatus::Accepted;
        }
    };

    class FileDialogBackend {
    public:
        virtual ~FileDialogBackend() = default;

        virtual FileDialogResult open_file(const FileDialogOptions& options) = 0;
        virtual FileDialogResult open_files(const FileDialogOptions& options) = 0;
        virtual FileDialogResult select_folder(const std::filesystem::path& default_path) = 0;
    };

    class FileDialog {
    public:
        explicit FileDialog(std::unique_ptr<FileDialogBackend> backend = {});

        FileDialog(const FileDialog&) = delete;
        FileDialog& operator=(const FileDialog&) = delete;

        FileDialogResult open_file(const FileDialogOptions& options = {});
        FileDialogResult open_files(const FileDialogOptions& options = {});
        FileDialogResult select_folder(const std::filesystem::path& default_path = {});

    private:
        static std::unique_ptr<FileDialogBackend> make_default_backend();

        std::unique_ptr<FileDialogBackend> m_backend;
    };
} // namespace ui
