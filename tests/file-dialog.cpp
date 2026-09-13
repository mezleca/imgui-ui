#include <catch2/catch_test_macros.hpp>

#include <ui/ui.hpp>

#include "imgui-context.hpp"

#include <memory>
#include <vector>

using namespace ui;

class TestFileDialogBackend final : public FileDialogBackend {
public:
    FileDialogResult show(FileDialogOperation operation, const FileDialogOptions& options) override {
        received_operation = operation;
        received_options = options;
        return {.status = FileDialogStatus::Accepted, .paths = {"selected.osu"}};
    }

    FileDialogOperation received_operation = FileDialogOperation::OpenFile;
    FileDialogOptions received_options;
};

TEST_CASE("UI uses the configured file dialog backend") {
    Runtime runtime;
    auto backend = std::make_unique<TestFileDialogBackend>();
    TestFileDialogBackend* backend_ptr = backend.get();
    UI surface(runtime, {.backend = ui_test::make_backend(), .file_dialog_backend = std::move(backend)});

    const std::vector<FileDialogFilter> filters{{"beatmaps", "osu"}};
    const FileDialogResult result = surface.file_dialog().save_file(
        {.filters = filters, .default_path = "songs", .default_name = "collection.osu", .title = "Save collection"}
    );

    REQUIRE(result.accepted());
    REQUIRE(result.paths == std::vector<std::filesystem::path>{"selected.osu"});
    REQUIRE(backend_ptr->received_operation == FileDialogOperation::SaveFile);
    REQUIRE(backend_ptr->received_options.filters.size() == 1);
    REQUIRE(backend_ptr->received_options.filters.front().name == "beatmaps");
    REQUIRE(backend_ptr->received_options.default_path == "songs");
    REQUIRE(backend_ptr->received_options.default_name == "collection.osu");
    REQUIRE(backend_ptr->received_options.title == "Save collection");
}
