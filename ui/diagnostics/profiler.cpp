#include "profiler.hpp"

#include "../constants.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>

namespace ui {
    static uint64_t profile_timestamp() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
        );
    }

    static double profile_milliseconds(uint64_t start, uint64_t end) {
        if (end < start) {
            return 0.0;
        }

        return static_cast<double>(end - start) / 1'000'000.0;
    }

    static std::filesystem::path default_profile_directory() {
        std::error_code error;
        const std::filesystem::path directory = std::filesystem::temp_directory_path(error);
        return error ? std::filesystem::current_path() : directory;
    }

    static uint64_t profile_session_timestamp() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        );
    }

    Profiler::Profiler(std::filesystem::path output_directory) {
        if constexpr (!constants::IS_DEBUG_BUILD) {
            return;
        }

        if (output_directory.empty()) {
            output_directory = default_profile_directory();
        }

        m_output_path = output_directory / (std::to_string(profile_session_timestamp()) + "-ui-perf.txt");
    }

    void Profiler::MetricSummary::record(double value) {
        if (samples == 0) {
            minimum = value;
            maximum = value;
        }

        ++samples;
        total += value;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
        last = value;
    }

    double Profiler::MetricSummary::average() const {
        return samples == 0 ? 0.0 : total / static_cast<double>(samples);
    }

    void Profiler::set_enabled(bool enabled) {
        if constexpr (!constants::IS_DEBUG_BUILD) {
            m_enabled = false;
            return;
        }

        m_enabled = enabled;
        if (!enabled) {
            m_frame_open = false;
        }
    }

    bool Profiler::enabled() const {
        return constants::IS_DEBUG_BUILD && m_enabled;
    }

    void Profiler::set_root_node(uint64_t identity) {
        m_root_identity = identity;
    }

    void Profiler::begin_frame() {
        if constexpr (!constants::IS_DEBUG_BUILD) {
            return;
        }

        if (!m_enabled) {
            return;
        }

        // reuse the inactive buffer and discard the previous frame's events and aggregates.
        FrameBuffer& frame = m_frames[m_write_index];
        frame.count = 0;
        frame.dropped = 0;
        frame.metrics = {};
        frame.node_durations.clear();
        frame.node_durations_ready = false;
        frame.start = profile_timestamp();
        frame.end = frame.start;
        m_frame_open = true;
    }

    void Profiler::end_frame() {
        if constexpr (!constants::IS_DEBUG_BUILD) {
            return;
        }

        if (!m_enabled || !m_frame_open) {
            return;
        }

        // publish only a completed buffer so debugger reads never see a frame being written.
        FrameBuffer& frame = m_frames[m_write_index];
        frame.end = profile_timestamp();
        record_root_phase_times(frame);
        m_read_index = m_write_index;
        m_write_index = 1 - m_write_index;
        m_frame_open = false;

        m_frame_metric.record(profile_milliseconds(frame.start, frame.end));
    }

    std::span<const ProfileEvent> Profiler::latest_events() const {
        const FrameBuffer& frame = m_frames[m_read_index];
        return {frame.events.data(), frame.count};
    }

    const ProfileFrameMetrics& Profiler::latest_metrics() const {
        return m_frames[m_read_index].metrics;
    }

    double Profiler::latest_frame_ms() const {
        const FrameBuffer& frame = m_frames[m_read_index];
        return profile_milliseconds(frame.start, frame.end);
    }

    double Profiler::node_duration_ms(uint64_t node_identity) const {
        if (node_identity == 0) {
            return 0.0;
        }

        // build the node index once; each node total is the sum of its update and draw zones.
        const FrameBuffer& frame = m_frames[m_read_index];
        if (!frame.node_durations_ready) {
            for (std::size_t index = 0; index < frame.count; ++index) {
                const ProfileEvent& event = frame.events[index];
                if (event.node_identity != 0 && (event.name == "Node::update" || event.name == "Node::draw")) {
                    frame.node_durations[event.node_identity] += profile_milliseconds(event.start, event.end);
                }
            }
            frame.node_durations_ready = true;
        }

        const auto it = frame.node_durations.find(node_identity);
        return it == frame.node_durations.end() ? 0.0 : it->second;
    }

    uint32_t Profiler::dropped_events() const {
        return m_frames[m_read_index].dropped;
    }

    void Profiler::record_frame_metrics(std::size_t input_entries, std::size_t input_entry_checks) {
        if (!m_enabled || !m_frame_open) {
            return;
        }

        ProfileFrameMetrics& metrics = m_frames[m_write_index].metrics;
        metrics.input_entries = input_entries;
        metrics.input_entry_checks = input_entry_checks;
    }

    void Profiler::clear_report() {
        m_frame_metric = {};
    }

    bool Profiler::has_report() const {
        return m_frame_metric.samples > 0;
    }

    bool Profiler::save_report() const {
        if (!has_report()) {
            return false;
        }

        // persist aggregate frame timing and the latest frame snapshot; raw events stay in memory.
        std::error_code error;
        std::filesystem::create_directories(m_output_path.parent_path(), error);
        if (error) {
            return false;
        }

        std::ofstream output(m_output_path);
        output << std::fixed << std::setprecision(2);
        const auto write_metric = [&output](std::string_view name, const MetricSummary& metric, std::string_view unit) {
            if (metric.samples == 0) {
                return;
            }

            output << name << ".samples = " << metric.samples << '\n';
            output << name << ".average_" << unit << " = " << metric.average() << '\n';
            output << name << ".min_" << unit << " = " << metric.minimum << '\n';
            output << name << ".max_" << unit << " = " << metric.maximum << '\n';
            output << name << ".last_" << unit << " = " << metric.last << '\n';
        };

        write_metric("frame", m_frame_metric, "ms");

        const ProfileFrameMetrics& metrics = latest_metrics();
        output << "latest.nodes_drawn = " << metrics.nodes_drawn << '\n';
        output << "latest.input_entries = " << metrics.input_entries << '\n';
        output << "latest.input_entry_checks = " << metrics.input_entry_checks << '\n';
        output << "latest.update_ms = " << metrics.update_ms << '\n';
        output << "latest.measure_ms = " << metrics.measure_ms << '\n';
        output << "latest.layout_ms = " << metrics.layout_ms << '\n';
        output << "latest.draw_ms = " << metrics.draw_ms << '\n';
        output << "latest.input_ms = " << metrics.input_ms << '\n';
        output << "latest.render_ms = " << metrics.render_ms << '\n';

        if (!output.good()) {
            return false;
        }

        return true;
    }

    const std::filesystem::path& Profiler::output_path() const {
        return m_output_path;
    }

    Profiler::ZoneToken Profiler::begin_zone(std::string_view name, uint64_t node_identity) {
        if constexpr (!constants::IS_DEBUG_BUILD) {
            return {};
        }

        if (!m_enabled || !m_frame_open) {
            return {};
        }

        FrameBuffer& frame = m_frames[m_write_index];
        if (frame.count >= frame.events.size()) {
            // keep the frame usable when instrumentation reaches the fixed event limit.
            ++frame.dropped;
            return {};
        }

        const std::size_t index = frame.count++;
        frame.events[index] = {
            .name = name,
            .start = profile_timestamp(),
            .node_identity = node_identity,
        };
        return {.event_index = index, .recorded = true};
    }

    void Profiler::end_zone(ZoneToken token) {
        if constexpr (!constants::IS_DEBUG_BUILD) {
            return;
        }

        if (!m_enabled || !m_frame_open) {
            return;
        }

        if (token.recorded) {
            m_frames[m_write_index].events[token.event_index].end = profile_timestamp();
        }
    }

    void Profiler::record_node_draw() {
        if (!m_enabled || !m_frame_open) {
            return;
        }

        ++m_frames[m_write_index].metrics.nodes_drawn;
    }

    void Profiler::record_root_phase_times(FrameBuffer& frame) {
        // root phase times are inclusive; layout and input sum all node zones.
        for (std::size_t index = 0; index < frame.count; ++index) {
            const ProfileEvent& event = frame.events[index];
            const double duration = profile_milliseconds(event.start, event.end);
            if (event.name == "Node::layout") {
                frame.metrics.layout_ms += duration;
            }

            if (event.name == "Node::input") {
                frame.metrics.input_ms += duration;
            }

            if (event.node_identity == m_root_identity && event.name == "Node::update") {
                frame.metrics.update_ms = duration;
            } else if (event.node_identity == m_root_identity && event.name == "Node::measure") {
                frame.metrics.measure_ms = duration;
            } else if (event.node_identity == m_root_identity && event.name == "Node::draw") {
                frame.metrics.draw_ms = duration;
            } else if (event.name == "UI::render") {
                frame.metrics.render_ms = duration;
            }
        }
    }

    ScopedProfileZone::ScopedProfileZone(Profiler* profiler, std::string_view name, uint64_t node_identity)
        : m_profiler(profiler) {
        if (m_profiler != nullptr) {
            m_token = m_profiler->begin_zone(name, node_identity);
        }
    }

    ScopedProfileZone::~ScopedProfileZone() {
        if (m_profiler != nullptr) {
            m_profiler->end_zone(m_token);
        }
    }
} // namespace ui
