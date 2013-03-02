#ifndef STATIONSETTINGS_H
#define STATIONSETTINGS_H

#include <QObject>

class StationSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString callsign READ callsign WRITE setCallsign NOTIFY callsignChanged)
    Q_PROPERTY(float lat READ lat WRITE setLat NOTIFY latChanged)
    Q_PROPERTY(float lon READ lon WRITE setLon NOTIFY lonChanged)
    Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)

    QString m_callsign;

    float m_lat;

    float m_lon;

public:
    explicit StationSettings(QObject *parent = 0);

    enum PositionAmbiguity {
        NONE,
        point11,
        onePoint15,

    };

QString callsign() const
{
    return m_callsign;
}

float lat() const
{
    return m_lat;
}

float lon() const
{
    return m_lon;
}

signals:

void callsignChanged(QString arg);

void latChanged(float arg);

void lonChanged(float arg);

public slots:

void setCallsign(QString arg)
{
    if (m_callsign != arg) {
        m_callsign = arg;
        emit callsignChanged(arg);
    }
}
void setLat(float arg)
{
    if (m_lat != arg) {
        m_lat = arg;
        emit latChanged(arg);
    }
}
void setLon(float arg)
{
    if (m_lon != arg) {
        m_lon = arg;
        emit lonChanged(arg);
    }
}
};

#endif // STATIONSETTINGS_H
