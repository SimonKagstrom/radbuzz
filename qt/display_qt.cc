#include "display_qt.hh"

#include "lvgl.h"

#include <QPainter>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

DisplayQt::DisplayQt(QGraphicsScene* scene, uint16_t display_width, uint16_t display_height)
    : m_display_width(display_width)
    , m_display_height(display_height)
    , m_screen(std::make_unique<QImage>(m_display_width, m_display_height, QImage::Format_RGB32))
    , m_pixmap(scene->addPixmap(QPixmap::fromImage(*m_screen)))
    , m_scene(scene)
    , m_staged_frame(static_cast<size_t>(m_display_width) * static_cast<size_t>(m_display_height))
{
    for (auto i = 0; i < 3; ++i)
    {
        posix_memalign((void**)&m_frame_buffers[i],
                       LV_DRAW_BUF_ALIGN,
                       m_display_width * m_display_height * sizeof(uint16_t));
    }

    connect(this, SIGNAL(DoFlip()), this, SLOT(UpdateScreen()));
    scene->installEventFilter(this);
}

void
DisplayQt::SaveScreenshot(const char* filename)
{
    QImage screenshot = *m_screen;

    if constexpr (hal::kDisplayRotation != hal::Rotation::k0)
    {
        QTransform transform;
        switch (hal::kDisplayRotation)
        {
        case hal::Rotation::k90:
            transform.rotate(-90);
            break;
        case hal::Rotation::k180:
            transform.rotate(-180);
            break;
        case hal::Rotation::k270:
            transform.rotate(-270);
            break;
        default:
            break;
        }

        screenshot = screenshot.transformed(transform);
    }

    screenshot.save(filename);
}

bool
DisplayQt::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_scene)
    {
        return false;
    }

    switch (event->type())
    {
    // Written by Copilot.
    case QEvent::GraphicsSceneMousePress: {
        auto* mouse_event = static_cast<QGraphicsSceneMouseEvent*>(event);
        const auto bounds = m_pixmap->sceneBoundingRect();
        const double scale_x =
            bounds.width() > 0.0 ? static_cast<double>(hal::kDisplayWidth) / bounds.width() : 1.0;
        const double scale_y = bounds.height() > 0.0
                                   ? static_cast<double>(hal::kDisplayHeight) / bounds.height()
                                   : 1.0;
        const int32_t x =
            std::clamp<int32_t>(static_cast<int32_t>(std::lround(
                                    (mouse_event->scenePos().x() - bounds.left()) * scale_x)),
                                0,
                                static_cast<int32_t>(hal::kDisplayWidth) - 1);
        const int32_t y =
            std::clamp<int32_t>(static_cast<int32_t>(std::lround(
                                    (mouse_event->scenePos().y() - bounds.top()) * scale_y)),
                                0,
                                static_cast<int32_t>(hal::kDisplayHeight) - 1);
        hal::ITouch::Data touch_data {};
        touch_data.x = static_cast<uint16_t>(x);
        touch_data.y = static_cast<uint16_t>(y);
        touch_data.pressed = true;
        touch_data.was_pressed = false;
        m_touch_data_queue.push(touch_data);
        m_on_state_changed();
        return false;
    }
    case QEvent::GraphicsSceneMouseMove: {
        auto* mouse_event = static_cast<QGraphicsSceneMouseEvent*>(event);
        if (mouse_event->buttons() & Qt::LeftButton)
        {
            const auto bounds = m_pixmap->sceneBoundingRect();
            const double scale_x = bounds.width() > 0.0
                                       ? static_cast<double>(hal::kDisplayWidth) / bounds.width()
                                       : 1.0;
            const double scale_y = bounds.height() > 0.0
                                       ? static_cast<double>(hal::kDisplayHeight) / bounds.height()
                                       : 1.0;
            const int32_t x =
                std::clamp<int32_t>(static_cast<int32_t>(std::lround(
                                        (mouse_event->scenePos().x() - bounds.left()) * scale_x)),
                                    0,
                                    static_cast<int32_t>(hal::kDisplayWidth) - 1);
            const int32_t y =
                std::clamp<int32_t>(static_cast<int32_t>(std::lround(
                                        (mouse_event->scenePos().y() - bounds.top()) * scale_y)),
                                    0,
                                    static_cast<int32_t>(hal::kDisplayHeight) - 1);
            hal::ITouch::Data touch_data {};
            touch_data.x = static_cast<uint16_t>(x);
            touch_data.y = static_cast<uint16_t>(y);
            touch_data.pressed = true;
            touch_data.was_pressed = true;
            m_touch_data_queue.push(touch_data);
            m_on_state_changed();
        }
        return false;
    }
    case QEvent::GraphicsSceneMouseRelease: {
        auto* mouse_event = static_cast<QGraphicsSceneMouseEvent*>(event);
        const auto bounds = m_pixmap->sceneBoundingRect();
        const double scale_x =
            bounds.width() > 0.0 ? static_cast<double>(hal::kDisplayWidth) / bounds.width() : 1.0;
        const double scale_y = bounds.height() > 0.0
                                   ? static_cast<double>(hal::kDisplayHeight) / bounds.height()
                                   : 1.0;
        const int32_t x =
            std::clamp<int32_t>(static_cast<int32_t>(std::lround(
                                    (mouse_event->scenePos().x() - bounds.left()) * scale_x)),
                                0,
                                static_cast<int32_t>(hal::kDisplayWidth) - 1);
        const int32_t y =
            std::clamp<int32_t>(static_cast<int32_t>(std::lround(
                                    (mouse_event->scenePos().y() - bounds.top()) * scale_y)),
                                0,
                                static_cast<int32_t>(hal::kDisplayHeight) - 1);
        hal::ITouch::Data touch_data {};
        touch_data.x = static_cast<uint16_t>(x);
        touch_data.y = static_cast<uint16_t>(y);
        touch_data.was_pressed = true;
        touch_data.pressed = false;
        m_touch_data_queue.push(touch_data);
        m_on_state_changed();
        return false;
    }
    default:
        return false;
    }
}

uint16_t*
DisplayQt::GetFrameBuffer(hal::IDisplay::Owner owner)
{
    if (owner == hal::IDisplay::Owner::kHardware)
    {
        return m_frame_buffers[!m_current_update_frame];
    }
    if (owner == hal::IDisplay::Owner::kRotationBuffer)
    {
        return m_frame_buffers[2];
    }

    return m_frame_buffers[m_current_update_frame];
}

void
DisplayQt::Flip()
{
    auto display_frame = m_current_update_frame.load(std::memory_order_acquire);
    auto* src = m_frame_buffers[display_frame];

    {
        auto lock = std::lock_guard(m_staged_frame_mutex);
        std::memcpy(
            m_staged_frame.data(), src, m_staged_frame.size() * sizeof(m_staged_frame.front()));
    }

    emit DoFlip();
    m_current_update_frame = !display_frame;
}

void
DisplayQt::SetActive(bool)
{
    // Ignored here
}

void
DisplayQt::UpdateScreen()
{
    auto lock = std::lock_guard(m_staged_frame_mutex);
    auto* src = m_staged_frame.data();
    auto src_stride = m_display_width;

    for (int y = 0; y < m_display_height; ++y)
    {
        for (int x = 0; x < m_display_width; ++x)
        {
            auto rgb565 = src[y * src_stride + x];
            auto r = (rgb565 >> 11) & 0x1F;
            auto g = (rgb565 >> 5) & 0x3F;
            auto b = rgb565 & 0x1F;

            r = (r * 255) / 31;
            g = (g * 255) / 63;
            b = (b * 255) / 31;

            auto color = QColor(r, g, b);

            m_screen->setPixelColor(x, y, color);
        }
    }


    // (from copilot)
    QPixmap pixmap = QPixmap::fromImage(*m_screen);

    if (m_display_width == m_display_height)
    {
        // Create a QPainter to draw on the pixmap
        QPainter painter(&pixmap);

        // Set the pen and brush for drawing
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);

        // Calculate the center and radius of the circle
        int width = pixmap.width();
        int height = pixmap.height();

        // Create a mask region
        QRegion maskRegion(0, 0, width, height);
        QRegion circleRegion(0, 0, width - 1, height - 1, QRegion::Ellipse);
        maskRegion = maskRegion.subtracted(circleRegion);

        // Fill the masked area with black
        painter.setClipRegion(maskRegion);
        painter.fillRect(0, 0, width, height, Qt::black);
    }


    // If it's rotated, rotate the image back to run the code, but get usable output
    if constexpr (hal::kDisplayRotation != hal::Rotation::k0)
    {
        QTransform transform;
        switch (hal::kDisplayRotation)
        {
        case hal::Rotation::k90:
            transform.rotate(-90);
            break;
        case hal::Rotation::k180:
            transform.rotate(-180);
            break;
        case hal::Rotation::k270:
            transform.rotate(-270);
            break;
        default:
            break;
        }
        pixmap = pixmap.transformed(transform);
    }

    // Set the modified pixmap to the label
    m_pixmap->setPixmap(pixmap);
}

std::unique_ptr<ListenerCookie>
DisplayQt::AttachIrqListener(std::function<void()> on_state_changed)
{
    m_on_state_changed = std::move(on_state_changed);

    return std::make_unique<ListenerCookie>([this]() { m_on_state_changed = []() {}; });
}

std::span<const hal::ITouch::Data>
DisplayQt::GetActiveTouchData()
{
    m_data_vector.clear();

    hal::ITouch::Data data;
    while (m_touch_data_queue.pop(data))
    {
        data.x = std::clamp(
            data.x, static_cast<uint16_t>(0), static_cast<uint16_t>(hal::kDisplayWidth - 1));
        data.y = std::clamp(
            data.y, static_cast<uint16_t>(0), static_cast<uint16_t>(hal::kDisplayHeight - 1));
        m_data_vector.push_back(data);
    }

    return m_data_vector;
}
