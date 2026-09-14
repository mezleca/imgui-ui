#pragma once

#include <filesystem>
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
        std::string default_name;
        std::string title;
        std::string accept_label;
        std::string cancel_label;
    };

    enum class FileDialogOperation {
        OpenFile,
        OpenFiles,
        SaveFile,
        SelectFolder,
        SelectFolders,
    };

    enum class FileDialogStatus {
        Accepted,
        Cancelled,
        Unavailable,
        Error,
    };

    /// carries the selected paths or diagnostic text returned by a file dialog request.
    struct FileDialogResult {
        FileDialogStatus status = FileDialogStatus::Unavailable;
        std::vector<std::filesystem::path> paths;
        std::string error;

        /// returns true when the backend reports that the dialog completed successfully.
        [[nodiscard]] bool accepted() const {
            return status == FileDialogStatus::Accepted;
        }
    };

    class FileDialogBackend {
    public:
        virtual ~FileDialogBackend() = default;

        FileDialogResult open_file(const FileDialogOptions& options = {}) {
            return show(FileDialogOperation::OpenFile, options);
        }

        FileDialogResult open_files(const FileDialogOptions& options = {}) {
            return show(FileDialogOperation::OpenFiles, options);
        }

        FileDialogResult save_file(const FileDialogOptions& options = {}) {
            return show(FileDialogOperation::SaveFile, options);
        }

        FileDialogResult select_folder(const FileDialogOptions& options = {}) {
            return show(FileDialogOperation::SelectFolder, options);
        }

        FileDialogResult select_folders(const FileDialogOptions& options = {}) {
            return show(FileDialogOperation::SelectFolders, options);
        }

        virtual FileDialogResult show(FileDialogOperation operation, const FileDialogOptions& options) = 0;
    };

} // namespace ui
