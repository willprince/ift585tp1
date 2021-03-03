#include "LinkLayer.h"
#include "../NetworkDriver.h"

#include "../../../General/Configuration.h"
#include "../../../General/Logger.h"

#include <iostream>
#include <map>


LinkLayer::LinkLayer(NetworkDriver* driver, const Configuration& config)
    : m_driver(driver)
    , m_address(config)
    , m_receivingQueue(config.get(Configuration::LINK_LAYER_RECEIVING_BUFFER_SIZE))
    , m_sendingQueue(config.get(Configuration::LINK_LAYER_SENDING_BUFFER_SIZE))
    , m_maximumBufferedFrameCount(config.get(Configuration::LINK_LAYER_MAXIMUM_BUFFERED_FRAME))
    , m_transmissionTimeout(config.get(Configuration::LINK_LAYER_TIMEOUT))
    , m_executeReceiving(false)
    , m_executeSending(false)
{
    m_maximumSequence = m_maximumBufferedFrameCount * 2 - 1;
    m_ackTimeout = m_transmissionTimeout / 4;
    m_timers = std::make_unique<Timer>();
}

LinkLayer::~LinkLayer()
{
    stop();
    m_driver = nullptr;
}

const MACAddress& LinkLayer::getMACAddress() const
{
    return m_address;
}

// Demarre les fils d'execution pour l'envoi et la reception des trames
void LinkLayer::start()
{
    stop();

    m_timers->start();

    m_executeReceiving = true;
    m_receiverThread = std::thread(&LinkLayer::receiverCallback, this);

    m_executeSending = true;
    m_senderThread = std::thread(&LinkLayer::senderCallback, this);
}

// Arrete les fils d'execution pour l'envoi et la reception des trames
void LinkLayer::stop()
{
    m_timers->stop();

    m_executeReceiving = false;
    if (m_receiverThread.joinable())
    {
        m_receiverThread.join();
    }

    m_executeSending = false;
    if (m_senderThread.joinable())
    {
        m_senderThread.join();
    }
}

// Indique vrai si on peut envoyer des donnees dans le buffer de sortie, faux si le buffer est plein
bool LinkLayer::canSendData(const Frame& data) const
{
    return m_sendingQueue.canWrite<Frame>(data);
}

// Indique vrai si des donnees sont disponibles dans le buffer d'entree, faux s'il n'y a rien
bool LinkLayer::dataReceived() const
{
    return m_receivingQueue.canRead<Frame>();
}

// Indique vrai s'il y a des donnees dans le buffer de sortie
bool LinkLayer::dataReady() const
{
    return m_sendingQueue.canRead<Frame>();
}

// Recupere la prochaine donnee du buffer de sortie
Frame LinkLayer::getNextData()
{
    return m_sendingQueue.pop<Frame>();
}

// Envoit une trame dans le buffer de sortie
// Cette fonction retourne faux si la trame n'a pas ete envoyee. Ce cas arrive seulement si le programme veut se terminer.
// Fait de l'attente active jusqu'a ce qu'il puisse envoyer la trame sinon.
bool LinkLayer::sendFrame(const Frame& frame)
{
    while (m_executeSending)
    {
        if (canSendData(frame))
        {
            Logger log(std::cout);

            if (frame.Size == FrameType::NAK)
            {
                log << "SENDER  :" << frame.Source << " : Sending NAK  to " << frame.Destination << " : " << frame.Ack << std::endl;
				m_sendingQueue.push(frame);
            }
            else if (frame.Size == FrameType::ACK)
            {
                log << "SENDER  :" << frame.Source << " : Sending ACK  to " << frame.Destination << " : " << frame.Ack << std::endl;
				m_sendingQueue.push(frame);
				startAckTimer(-1, frame.Ack);
            }
            else
            {
                log << "SENDER  :" << frame.Source << " : Sending DATA to " << frame.Destination << " : " << frame.NumberSequence << std::endl;
				m_sendingQueue.push(frame);
				startTimeoutTimer(frame.NumberSequence);
            }
            
            return true;
        }
    }
    return false;
}

// Recupere le prochain evenement de communication a gerer pour l'envoi de donnees
LinkLayer::Event LinkLayer::getNextSendingEvent()
{
    std::lock_guard<std::mutex> lock(m_sendEventMutex);
    if (m_sendingEventQueue.size() > 0)
    {
        Event ev = m_sendingEventQueue.front();
        m_sendingEventQueue.pop();
        return ev;
    }
    return Event::Invalid();
}

// Recupere le prochain evenement de communication a gerer pour la reception de donnees
LinkLayer::Event LinkLayer::getNextReceivingEvent()
{
    std::lock_guard<std::mutex> lock(m_receiveEventMutex);
    if (m_receivingEventQueue.size() > 0)
    {
        Event ev = m_receivingEventQueue.front();
        m_receivingEventQueue.pop();
        return ev;
    }
    return Event::Invalid();
}

// Indique si la valeur est comprise entre first et last de facon circulaire
bool LinkLayer::between(NumberSequence value, NumberSequence first, NumberSequence last) const
{
    // Value is between first and last, circular style
    return ((first <= value) && (value < last)) || ((last < first) && (first <= value)) || ((value < last) && (last < first));
}

// Envoit un evenement de communication pour indiquer a l'envoi d'envoyer un ACK
// L'evenement contiendra l'adresse a qui il faut envoyer un ACK et le numero du ACK
void LinkLayer::sendAck(const MACAddress& to, NumberSequence ackNumber)
{
    Event ev = Event::Invalid();
    ev.Type = EventType::SEND_ACK_REQUEST;
    ev.Number = ackNumber;
    ev.Address = to;
    std::lock_guard<std::mutex> lock(m_sendEventMutex);
    m_sendingEventQueue.push(ev);
}

// Envoit un evenement de communication pour indiquer a l'envoi d'envoyer un NAK
// L'evenement contiendra l'adresse a qui il faut envoyer un ACK et le numero du NAK
void LinkLayer::sendNak(const MACAddress& to, NumberSequence nakNumber)
{
    Event ev = Event::Invalid();
    ev.Type = EventType::SEND_NAK_REQUEST;
    ev.Number = nakNumber;
    ev.Address = to;
    std::lock_guard<std::mutex> lock(m_sendEventMutex);
    m_sendingEventQueue.push(ev);
}

// Envoit un evenement de communication pour indiquer a l'envoi qu'on a recu une trame avec potentiellement un ACK (piggybacking)
// L'evenement contiendra l'adresse d'ou provient l'information, le numero du ACK et le prochain ACK qu'on devrait nous-meme envoyer (pour le piggybacking)
void LinkLayer::notifyACK(const Frame& frame, NumberSequence piggybackAck)
{
    Event ev = Event::Invalid();
    ev.Type = EventType::ACK_RECEIVED;
    ev.Number = frame.Ack;
    ev.Address = frame.Source;
    ev.Next = piggybackAck;
    std::lock_guard<std::mutex> lock(m_sendEventMutex);
    m_sendingEventQueue.push(ev);
}

// Envoit un evenement de communication pour indiquer a l'envoi qu'on a recu un NAK
// L'evenement contiendra l'adresse d'ou provient l'information et le numero du NAK
void LinkLayer::notifyNAK(const Frame& frame)
{
    Event ev = Event::Invalid();
    ev.Type = EventType::NAK_RECEIVED;
    ev.Number = frame.Ack;
    ev.Address = frame.Source;
    std::lock_guard<std::mutex> lock(m_sendEventMutex);
    m_sendingEventQueue.push(ev);
}

// Envoit un evenement de communication pour indiquer au recepteur qu'on a atteint un timeout pour un ACK
// L'evenement contiendra le numero du Timer qui est arrive a echeance et le numero de la trame associe au Timer
void LinkLayer::ackTimeout(size_t timerID, NumberSequence numberData)
{
    Event ev;
    ev.Type = EventType::ACK_TIMEOUT;
    ev.Number = numberData;
    ev.TimerID = timerID;
    std::lock_guard<std::mutex> guard(m_receiveEventMutex);
    m_receivingEventQueue.push(ev);
}

// Envoit un evenement de communication pour indiquer a l'envoi qu'on n'a aps recu de reponse a un envoit et qu'il faut reenvoyer la trame
// L'evenement contiendra le numero de la trame et le numero du Timer qui est arrive a echeance
void LinkLayer::transmissionTimeout(size_t timerID, NumberSequence numberData)
{
    Event ev;
    ev.Type = EventType::SEND_TIMEOUT;
    ev.Number = numberData;
    ev.TimerID = timerID;
    std::lock_guard<std::mutex> guard(m_sendEventMutex);
    m_sendingEventQueue.push(ev);
}


// Demarre un nouveau Timer d'attente pour l'envoi a nouveau d'une trame
// La methode retourne le numero du Timer qui vient d'etre demarre. Cette valeur doit etre garder pour pouvoir retrouver quel evenement y sera associe lorsque
// le timer arrivera a echeance
size_t LinkLayer::startTimeoutTimer(NumberSequence numberData)
{
    return m_timers->addTimer(m_transmissionTimeout, std::bind(&LinkLayer::transmissionTimeout, this, std::placeholders::_1, std::placeholders::_2), numberData);
}

// Demarre un nouveau Timer pour l'envoi d'un ACK, pour garantir un niveau de service minimal dans une communication unidirectionnelle
// Retourne le numero du Timer qui vient d'etre demarre. La methode prend en parametre le numero actuel du Timer de ACK afin de le redemarrer s'il existe encore
size_t LinkLayer::startAckTimer(size_t existingTimerID, NumberSequence ackNumber)
{
    if (!m_timers->restartTimer(existingTimerID, ackNumber))
    {
        return m_timers->addTimer(m_ackTimeout, std::bind(&LinkLayer::ackTimeout, this, std::placeholders::_1, std::placeholders::_2), ackNumber);
    }
    return existingTimerID;
}

// Envoit un evenement de communication pour indiquer a la fonction de reception qu'une ACK vient d'etre envoyer (en piggybacking) et 
// qu'on n'a pas besoin d'envoyer le ACK en attente
void LinkLayer::notifyStopAckTimers(const MACAddress& to)
{
    Event ev;
    ev.Type = EventType::STOP_ACK_TIMER_REQUEST;
    ev.Address = to;
    std::lock_guard<std::mutex> guard(m_receiveEventMutex);
    m_receivingEventQueue.push(ev);
}

// Arrete le Timer de ACK avec le TimerID specifie
void LinkLayer::stopAckTimer(size_t timerID)
{
    m_timers->removeTimer(timerID);
}

// Indique s'il y a assez de place dans le buffer de reception pour recevoir des donnees de la couche physique
bool LinkLayer::canReceiveDataFromPhysicalLayer(const Frame& data) const
{
    return m_receivingQueue.canWrite<Frame>(data);
}

// Recoit des donnees de la couche physique
void LinkLayer::receiveData(Frame data)
{
    // Si la couche est pleine, la trame est perdue. Elle devra etre envoye a nouveau par l'emetteur
    if (canReceiveDataFromPhysicalLayer(data))
    {
        // Est-ce que la trame reçue est pour nous?
        if (data.Destination == m_address || data.Destination.isMulticast())
        {			
            m_receivingQueue.push(data);
        }
    }
}

// Fonction qui retourne l'adresse MAC du destinataire d'un packet particulier de la couche Reseau.
// Dans la realite, cette fonction ferait un lookup dans une table a partir des adresses IP pour recupere les addresse MAC.
// Ici, on utilise directement seulement les adresse MAC.
MACAddress LinkLayer::arp(const Packet& packet) const
{
    return packet.Destination;
}

// Fonction qui fait l'envoi des trames et qui gere la fenetre d'envoi
void LinkLayer::senderCallback()
{
	int NR_BUFS = (m_maximumSequence + 1) / 2;
	NumberSequence ack_expected = 0;
	NumberSequence next_frame_to_send = 0;
	Frame* out_buf = new Frame[NR_BUFS];
	NumberSequence nbuffered = 0;
	bool no_nak = true;

    while (m_executeSending)
    {
		Logger log(std::cout);
		// Check if there is an event
		Event next_sending_event = getNextSendingEvent();
		if (next_sending_event.Type == EventType::SEND_ACK_REQUEST) {
			
			Frame frame;
			frame.Destination = next_sending_event.Address;
			frame.Source = m_address;
			frame.NumberSequence = next_sending_event.Number;
			frame.Ack = next_sending_event.Number;
			frame.Size = FrameType::ACK;

			sendFrame(frame);
			
		}

		if (next_sending_event.Type == EventType::ACK_RECEIVED) {

			log << "ACK RECEIVED: " << next_sending_event.Number << std::endl;
			
			while (between(next_sending_event.Number, ack_expected, next_frame_to_send)) {
				nbuffered--;
				stopAckTimer(next_sending_event.TimerID);
				ack_expected++;
				log << "ARGG " << ack_expected << std::endl;
			}
			
		}

		if (next_sending_event.Type == EventType::SEND_TIMEOUT) {

			log << "SEND TIMEOUT: " << next_sending_event.Number << std::endl;
			
			int n = 0;

			log << "SENDING AGAIN: " << out_buf[n].NumberSequence << std::endl;

			while(n < NR_BUFS){
				
				if (next_sending_event.Number == out_buf[n].NumberSequence) {

					sendFrame(out_buf[n]);
				}
				n++;
			}

		}

		

		// S'il n'y a pas d'event
		if (next_sending_event.Type == EventType::INVALID) {

			// On valid si l'on peux envoyé une nouvelle trame
			if (nbuffered < NR_BUFS) {

				// On envoie une trame
				if (m_driver->getNetworkLayer().dataReady())
				{

					Packet packet = m_driver->getNetworkLayer().getNextData();
					

					for (int i = 0; i < NR_BUFS - 1; i++) {
						out_buf[i] = out_buf[i + 1];
					}

					
					
					Frame frame;
					frame.Destination = arp(packet);
					frame.Source = m_address;
					frame.NumberSequence = next_frame_to_send;
					frame.Data = Buffering::pack<Packet>(packet);
					frame.Size = (uint16_t)frame.Data.size();

					out_buf[NR_BUFS - 1] = frame;

					nbuffered++;
					next_frame_to_send++;

					// On envoit la trame. Si la trame n'est pas envoye, c'est qu'on veut arreter le simulateur
					if (!sendFrame(frame))
					{
						return;
					}
				}
			}
			
		} 
    }
}

// Fonction qui s'occupe de la reception des trames
void LinkLayer::receiverCallback()
{
	const int NR_BUFS = (m_maximumSequence + 1) / 2;
	NumberSequence frame_expected = 0;
	NumberSequence too_far = NR_BUFS;
	NumberSequence* in_buf = new NumberSequence[NR_BUFS];
	bool* arrived = new bool[NR_BUFS];
	bool no_nak = true;

	for (int i = 0; i < NR_BUFS; i++) { arrived[i] = false; }

    // Passtrough
    while (m_executeReceiving)
    {        
		Logger log(std::cout);
		// Check if there is an event
		Event next_receiving_event = getNextSendingEvent();

		if (next_receiving_event.Type == EventType::SEND_TIMEOUT) {
			log << "RECEIVER TIMEOUT: " << next_receiving_event.Number << std::endl;
			Frame frame;
			frame.Destination = next_receiving_event.Address;
			frame.Source = m_address;
			frame.NumberSequence = next_receiving_event.Number;
			frame.Ack = next_receiving_event.Number;
			frame.Size = FrameType::ACK;
			notifyACK(frame, frame.Ack);

		}

		// S'il n'y a pas d'event
		if (next_receiving_event.Type == EventType::INVALID) {
			if (m_receivingQueue.canRead<Frame>())
			{
				Frame frame = m_receivingQueue.pop<Frame>();

				if (frame.Size == FrameType::NAK)
				{
					log << "RECEIVER: " << frame.Destination << " : received a NAK  from " << frame.Source << " : " << frame.Ack << std::endl;
				}
				else if (frame.Size == FrameType::ACK)
				{
					log << "RECEIVER: " << frame.Destination << " : received a ACK  from " << frame.Source << " : " << frame.Ack << std::endl;
					notifyACK(frame, frame.Ack);
					startAckTimer(-1, frame.Ack);
				}
				else
				{
					log << "RECEIVER: " << frame.Destination << " : received DATA from " << frame.Source << " : " << frame.NumberSequence << std::endl;
					if ((frame.NumberSequence != frame_expected) && no_nak) {

						log << "unexpected frame receive: " << frame.NumberSequence << " sending NAK" << std::endl;
						sendNak(frame.Source, frame.NumberSequence);
					}

					if (between( frame.NumberSequence, frame_expected, too_far) && (arrived[frame.NumberSequence % NR_BUFS] == false)) {

						arrived[frame.NumberSequence % NR_BUFS] = true;
						in_buf[frame.NumberSequence % NR_BUFS] = frame.NumberSequence;

						while (arrived[frame_expected % NR_BUFS]) {
							m_driver->getNetworkLayer().receiveData(Buffering::unpack<Packet>(frame.Data));
							no_nak = true;
							arrived[frame_expected % NR_BUFS] = false;
							frame_expected++;
							too_far++;
							sendAck(frame.Source, frame.NumberSequence);
							startTimeoutTimer(frame.NumberSequence);
						}
					}

					//m_driver->getNetworkLayer().receiveData(Buffering::unpack<Packet>(frame.Data));
					//frame_expected++;;

				}

				/*m_driver->getNetworkLayer().receiveData(Buffering::unpack<Packet>(frame.Data));*/
			}
		}
        
    }
}
