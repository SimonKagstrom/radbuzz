#include "display_qt.hh"

#include <QPainter>

DisplayQt::DisplayQt(QGraphicsScene* scene)
    : m_screen(
          std::make_unique<QImage>(hal::kDisplayWidth, hal::kDisplayHeight, QImage::Format_RGB32))
    , m_pixmap(scene->addPixmap(QPixmap::fromImage(*m_screen)))
{
    connect(this, SIGNAL(DoFlip()), this, SLOT(UpdateScreen()));
}

uint16_t*
DisplayQt::GetFrameBuffer(hal::IDisplay::Owner owner)
{
    if (owner == hal::IDisplay::Owner::kHardware)
    {
        // Single buffer
        return nullptr;
    }

    return m_frame_buffer.data();
}

void
DisplayQt::Flip()
{
    emit DoFlip();
}

void
DisplayQt::SetActive(bool)
{
    // Ignored here
}

void
DisplayQt::UpdateScreen()
{
    for (int y = 0; y < hal::kDisplayHeight; ++y)
    {
        for (int x = 0; x < hal::kDisplayWidth; ++x)
        {
            auto rgb565 = m_frame_buffer[y * hal::kDisplayWidth + x];
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

    // Set the modified pixmap to the label
    m_pixmap->setPixmap(pixmap);
}
