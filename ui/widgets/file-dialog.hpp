#pragma once

#include "../file-dialog.hpp"
#include "../layout/stack-container.hpp"

#include <string>
#include <string_view>

namespace ui {
    class TextWidget;
    class UI;

    class FileDialogWidget final : public StackContainer {
    public:
        explicit FileDialogWidget(UI& ui, std::string label, std::string id = "FileDialog");

        FileDialogResult select_file(const FileDialogOptions& options = {});
        FileDialogResult select_files(const FileDialogOptions& options = {});
        FileDialogResult select_folder(const std::filesystem::path& default_path = {});

        [[nodiscard]] const std::string& value() const;
        bool set_value(std::string_view value);

    private:
        UI& m_ui;
        std::string m_value;
        TextWidget& m_field;
    };
} // namespace ui
