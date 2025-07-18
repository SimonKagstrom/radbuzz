#pragma once

#include "hal/i_display.hh"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QObject>

class DisplayQt : public QObject, public hal::IDisplay
{
    Q_OBJECT

public:
    DisplayQt(QGraphicsScene* scene);

    uint16_t* GetFrameBuffer(hal::IDisplay::Owner owner) final;
    void Flip() final;

signals:
    void DoFlip();

private slots:
    void UpdateScreen();


private:
    void SetActive(bool active) final;

    std::unique_ptr<QImage> m_screen;
    QImage m_circle_mask;
    QGraphicsPixmapItem* m_pixmap;

    std::array<uint16_t, hal::kDisplayWidth * hal::kDisplayHeight> m_frame_buffer;
};
