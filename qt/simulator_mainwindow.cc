#include "simulator_mainwindow.hh"

#include "ui_simulator_mainwindow.h"

GpioHost MainWindow::m_pin_a;
GpioHost MainWindow::m_pin_b;

MainWindow::MainWindow(ApplicationState& application_state,
                       PostOffice<MSG::AllMessages>& post_office,
                       QWidget* parent)
    : QMainWindow(parent)
    , RotaryEncoder(m_pin_a, m_pin_b)
    , m_application_state(application_state)
    , m_post_office(post_office)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    auto display_w = hal::kDisplayWidth;
    auto display_h = hal::kDisplayHeight;

    if constexpr (hal::kDisplayRotation != hal::Rotation::k0)
    {
        std::swap(display_w, display_h);
    }

    m_scene = std::make_unique<QGraphicsScene>();
    m_display = std::make_unique<DisplayQt>(m_scene.get(), display_w, display_h);
    m_ui->displayGraphicsView->setScene(m_scene.get());

    // TODO: This is a thread race
    m_left_buzzer_cookie = m_left_buzzer.AttachIrqListener(
        [this](bool state) { m_ui->leftBuzzer->setText(state ? "Buzz!" : "No buzz"); });
    m_right_buzzer_cookie = m_right_buzzer.AttachIrqListener(
        [this](bool state) { m_ui->rightBuzzer->setText(state ? "Buzz!" : "No buzz"); });

    using Dir = RotaryEncoder::Direction;

    connect(m_ui->leftButton, &QPushButton::clicked, [this]() { m_on_rotation(Dir::kLeft); });
    connect(m_ui->rightButton, &QPushButton::clicked, [this]() { m_on_rotation(Dir::kRight); });
    connect(m_ui->centerButton, &QPushButton::pressed, [this]() { m_button.SetState(true); });
    connect(m_ui->centerButton, &QPushButton::released, [this]() { m_button.SetState(false); });

    connect(m_ui->screenshotButton, &QPushButton::clicked, [this]() {
        auto filename = std::format("screenshot_{}.png", m_screenshot_index);

        printf("Saved screenshot '%s'\n", filename.c_str());
        m_display->SaveScreenshot(filename.c_str());
        m_screenshot_index++;
    });

    connect(m_ui->callButton, &QPushButton::clicked, [this]() {
        m_post_office.Send(MSG::incoming_call {"+1555-132634", "Donald Trunk"});
    });
    connect(m_ui->callEndedButton, &QPushButton::clicked, [this]() {
        m_post_office.Send(MSG::call_ended {});
    });
    connect(m_ui->messageButton, &QPushButton::clicked, [this]() {
        m_post_office.Send(MSG::message {
            1234,
            "gmail",
            "Investment proposal",
            "Dear Sir/Madam,\n\nI am writing to you about a unique investment "
            "opportunity. My late uncle left a considerable sum in a bank account, "
            "and I need a trustworthy partner to help me move it.\n\nIn return, you "
            "will receive 30% of the funds.\n\nPlease reply at your earliest "
            "convenience.\n\nYours sincerely,\nA. Prince",
        });
    });
    connect(m_ui->dismissMessageButton, &QPushButton::clicked, [this]() {
        m_post_office.Send(MSG::dismiss_message {.id = 1234});
    });

    m_application_state.CheckoutReadWrite().Set<AS::battery_millivolts>(m_ui->socSlider->value());
    connect(m_ui->socSlider, QOverload<int>::of(&QSlider::valueChanged), [this](int value) {
        printf("Setting millivolts to %d\n", value);
        m_application_state.CheckoutReadWrite().Set<AS::battery_millivolts>(value);
    });

    m_speedometer = std::make_unique<SpeedometerQt>(m_ui->speedometerGraphicsView);
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

hal::IDisplay&
MainWindow::GetDisplay()
{
    return *m_display;
}


hal::ITouch&
MainWindow::GetTouch()
{
    return *m_display;
}

hal::IStepperMotor&
MainWindow::GetStepperMotor()
{
    return *m_speedometer;
}

hal::IGpio&
MainWindow::GetButtonGpio()
{
    return m_button;
}

hal::IGpio&
MainWindow::GetLeftBuzzer()
{
    return m_left_buzzer;
}
hal::IGpio&
MainWindow::GetRightBuzzer()
{
    return m_right_buzzer;
}

std::unique_ptr<ListenerCookie>
MainWindow::AttachIrqListener(std::function<void(RotaryEncoder::Direction)> on_rotation)
{
    m_on_rotation = std::move(on_rotation);

    return std::make_unique<ListenerCookie>([this]() { m_on_rotation = [](auto) {}; });
}
