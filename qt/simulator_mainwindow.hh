#pragma once

#include "application_state.hh"
#include "display_qt.hh"
#include "gpio_host.hh"
#include "speedometer_qt.hh"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMainWindow>
#include <QPaintEvent>

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ApplicationState& application_state, QWidget* parent = nullptr);
    ~MainWindow() final;

    hal::IDisplay& GetDisplay();

    hal::IStepperMotor& GetStepperMotor();

    hal::IGpio& GetLeftBuzzer();

    hal::IGpio& GetRightBuzzer();

private slots:


private:
    ApplicationState& m_application_state;

    Ui::MainWindow* m_ui {nullptr};

    std::unique_ptr<QGraphicsScene> m_scene;
    std::unique_ptr<SpeedometerQt> m_speedometer;
    std::unique_ptr<DisplayQt> m_display;

    uint8_t m_state {0};

    GpioHost m_left_buzzer;
    GpioHost m_right_buzzer;

    std::unique_ptr<ListenerCookie> m_left_buzzer_cookie;
    std::unique_ptr<ListenerCookie> m_right_buzzer_cookie;
};
