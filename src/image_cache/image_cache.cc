#include "image_cache.hh"

void
ImageCache::Insert(uint32_t key, uint8_t width, uint8_t height, std::span<const uint8_t> data)
{
    if (m_cache.find(key) == m_cache.end())
    {
        uint8_t index = m_images.size();

        m_images.push_back(std::make_unique<CachedImage>(data, width, height));
        m_cache[key] = index;

        std::ranges::for_each(m_listeners, [](auto s) { s->release(); });
    }
}

// Context: Some other thread (UI)
const CachedImage*
ImageCache::Lookup(uint32_t key) const
{
    auto it = m_cache.find(key);

    if (it == m_cache.end())
    {
        return nullptr;
    }

    return m_images[it->second].get();
}


std::unique_ptr<ListenerCookie>
ImageCache::ListenToChanges(os::binary_semaphore& semaphore)
{
    auto sema_p = &semaphore;

    m_listeners.push_back(sema_p);

    return std::make_unique<ListenerCookie>(
        [this, sema_p]() { std::ranges::remove(m_listeners, sema_p); });
}
