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
