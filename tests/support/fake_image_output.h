#ifndef KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_IMAGE_OUTPUT_H
#define KOVAAKSSTATISTICSVIEWER_TESTS_FAKE_IMAGE_OUTPUT_H

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "app/contracts/image_output.h"
#include "app/contracts/raster_image.h"
#include "data/interfaces/i_clipboard_image_writer.h"
#include "data/interfaces/i_png_image_writer.h"

namespace ksv::tests_support {
    // What a writer saw of an image. Deliberately not the image itself: holding a RasterImage would
    // keep its storage alive and defeat the pixel-lifetime cases.
    struct SeenImage {
        int width = 0;
        int height = 0;
        const std::uint32_t *pixels = nullptr;
        std::thread::id thread;
    };

    struct SeenPngWrite {
        SeenImage image;
        std::filesystem::path destination;
    };

    // Safe to call from the save worker thread while the test reads from its own.
    class FakeClipboardImageWriter final : public data::IClipboardImageWriter {
    public:
        application::ImageOutputStatus publish(const application::RasterImage &image) override {
            std::function<void()> hook;
            application::ImageOutputStatus result;
            {
                std::scoped_lock lock(m_mutex);
                m_seen.push_back({image.width(), image.height(), image.pixels(), std::this_thread::get_id()});
                hook = m_hook;
                result = m_status;
            }
            if (hook) hook();
            return result;
        }

        void setStatus(application::ImageOutputStatus status) {
            std::scoped_lock lock(m_mutex);
            m_status = status;
        }

        // Runs on the calling thread of publish() after it is recorded; it may throw.
        void setHook(std::function<void()> hook) {
            std::scoped_lock lock(m_mutex);
            m_hook = std::move(hook);
        }

        [[nodiscard]] std::vector<SeenImage> seen() const {
            std::scoped_lock lock(m_mutex);
            return m_seen;
        }

        [[nodiscard]] std::size_t publishCount() const {
            std::scoped_lock lock(m_mutex);
            return m_seen.size();
        }

    private:
        mutable std::mutex m_mutex;
        application::ImageOutputStatus m_status = application::ImageOutputStatus::Succeeded;
        std::function<void()> m_hook;
        std::vector<SeenImage> m_seen;
    };

    class FakePngImageWriter final : public data::IPngImageWriter {
    public:
        application::ImageOutputStatus write(const application::RasterImage &image,
                                             const std::filesystem::path &destination) override {
            std::function<void()> hook;
            {
                std::unique_lock lock(m_mutex);
                m_seen.push_back({{image.width(), image.height(), image.pixels(), std::this_thread::get_id()},
                                  destination});
                hook = m_hook;
                m_gateChanged.wait(lock, [this] { return !m_gateClosed; });
            }
            if (hook) hook();
            std::scoped_lock lock(m_mutex);
            return m_status;
        }

        void setStatus(application::ImageOutputStatus status) {
            std::scoped_lock lock(m_mutex);
            m_status = status;
        }

        // Runs on the writing thread once the gate is open; it may throw.
        void setHook(std::function<void()> hook) {
            std::scoped_lock lock(m_mutex);
            m_hook = std::move(hook);
        }

        // While closed, write() blocks after recording the call.
        void closeGate() {
            std::scoped_lock lock(m_mutex);
            m_gateClosed = true;
        }

        void openGate() {
            {
                std::scoped_lock lock(m_mutex);
                m_gateClosed = false;
            }
            m_gateChanged.notify_all();
        }

        [[nodiscard]] std::vector<SeenPngWrite> seen() const {
            std::scoped_lock lock(m_mutex);
            return m_seen;
        }

        [[nodiscard]] std::size_t writeCount() const {
            std::scoped_lock lock(m_mutex);
            return m_seen.size();
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_gateChanged;
        bool m_gateClosed = false;
        application::ImageOutputStatus m_status = application::ImageOutputStatus::Succeeded;
        std::function<void()> m_hook;
        std::vector<SeenPngWrite> m_seen;
    };
}

#endif
