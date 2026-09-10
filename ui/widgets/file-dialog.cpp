#include "file-dialog.hpp"

#include "../style/theme.hpp"
#include "../ui.hpp"
#include "text.hpp"

using namespace ui;

FileDialogWidget::FileDialogWidget(UI& ui, std::string label, std::string id)
    : StackContainer(std::move(id), StackDirection::Horizontal), m_ui(ui), m_value(std::move(label)),
      m_field(add<TextWidget>(m_value)) {
    const Theme& theme = m_ui.theme();

    set_input_mode(InputMode::Target);
    set_type_name("FileDialog");
    set_content_alignment({0.5F, 0.5F});
    set_size({grow(), fit()});
    set_font(ui.get_primary_font(18));

    configure_all_styles([&theme](Style& style) {
        style.border_color(theme.border_color, 0.15F)
            .padding({14.0F, 18.0F})
            .background_color(theme.background_color)
            .border(BORDER_ALL)
            .border_radius(theme.box_rounding)
            .border_style(BorderStyle::Dashed);
    });
    configure_style(StyleType::HOVER, [&theme](Style& style) {
        style.background_color(theme.background_secondary_color).border_color(theme.accent_color);
    });
    m_field.configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color).background_color(theme.transparent).padding({}).border(BORDER_NONE);
    });
}

FileDialogResult FileDialogWidget::select_file(const FileDialogOptions& options) {
    return m_ui.file_dialog().open_file(options);
}

FileDialogResult FileDialogWidget::select_files(const FileDialogOptions& options) {
    return m_ui.file_dialog().open_files(options);
}

FileDialogResult FileDialogWidget::select_folder(const std::filesystem::path& default_path) {
    return m_ui.file_dialog().select_folder(default_path);
}

const std::string& FileDialogWidget::value() const {
    return m_value;
}

bool FileDialogWidget::set_value(std::string_view value) {
    if (m_value == value) return false;

    m_value = value;
    m_field.set_text(m_value);
    notify_change();
    return true;
}
