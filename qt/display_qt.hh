#pragma once

#include "hal/i_display.hh"
#include "hal/i_touch.hh"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <etl/queue_spsc_atomic.h>
#include <functional>

class DisplayQt : public QObject, public hal::IDisplay, public hal::ITouch
{
    Q_OBJECT

public:
    DisplayQt(QGraphicsScene* scene, uint16_t display_width, uint16_t display_height);

    uint16_t* GetFrameBuffer(hal::IDisplay::Owner owner) final;
    void Flip() final;

signals:
    void DoFlip();

private slots:
    void UpdateScreen();


private:
    std::unique_ptr<ListenerCookie> AttachIrqListener(std::function<void()> on_state_changed) final;

    std::span<const hal::ITouch::Data> GetActiveTouchData() final;


    void SetActive(bool active) final;

    bool eventFilter(QObject* watched, QEvent* event) override;

    const uint16_t m_display_width;
    const uint16_t m_display_height;
    std::unique_ptr<QImage> m_screen;
    QImage m_circle_mask;
    QGraphicsPixmapItem* m_pixmap;
    QGraphicsScene* m_scene;

    std::atomic<uint8_t> m_current_update_frame {0};
    std::atomic<uint8_t> m_display_frame {0};
    std::array<uint16_t*, 3> m_frame_buffers;

    std::function<void()> m_on_state_changed {[]() {}};

    std::vector<hal::ITouch::Data> m_data_vector;
    etl::queue_spsc_atomic<hal::ITouch::Data, 16> m_touch_data_queue;
};
