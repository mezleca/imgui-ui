#include <catch2/catch_test_macros.hpp>

#include <ui/file-dialog.hpp>

#include <memory>
#include <vector>

using namespace ui;

class TestFileDialogBackend final : public FileDialogBackend {
public:
    FileDialogResult open_file(const FileDialogOptions& options) override {
        received_options = options;
        return {.status = FileDialogStatus::Accepted, .paths = {"selected.osu"}};
    }

    FileDialogResult open_files(const FileDialogOptions&) override {
        return {.status = FileDialogStatus::Cancelled};
    }

    FileDialogResult select_folder(const std::filesystem::path&) override {
        return {.status = FileDialogStatus::Cancelled};
    }

    FileDialogOptions received_options;
};

TEST_CASE("file dialog delegates to a custom backend") {
    auto backend = std::make_unique<TestFileDialogBackend>();
    TestFileDialogBackend* backend_ptr = backend.get();
    FileDialog dialog(std::move(backend));

    const std::vector<FileDialogFilter> filters{{"beatmaps", "osu"}};
    const FileDialogResult result = dialog.open_file({.filters = filters, .default_path = "songs"});

    REQUIRE(result.accepted());
    REQUIRE(result.paths == std::vector<std::filesystem::path>{"selected.osu"});
    REQUIRE(backend_ptr->received_options.filters.size() == 1);
    REQUIRE(backend_ptr->received_options.filters.front().name == "beatmaps");
    REQUIRE(backend_ptr->received_options.default_path == "songs");
}
