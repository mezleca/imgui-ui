#include "file-dialog.hpp"

#include <nfd.hpp>

#include <utility>

namespace ui {
    class NfdFileDialogBackend final : public FileDialogBackend {
    public:
        NfdFileDialogBackend() {
            m_initialized = NFD::Init() == NFD_OKAY;
            if (!m_initialized) m_error = NFD::GetError();
        }

        ~NfdFileDialogBackend() override {
            if (m_initialized) NFD::Quit();
        }

        FileDialogResult open_file(const FileDialogOptions& options) override {
            if (!m_initialized) return initialization_error();

            const std::vector<nfdfilteritem_t> filters = make_filters(options.filters);
            const std::string default_path = options.default_path.string();
            NFD::UniquePath path;
            const nfdresult_t result = NFD::OpenDialog(
                path, filters.empty() ? nullptr : filters.data(), static_cast<nfdfiltersize_t>(filters.size()),
                default_path.empty() ? nullptr : default_path.c_str()
            );
            if (result != NFD_OKAY) return dialog_result(result);
            return {.status = FileDialogStatus::Accepted, .paths = {path.get()}, .error = {}};
        }

        FileDialogResult open_files(const FileDialogOptions& options) override {
            if (!m_initialized) return initialization_error();

            const std::vector<nfdfilteritem_t> filters = make_filters(options.filters);
            const std::string default_path = options.default_path.string();
            NFD::UniquePathSet paths;
            const nfdresult_t result = NFD::OpenDialogMultiple(
                paths, filters.empty() ? nullptr : filters.data(), static_cast<nfdfiltersize_t>(filters.size()),
                default_path.empty() ? nullptr : default_path.c_str()
            );
            if (result != NFD_OKAY) return dialog_result(result);

            nfdpathsetsize_t count = 0;
            if (NFD::PathSet::Count(paths, count) != NFD_OKAY) return dialog_result(NFD_ERROR);

            FileDialogResult dialog{.status = FileDialogStatus::Accepted, .paths = {}, .error = {}};
            dialog.paths.reserve(count);
            for (nfdpathsetsize_t index = 0; index < count; ++index) {
                NFD::UniquePathSetPath path;
                if (NFD::PathSet::GetPath(paths, index, path) != NFD_OKAY) return dialog_result(NFD_ERROR);
                dialog.paths.emplace_back(path.get());
            }

            return dialog;
        }

        FileDialogResult select_folder(const std::filesystem::path& default_path) override {
            if (!m_initialized) return initialization_error();

            const std::string location = default_path.string();
            NFD::UniquePath path;
            const nfdresult_t result = NFD::PickFolder(path, location.empty() ? nullptr : location.c_str());
            if (result != NFD_OKAY) return dialog_result(result);
            return {.status = FileDialogStatus::Accepted, .paths = {path.get()}, .error = {}};
        }

    private:
        static std::vector<nfdfilteritem_t> make_filters(std::span<const FileDialogFilter> filters) {
            std::vector<nfdfilteritem_t> result;
            result.reserve(filters.size());
            for (const FileDialogFilter& filter : filters) {
                result.push_back({filter.name.c_str(), filter.spec.c_str()});
            }

            return result;
        }

        FileDialogResult initialization_error() const {
            return {.status = FileDialogStatus::Error, .paths = {}, .error = m_error};
        }

        static FileDialogResult dialog_result(nfdresult_t result) {
            if (result == NFD_CANCEL) return {.status = FileDialogStatus::Cancelled, .paths = {}, .error = {}};
            return {.status = FileDialogStatus::Error, .paths = {}, .error = NFD::GetError()};
        }

        std::string m_error;
        bool m_initialized = false;
    };
} // namespace ui

namespace ui {
    std::unique_ptr<FileDialogBackend> FileDialog::make_default_backend() {
        return std::make_unique<NfdFileDialogBackend>();
    }
} // namespace ui
