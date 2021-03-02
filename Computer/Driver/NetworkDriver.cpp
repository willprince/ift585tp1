#include "NetworkDriver.h"
#include "../Hardware/NetworkInterfaceCard.h"
#include "../../General/Configuration.h"



NetworkDriver::NetworkDriver(NetworkInterfaceCard* hardware, const Configuration& config)
    : m_physicalLayer(std::make_unique<PhysicalLayer>(this, config))
    , m_linkLayer(std::make_unique<LinkLayer>(this, config))
    , m_networkLayer(std::make_unique<NetworkLayer>(this, config))
    , m_hardware(hardware)
{
    m_networkLayer->start();
    m_physicalLayer->start();
    m_linkLayer->start();
}

NetworkDriver::~NetworkDriver()
{
    m_networkLayer->stop();
    m_linkLayer->stop();
    m_physicalLayer->stop();
    m_hardware = nullptr;
}

PhysicalLayer& NetworkDriver::getPhysicalLayer()
{
    return *m_physicalLayer;
}

LinkLayer& NetworkDriver::getLinkLayer()
{
    return *m_linkLayer;
}

NetworkLayer& NetworkDriver::getNetworkLayer()
{
    return *m_networkLayer;
}

const MACAddress& NetworkDriver::getMACAddress() const
{
    return m_linkLayer->getMACAddress();
}

void NetworkDriver::sendToCard(DynamicDataBuffer& data)
{
    if (m_hardware)
    {
        m_hardware->send(data);
    }
}

void NetworkDriver::receiveFromCard(const DynamicDataBuffer& data)
{
    m_physicalLayer->receiveData(data);
}

void NetworkDriver::start_sending_process(const MACAddress& to, const std::string& fileName)
{
    m_networkLayer->startSending(to, fileName);
}

bool NetworkDriver::sendingFinished() const
{
    return m_networkLayer->currentSendingFinished();
}

