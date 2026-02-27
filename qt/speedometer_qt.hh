#pragma once

#include "hal/i_stepper_motor.hh"

#include <QGraphicsScene>
#include <memory>

class QGraphicsView;

class SpeedometerQt : public hal::IStepperMotor
{
public:
    explicit SpeedometerQt(QGraphicsView* graphics_view);

private:
    void Step(int delta) final;

    void DrawNeedle(float normalized_speed);

    int m_position {0};

    QGraphicsView* m_graphics_view {nullptr};
    std::unique_ptr<QGraphicsScene> m_scene;
};
