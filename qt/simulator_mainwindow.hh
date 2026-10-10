#pragma once

#include "application_state.hh"
#include "display_qt.hh"
#include "gpio_host.hh"
#include "hal/i_input.hh"
#include "messages.hh"
#include "post_office.hh"
#include "rotary_encoder.hh"
#include "speedometer_qt.hh"
#include "wifi_client_host.hh"

#include <queue>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMainWindow>
#include <QPaintEvent>
#include <QPoint>

namespace Ui
{
class MainWindow;
}

class MainWindow final : public QMainWindow, public RotaryEncoder
{
    Q_OBJECT

public:
    explicit MainWindow(ApplicationState& application_state,
                        PostOffice<MSG::AllMessages>& post_office,
                        WifiClientHost& wifi_client,
                        QWidget* parent = nullptr);
    ~MainWindow() final;

    hal::IDisplay& GetDisplay();

    hal::ITouch& GetTouch();

    hal::IStepperMotor& GetStepperMotor();

    hal::IGpio& GetButtonGpio();

    hal::IGpio& GetLeftBuzzer();

    hal::IGpio& GetRightBuzzer();

private slots:


private:
    std::unique_ptr<ListenerCookie>
    AttachIrqListener(std::function<void(RotaryEncoder::Direction)> on_rotation) final;

    ApplicationState& m_application_state;
    PostOffice<MSG::AllMessages>& m_post_office;
    WifiClientHost& m_wifi_client;
    Ui::MainWindow* m_ui {nullptr};

    std::unique_ptr<QGraphicsScene> m_scene;
    std::unique_ptr<SpeedometerQt> m_speedometer;
    std::unique_ptr<DisplayQt> m_display;

    uint8_t m_state {0};

    GpioHost m_button;
    GpioHost m_left_buzzer;
    GpioHost m_right_buzzer;

    // Ugly
    static GpioHost m_pin_a;
    static GpioHost m_pin_b;

    std::unique_ptr<ListenerCookie> m_left_buzzer_cookie;
    std::unique_ptr<ListenerCookie> m_right_buzzer_cookie;

    std::function<void(RotaryEncoder::Direction)> m_on_rotation {[](auto) { /* NOP */ }};

    int m_screenshot_index {0};
    uint32_t m_message_index {9957};
    std::queue<uint32_t> m_message_ids;
};
