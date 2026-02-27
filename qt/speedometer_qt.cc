#include "speedometer_qt.hh"

#include <cassert>
#include <QGraphicsView>
#include <QPen>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{

constexpr float kNeedleThicknessPx = 8.0F;
constexpr qreal kNeedleLengthPx = 60.0;
constexpr float kStartAngleDegFromBottom = 20.0F;
constexpr float kEndAngleDegFromBottom = 320.0F;

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
    printf("Step: %d\n", delta);
    m_position = std::clamp(m_position + delta, 0, kMaxSteps);

    m_normalized_speed.store(static_cast<float>(m_position) / kMaxSteps);
}

void
SpeedometerQt::OnRepaint()
{
    DrawNeedle(m_normalized_speed.load());
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
