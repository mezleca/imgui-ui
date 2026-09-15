#pragma once

#include "../file-dialog/file-dialog.hpp"
#include "../layout/container.hpp"

#include <string>
#include <string_view>

namespace ui {
    class TextWidget;

    class FileDialogWidget final : public Container {
    public:
        explicit FileDialogWidget(std::string label, std::string id = "FileDialog");

        FileDialogResult select_file(const FileDialogOptions& options = {});
        FileDialogResult select_files(const FileDialogOptions& options = {});
        FileDialogResult save_file(const FileDialogOptions& options = {});
        FileDialogResult select_folder(const FileDialogOptions& options = {});
        FileDialogResult select_folders(const FileDialogOptions& options = {});

        [[nodiscard]] const std::string& value() const;
        bool set_value(std::string_view value);

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        std::string m_value;
        TextWidget& m_field;
    };
} // namespace ui
