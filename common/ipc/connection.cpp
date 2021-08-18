#include "connection.h"
#include "commandfactory.h"
#include <QTimer>

namespace IPC
{

Connection::Connection(QLocalSocket *localSocket) : localSocket_(localSocket), bytesWrittingInProgress_(0)
{
    QObject::connect(localSocket_, SIGNAL(disconnected()), SLOT(onSocketDisconnected()));
    QObject::connect(localSocket_, SIGNAL(bytesWritten(qint64)), SLOT(onSocketBytesWritten(qint64)));
    QObject::connect(localSocket_, SIGNAL(readyRead()), SLOT(onReadyRead()));
    QObject::connect(localSocket_, SIGNAL(error(QLocalSocket::LocalSocketError)), SLOT(onSocketError(QLocalSocket::LocalSocketError)));
}

Connection::Connection() : localSocket_(NULL), bytesWrittingInProgress_(0)
{
}

Connection::~Connection()
{
    safeDeleteSocket();
}

void Connection::connect()
{
    safeDeleteSocket();
    localSocket_ = new QLocalSocket;
    QObject::connect(localSocket_, SIGNAL(connected()), SLOT(onSocketConnected()));
    QObject::connect(localSocket_, SIGNAL(disconnected()), SLOT(onSocketDisconnected()));
    QObject::connect(localSocket_, SIGNAL(bytesWritten(qint64)), SLOT(onSocketBytesWritten(qint64)));
    QObject::connect(localSocket_, SIGNAL(readyRead()), SLOT(onReadyRead()));
    QObject::connect(localSocket_, SIGNAL(error(QLocalSocket::LocalSocketError)), SLOT(onSocketError(QLocalSocket::LocalSocketError)));
    localSocket_->connectToServer("Windscribe8rM7bza5OR");
}

void Connection::close()
{
    Q_ASSERT(localSocket_ != NULL);
    safeDeleteSocket();
}

void Connection::sendCommand(const Command &commandl)
{
    Q_ASSERT(localSocket_ != NULL);

    // command structure
    // 1) (int) size of protobuf message in bytes
    // 2) (int) size of message string id in bytes
    // 3) (string) message string id
    // 4) (byte array) body of protobuf message

    std::vector<char> buf = commandl.getData();
    int sizeOfBuf = buf.size();
    std::string strId = commandl.getStringId();
    int sizeOfStringId = strId.length();

    Q_ASSERT(sizeOfStringId > 0);

    bool isWriteBufIsEmpty = writeBuf_.isEmpty();

    writeBuf_.append((const char *)&sizeOfBuf, sizeof(sizeOfBuf));
    writeBuf_.append((const char *)&sizeOfStringId, sizeof(sizeOfStringId));
    writeBuf_.append(strId.c_str(), sizeOfStringId);
    if (sizeOfBuf > 0)
    {
        writeBuf_.append(&buf[0], sizeOfBuf);
    }
    if (isWriteBufIsEmpty)
    {
        qint64 bytesWritten = localSocket_->write(writeBuf_);
        if (bytesWritten == -1)
        {
            emit stateChanged(CONNECTION_DISCONNECTED, this);
        }
        else
        {
            bytesWrittingInProgress_ += bytesWritten;
            writeBuf_.remove(0, bytesWritten);
        }
    }
}

void Connection::onSocketConnected()
{
    emit stateChanged(CONNECTION_CONNECTED, this);
}

void Connection::onSocketDisconnected()
{
    emit stateChanged(CONNECTION_DISCONNECTED, this);
}

void Connection::onSocketBytesWritten(qint64 bytes)
{
    bytesWrittingInProgress_ -= bytes;

    if (!writeBuf_.isEmpty())
    {
        qint64 bytesWritten = localSocket_->write(writeBuf_);
        if (bytesWritten == -1)
        {
           emit stateChanged(CONNECTION_DISCONNECTED, this);
        }
        else
        {
            bytesWrittingInProgress_ += bytesWritten;
            writeBuf_.remove(0, bytesWritten);
        }
    }
    else if (bytesWrittingInProgress_ == 0)
    {
        emit allWritten(this);
    }
}

void Connection::onReadyRead()
{
    readBuf_.append(localSocket_->readAll());
    while (canReadCommand())
    {
        Command *cmd = readCommand();
        emit newCommand(cmd, this);
    }
}

void Connection::onSocketError(QLocalSocket::LocalSocketError /*socketError*/)
{
    emit stateChanged(CONNECTION_ERROR, this);
}

bool Connection::canReadCommand()
{
    if (readBuf_.size() > (int)(sizeof(int) * 2))
    {
        int sizeOfCmd;
        int sizeOfId;
        memcpy(&sizeOfCmd, readBuf_.data(), sizeof(int));
        memcpy(&sizeOfId, readBuf_.data() + sizeof(int), sizeof(int));

        if (readBuf_.size() >= (int)(sizeof(int) * 2 + sizeOfCmd + sizeOfId))
        {
            return true;
        }
    }
    return false;
}

Command *Connection::readCommand()
{
    int sizeOfCmd;
    int sizeOfId;
    memcpy(&sizeOfCmd, readBuf_.data(), sizeof(int));
    memcpy(&sizeOfId, readBuf_.data() + sizeof(int), sizeof(int));

    std::string strId(readBuf_.data() + sizeof(int) * 2, sizeOfId);

    Command *cmd = CommandFactory::makeCommand(strId, readBuf_.data() + sizeof(int) * 2 + sizeOfId, sizeOfCmd);
    readBuf_.remove(0, sizeof(int) * 2 + sizeOfId + sizeOfCmd);
    return cmd;
}

void Connection::safeDeleteSocket()
{
    if (localSocket_)
    {
        localSocket_->deleteLater();
        localSocket_ = NULL;
    }
}

} // namespace IPC