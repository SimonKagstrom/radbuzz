#pragma once

#include "hal/i_stepper_motor.hh"

#include <QGraphicsScene>
#include <memory>
#include <atomic>

class QGraphicsView;

class SpeedometerQt : public hal::IStepperMotor
{
public:
    explicit SpeedometerQt(QGraphicsView* graphics_view);

    void OnRepaint();

private:
    void Step(int delta) final;

    void DrawNeedle(float normalized_speed);

    int m_position {0};
    std::atomic<float> m_normalized_speed {0.0f};

    QGraphicsView* m_graphics_view {nullptr};
    std::unique_ptr<QGraphicsScene> m_scene;
};
