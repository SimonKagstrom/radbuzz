#pragma once

#include "uuid_helper.hh"

#include <functional>
#include <memory>
#include <span>

namespace hal
{

class IBleClient
{
public:
    class ICharacteristic
    {
    public:
        virtual ~ICharacteristic() = default;

        virtual Uuid16 GetUuid() const = 0;

        virtual void Write(std::span<const uint8_t> data) = 0;

        virtual void Read(std::function<void(std::span<const uint8_t>)>& cb) = 0;

        virtual void Subscribe(std::function<void(std::span<const uint8_t>)>& cb) = 0;
    };

    class IService
    {
    public:
        virtual ~IService() = default;

        virtual uint16_t GetUuid() const = 0;

        virtual std::vector<ICharacteristic&> GetCharacteristics() = 0;
    };

    class IPeer
    {
    public:
        virtual ~IPeer() = default;

        virtual bool Connect() = 0;

        virtual std::vector<IService&> GetServices() = 0;
    };

    virtual ~IBleClient() = default;

    virtual void ScanForService(Uuid16 service_uuid,
                                const std::function<void(std::unique_ptr<IPeer>)>& cb) = 0;
};

} // namespace hal
