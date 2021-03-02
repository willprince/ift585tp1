#include "PhysicalLayer.h"

#include "LinkLayer.h"
#include "../NetworkDriver.h"
#include "../../../DataStructures/DataBuffer.h"
#include "../../../General/Configuration.h"
#include "../../../General/Logger.h"

#include <iostream>

std::unique_ptr<DataEncoderDecoder> DataEncoderDecoder::CreateEncoderDecoder(const Configuration& config)
{
    int encoderDecoderConfig = config.get(Configuration::PHYSICAL_LAYER_DATA_ENCODER_DECODER);
    if (encoderDecoderConfig == 1)
    {
        return std::make_unique<HammingDataEncoderDecoder>();
    }
    else if (encoderDecoderConfig == 2)
    {
        return std::make_unique<CRCDataEncoderDecoder>();
    }
    else
    {
        return std::make_unique<PassthroughDataEncoderDecoder>();
    }
}


DynamicDataBuffer PassthroughDataEncoderDecoder::encode(const DynamicDataBuffer& data) const
{
    return data;
}

std::pair<bool, DynamicDataBuffer> PassthroughDataEncoderDecoder::decode(const DynamicDataBuffer& data) const
{
    return std::pair<bool, DynamicDataBuffer>(true, data);
}


//===================================================================
// Hamming Encoder decoder implementation
//===================================================================
HammingDataEncoderDecoder::HammingDataEncoderDecoder()
{
}

HammingDataEncoderDecoder::~HammingDataEncoderDecoder()
{
}

DynamicDataBuffer HammingDataEncoderDecoder::encode(const DynamicDataBuffer& data) const
{
    // À faire TP1
    return data;
}

std::pair<bool, DynamicDataBuffer> HammingDataEncoderDecoder::decode(const DynamicDataBuffer& data) const
{
    // À faire TP1
    return std::pair<bool, DynamicDataBuffer>(true, data);
}



//===================================================================
// CRC Encoder decoder implementation
//===================================================================
CRCDataEncoderDecoder::CRCDataEncoderDecoder()
{
}

CRCDataEncoderDecoder::~CRCDataEncoderDecoder()
{
}

DynamicDataBuffer CRCDataEncoderDecoder::encode(const DynamicDataBuffer& data) const
{
    // À faire TP1
    return data;
}

std::pair<bool, DynamicDataBuffer> CRCDataEncoderDecoder::decode(const DynamicDataBuffer& data) const
{
    // À faire TP1
    return std::pair<bool, DynamicDataBuffer>(true, data);
}


//===================================================================
// Network Driver Physical layer implementation
//===================================================================
PhysicalLayer::PhysicalLayer(NetworkDriver* driver, const Configuration& config)
    : m_driver(driver)
    , m_sendingBuffer(config.get(Configuration::PHYSICAL_LAYER_SENDING_BUFFER_SIZE))
    , m_receivingBuffer(config.get(Configuration::PHYSICAL_LAYER_RECEIVING_BUFFER_SIZE))
    , m_stopReceiving(true)
    , m_stopSending(true)
{
    m_encoderDecoder = DataEncoderDecoder::CreateEncoderDecoder(config);
}

PhysicalLayer::~PhysicalLayer()
{
    stop();
    m_driver = nullptr;
}

void PhysicalLayer::start()
{
    stop();

    start_receiving();
    start_sending();
}

void PhysicalLayer::stop()
{
    stop_receiving();
    stop_sending();
}

bool PhysicalLayer::dataReceived() const
{
    return m_receivingBuffer.canRead<DynamicDataBuffer>();
}

DynamicDataBuffer PhysicalLayer::encode(const DynamicDataBuffer& data) const
{
    return m_encoderDecoder->encode(data);
}

std::pair<bool, DynamicDataBuffer> PhysicalLayer::decode(const DynamicDataBuffer& data) const
{
    return m_encoderDecoder->decode(data);
}

void PhysicalLayer::start_receiving()
{
    m_stopReceiving = false;
    m_receivingThread = std::thread(&PhysicalLayer::receiving, this);
}

void PhysicalLayer::stop_receiving()
{
    m_stopReceiving = true;
    if (m_receivingThread.joinable())
    {
        m_receivingThread.join();
    }
}

void PhysicalLayer::start_sending()
{
    m_stopSending = false;
    m_sendingThread = std::thread(&PhysicalLayer::sending, this);
}

void PhysicalLayer::stop_sending()
{
    m_stopSending = true;
    if (m_sendingThread.joinable())
    {
        m_sendingThread.join();
    }
}


void PhysicalLayer::receiving()
{
    while (!m_stopReceiving)
    {
        if (dataReceived())
        {
            DynamicDataBuffer data = m_receivingBuffer.pop<DynamicDataBuffer>();
            std::pair<bool, DynamicDataBuffer> dataBuffer = decode(data);
            if (dataBuffer.first) // Les donnees recues sont correctes et peuvent etre utilisees
            {
                Frame frame = Buffering::unpack<Frame>(dataBuffer.second);
                m_driver->getLinkLayer().receiveData(frame);
            }
            else
            {
                // Les donnees recues sont corrompues et doivent etre delaissees
                Logger log(std::cout);
                log << m_driver->getMACAddress() << " : Corrupted data received" << std::endl;
            }
        }
    }
}

void PhysicalLayer::sending()
{
    while (!m_stopSending)
    {
        if (m_driver->getLinkLayer().dataReady())
        {
            Frame dataFrame = m_driver->getLinkLayer().getNextData();
            DynamicDataBuffer buffer = encode(Buffering::pack<Frame>(dataFrame));
            sendData(buffer);
        }
    }
}

void PhysicalLayer::receiveData(const DynamicDataBuffer& data)
{
    // Si le buffer est plein, on fait juste oublier les octets recus du cable
    // Sinon, on ajoute les octets au buffer
    if (m_receivingBuffer.canWrite<DynamicDataBuffer>(data))
    {
        m_receivingBuffer.push(data);
    }
    else
    {
        Logger log(std::cout);
        log << m_driver->getMACAddress() << " : Physical reception buffer full... data discarded" << std::endl;
    }
}

void PhysicalLayer::sendData(DynamicDataBuffer data)
{
    // Envoit une suite d'octet sur le cable connecte
    m_driver->sendToCard(data);
}