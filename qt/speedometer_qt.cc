#include "speedometer_qt.hh"

#include <QGraphicsView>
#include <QMetaObject>
#include <QPen>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace
{

constexpr float kNeedleThicknessPx = 8.0F;
constexpr qreal kNeedleLengthPx = 60.0;
// Like the adafruit stepper motor for automotive guages
constexpr float kStartAngleDegFromBottom = 22.5F;
constexpr float kEndAngleDegFromBottom = 337.5F;

constexpr auto kMaxSteps = 6000;

} // namespace

SpeedometerQt::SpeedometerQt(QGraphicsView* graphics_view)
    : m_graphics_view(graphics_view)
    , m_scene(std::make_unique<QGraphicsScene>())
{
    assert(m_graphics_view != nullptr);

    m_graphics_view->setScene(m_scene.get());
}

void
SpeedometerQt::Step(int delta)
{
    m_position = std::clamp(m_position + delta, 0, kMaxSteps);

    auto normalized_speed = static_cast<float>(m_position) / kMaxSteps;

    QMetaObject::invokeMethod(
        m_graphics_view,
        [this, normalized_speed]() { DrawNeedle(normalized_speed); },
        Qt::QueuedConnection);
}

void
SpeedometerQt::DrawNeedle(float normalized_speed)
{
    const QRect viewport_rect = m_graphics_view->viewport()->rect();
    m_scene->setSceneRect(viewport_rect);
    m_scene->clear();

    const QRectF scene_rect = m_scene->sceneRect();
    const QPointF center = scene_rect.center();

    const float clamped_speed = 1.0F - std::clamp(normalized_speed, 0.0F, 1.0F);
    const float angle_deg_from_bottom =
        kStartAngleDegFromBottom +
        (kEndAngleDegFromBottom - kStartAngleDegFromBottom) * clamped_speed;

    const qreal angle_rad = angle_deg_from_bottom * std::numbers::pi_v<float> / 180.0F;
    const QPointF end_point(center.x() + kNeedleLengthPx * std::sin(angle_rad),
                            center.y() + kNeedleLengthPx * std::cos(angle_rad));

    QPen needle_pen(Qt::white);
    needle_pen.setWidthF(kNeedleThicknessPx);
    needle_pen.setCapStyle(Qt::RoundCap);

    m_scene->addLine(QLineF(center, end_point), needle_pen);
}
