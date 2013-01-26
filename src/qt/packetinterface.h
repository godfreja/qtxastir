#ifndef PACKETINTERFACE_H
#define PACKETINTERFACE_H

#include <QObject>

class PacketInterface : public QObject
{
    Q_OBJECT
public:
    explicit PacketInterface(QObject *parent = 0);
public:
    enum Device_Active {
        DEVICE_NOT_IN_USE,
        DEVICE_IN_USE
    };

    enum Device_Status {
        DEVICE_DOWN,
        DEVICE_UP,
        DEVICE_ERROR
    };

    virtual void start() = 0;
    virtual void stop() = 0;

    bool transmitAllowed();
    Device_Status deviceStatus();



signals:
    void packetReceived(QString data);
    void interfaceChangedState(PacketInterface::Device_Status state);


public slots:
    
protected:
    bool allowTransmit;
    Device_Status deviceState;
};

#endif // PACKETINTERFACE_H
