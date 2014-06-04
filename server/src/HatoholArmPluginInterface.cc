/*
 * Copyright (C) 2014 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

#include <MutexLock.h>
#include <SmartBuffer.h>
#include <Reaper.h>
#include <qpid/messaging/Address.h>
#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include "HatoholArmPluginInterface.h"
#include "HatoholArmPluginGate.h"
#include "HatoholException.h"

using namespace std;
using namespace mlpl;
using namespace qpid::messaging;

struct HatoholArmPluginInterface::PrivateContext {
	HatoholArmPluginInterface *hapi;
	bool       workInServer;
	string     queueAddr;
	Connection connection;
	Session    session;
	Sender     sender;
	Receiver   receiver;
	Message   *currMessage;
	CommandHandlerMap receiveHandlerMap;

	PrivateContext(HatoholArmPluginInterface *_hapi,
	               const string &_queueAddr,
	               const bool &_workInServer)
	: hapi(_hapi),
	  workInServer(_workInServer),
	  queueAddr(_queueAddr),
	  currMessage(NULL),
	  connected(false)
	{
	}

	virtual ~PrivateContext()
	{
		disconnect();
	}

	void setQueueAddress(const string &_queueAddr)
	{
		queueAddr = _queueAddr;
	}

	void connect(void)
	{
		const string brokerUrl =
		   HatoholArmPluginGate::DEFAULT_BROKER_URL;
		const string connectionOptions;
		connectionLock.lock();
		Reaper<MutexLock> unlocker(&connectionLock, MutexLock::unlock);
		connection = Connection(brokerUrl, connectionOptions);
		connection.open();
		session = connection.createSession();

		string queueAddrS = queueAddr + "-S"; // Plugin -> Hatohol
		string queueAddrT = queueAddr + "-T"; // Plugin <- Hatohol
		if (workInServer) {
			queueAddrS += "; {create: always}";
			queueAddrT += "; {create: always}";
			sender = session.createSender(queueAddrT);
			receiver = session.createReceiver(queueAddrS);
		} else {
			sender = session.createSender(queueAddrS);
			receiver = session.createReceiver(queueAddrT);
		}
		connected = true;
		hapi->onConnected(connection);
		hapi->onSessionChanged(&session);
	}

	void disconnect(void)
	{
		connectionLock.lock();
		Reaper<MutexLock> unlocker(&connectionLock, MutexLock::unlock);
		if (!connected)
			return;
		try {
			session.sync();
			session.close();
			connection.close();
		} catch (...) {
		}
		connected = false;
		hapi->onSessionChanged(NULL);
	}

	void acknowledge(void)
	{
		Reaper<MutexLock> unlocker(&connectionLock, MutexLock::unlock);
		connectionLock.lock();
		if (!connected)
			return;
		session.acknowledge();
	}

private:
	bool       connected;
	MutexLock  connectionLock;
};

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
HatoholArmPluginInterface::HatoholArmPluginInterface(
  const string &queueAddr, const bool &workInServer)
: m_ctx(NULL)
{
	m_ctx = new PrivateContext(this, queueAddr, workInServer);
}

HatoholArmPluginInterface::~HatoholArmPluginInterface()
{
	exitSync();
	if (m_ctx)
		delete m_ctx;
}

void HatoholArmPluginInterface::setQueueAddress(const string &queueAddr)
{
	m_ctx->setQueueAddress(queueAddr);
}

void HatoholArmPluginInterface::send(const string &message)
{
	Message request;
	request.setContent(message);
	m_ctx->sender.send(request);
}

void HatoholArmPluginInterface::send(const SmartBuffer &smbuf)
{
	Message request;
	request.setContent(smbuf.getPointer<char>(0), smbuf.size());
	m_ctx->sender.send(request);
}

void HatoholArmPluginInterface::reply(const mlpl::SmartBuffer &replyBuf)
{
	HATOHOL_ASSERT(m_ctx->currMessage,
	               "This object doesn't have a current message.\n");
	const Address& address = m_ctx->currMessage->getReplyTo();
	if (!address) {
		MLPL_ERR("No return address.\n");
		m_ctx->session.reject(*m_ctx->currMessage);
		return;
	}
	Message reply;
	reply.setContent(replyBuf.getPointer<char>(0), replyBuf.size());
	Sender sender = m_ctx->session.createSender(address);
	sender.send(reply);
}

void HatoholArmPluginInterface::replyError(const HapiResponseCode &code)
{
	SmartBuffer resBuf(sizeof(HapiResponseHeader));
	HapiResponseHeader *header = resBuf.getPointer<HapiResponseHeader>();
	header->type = HAPI_MSG_RESPONSE;
	header->code = code;
	reply(resBuf);
}

void HatoholArmPluginInterface::exitSync(void)
{
	requestExit();
	m_ctx->disconnect();
	HatoholThreadBase::exitSync();
}

void HatoholArmPluginInterface::registCommandHandler(
  const HapiCommandCode &code, CommandHandler handler)
{
	m_ctx->receiveHandlerMap[code] = handler;
}

const string &HatoholArmPluginInterface::getQueueAddress(void) const
{
	return m_ctx->queueAddr;
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------
gpointer HatoholArmPluginInterface::mainThread(HatoholThreadArg *arg)
{
	m_ctx->connect();
	while (!isExitRequested()) {
		Message message;
		m_ctx->receiver.fetch(message);
		if (isExitRequested()) 
			break;
		SmartBuffer sbuf;
		load(sbuf, message);
		sbuf.resetIndex();
		m_ctx->currMessage = &message;
		onReceived(sbuf);
		m_ctx->currMessage = NULL;
		m_ctx->acknowledge();
	};
	m_ctx->disconnect();
	return NULL;
}

void HatoholArmPluginInterface::onConnected(Connection &conn)
{
}

void HatoholArmPluginInterface::onSessionChanged(Session *session)
{
}

void HatoholArmPluginInterface::onReceived(mlpl::SmartBuffer &smbuf)
{
	if (smbuf.size() < sizeof(HapiCommandHeader)) {
		MLPL_ERR("Got a too small packet: %zd.\n", smbuf.size());
		replyError(HAPI_RES_INVALID_HEADER);
		return;
	}

	const uint16_t type = smbuf.getValue<uint16_t>();
	switch (type) {
	case HAPI_MSG_COMMAND:
		parseCommand(smbuf.getPointer<HapiCommandHeader>(), smbuf);
		break;
	case HAPI_MSG_RESPONSE:
		parseResponse(smbuf.getPointer<HapiResponseHeader>(), smbuf);
		break;
	default:
		MLPL_ERR("Got an invalid message type: %d.\n", type);
		replyError(HAPI_RES_INVALID_HEADER);
	}
}

void HatoholArmPluginInterface::parseCommand(
  const HapiCommandHeader *header, mlpl::SmartBuffer &cmdBuf)
{
	CommandHandlerMapConstIterator it =
	  m_ctx->receiveHandlerMap.find(header->code);
	if (it == m_ctx->receiveHandlerMap.end()) {
		replyError(HAPI_RES_UNKNOWN_CODE);
		return;
	}
	CommandHandler handler = it->second;
	(this->*handler)(header);
}

void HatoholArmPluginInterface::parseResponse(
  const HapiResponseHeader *header, mlpl::SmartBuffer &resBuf)
{
	onGotResponse(header, resBuf);
}

void HatoholArmPluginInterface::onGotError(
  const HatoholArmPluginError &hapError)
{
	THROW_HATOHOL_EXCEPTION("Got error: %d\n", hapError.code);
}

void HatoholArmPluginInterface::onGotResponse(
  const HapiResponseHeader *header, mlpl::SmartBuffer &resBuf)
{
}

void HatoholArmPluginInterface::load(SmartBuffer &sbuf, const Message &message)
{
	sbuf.addEx(message.getContentPtr(), message.getContentSize());
}

const HapiResponseHeader *HatoholArmPluginInterface::getResponseHeader(
  SmartBuffer &resBuf) throw(HatoholException)
{
	if (resBuf.size() < sizeof(HapiResponseHeader)) {
		THROW_HATOHOL_EXCEPTION("Invalid message size: %zd\n",
		                        sizeof(HapiResponseHeader));
	}
	const HapiResponseHeader *header =
	  resBuf.getPointer<HapiResponseHeader>(0);
	if (header->code != HAPI_RES_OK) {
		THROW_HATOHOL_EXCEPTION("Bad response code: %d\n",
		                        header->code);
	}
	return header;
}