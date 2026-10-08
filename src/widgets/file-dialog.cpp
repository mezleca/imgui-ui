#include <imgui-ui/widgets/file-dialog.hpp>

#include <imgui-ui/style/theme.hpp>
#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/text.hpp>

using namespace ui;

FileDialogWidget::FileDialogWidget(std::string label, std::string id)
    : Container(std::move(id), StackDirection::Horizontal, "FileDialog"), m_value(std::move(label)),
      m_field(add<TextWidget>(m_value)) {
    set_content_alignment({0.5F, 0.5F});
    set_size({fit(), fit()});
}

void FileDialogWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    set_font(surface().get_primary_font(), 18.0F);

    configure_all_styles([&theme](Style& style) {
        style.border_color(theme.border_color, 0.15F)
            .padding({14.0F, 18.0F})
            .background_color(theme.background_color)
            .border(BORDER_ALL)
            .border_radius(4.0F)
            .border_style(BorderStyle::Dashed);
    });
    style(StyleType::HOVER).background_color(theme.background_secondary_color).border_color(theme.accent_color);
    m_field.configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color).background_color(theme.transparent).padding({}).border(BORDER_NONE);
    });
}

FileDialogResult FileDialogWidget::show_dialog(FileDialogOperation operation, const FileDialogOptions& options) {
    return surface().file_dialog().show(operation, options);
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
