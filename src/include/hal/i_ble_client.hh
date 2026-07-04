#pragma once

#include "uuid_helper.hh"

#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace hal
{

class IBleClient
{
public:
    class ICharacteristic
    {
    public:
        using DataCallback = std::function<void(std::span<const uint8_t>)>;

        virtual ~ICharacteristic() = default;

        virtual Uuid128 GetUuid() const = 0;

        virtual bool Write(std::span<const uint8_t> data) = 0;

        virtual bool Read(DataCallback cb) = 0;

        virtual bool Subscribe(DataCallback cb) = 0;

        virtual void Unsubscribe() = 0;
    };

    class IService
    {
    public:
        virtual ~IService() = default;

        virtual Uuid128 GetUuid() const = 0;

        virtual std::vector<ICharacteristic*> GetCharacteristics() = 0;
    };

    class IPeer
    {
    public:
        virtual ~IPeer() = default;

        virtual bool IsConnected() const = 0;

        virtual void Disconnect() = 0;

        virtual std::vector<IService*> GetServices() = 0;
    };

    virtual ~IBleClient() = default;

    virtual void ScanForService(Uuid128Span service_uuid,
                                const std::function<void(std::unique_ptr<IPeer>)>& cb) = 0;
};

} // namespace hal
