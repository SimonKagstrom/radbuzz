#include "simulator_mainwindow.hh"

#include "ui_simulator_mainwindow.h"

MainWindow::MainWindow(ApplicationState& application_state, QWidget* parent)
    : QMainWindow(parent)
    , m_application_state(application_state)
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

    using Ev = hal::IInput::EventType;

    connect(m_ui->leftButton, &QPushButton::clicked, [this]() { m_on_event(Ev::kLeft); });
    connect(m_ui->rightButton, &QPushButton::clicked, [this]() { m_on_event(Ev::kRight); });
    connect(m_ui->centerButton, &QPushButton::pressed, [this]() { m_on_event(Ev::kButtonDown); });
    connect(m_ui->centerButton, &QPushButton::released, [this]() { m_on_event(Ev::kButtonUp); });

    m_application_state.CheckoutReadWrite().Set<AS::battery_millivolts>(m_ui->socSlider->value());
    connect(m_ui->socSlider, QOverload<int>::of(&QSlider::valueChanged), [this](int value) {
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

hal::IStepperMotor&
MainWindow::GetStepperMotor()
{
    return *m_speedometer;
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
MainWindow::AttachListener(std::function<void(EventType)> on_event)
{
    m_on_event = std::move(on_event);

    return std::make_unique<ListenerCookie>([this]() { m_on_event = [](auto) {}; });
}
