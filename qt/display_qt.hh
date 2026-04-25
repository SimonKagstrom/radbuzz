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
    DisplayQt(QGraphicsScene* scene, bool is_round);

    uint16_t* GetFrameBuffer(hal::IDisplay::Owner owner) final;
    void Flip() final;

signals:
    void DoFlip();

private slots:
    void UpdateScreen();


private:
    void SetActive(bool active) final;

    const bool m_is_round;
    std::unique_ptr<QImage> m_screen;
    QImage m_circle_mask;
    QGraphicsPixmapItem* m_pixmap;

    std::atomic<uint8_t> m_current_update_frame {0};
    std::array<uint16_t *, 3> m_frame_buffers;
};
