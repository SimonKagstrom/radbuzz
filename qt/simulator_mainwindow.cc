#include "simulator_mainwindow.hh"

#include "ui_simulator_mainwindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    m_scene = std::make_unique<QGraphicsScene>();
    m_display = std::make_unique<DisplayQt>(m_scene.get());
    m_ui->displayGraphicsView->setScene(m_scene.get());

    // TODO: This is a thread race
    m_left_buzzer_cookie = m_left_buzzer.AttachIrqListener(
        [this](bool state) { m_ui->leftBuzzer->setText(state ? "Buzz!" : "No buzz"); });
    m_right_buzzer_cookie = m_right_buzzer.AttachIrqListener(
        [this](bool state) { m_ui->rightBuzzer->setText(state ? "Buzz!" : "No buzz"); });
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
