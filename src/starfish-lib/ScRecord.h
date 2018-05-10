#pragma once

#include <QObject>
#include <QDate>
#include <QDomNode>


class ScRecord
{
public:
    enum {
        ValidGame = 0x001,
        ValidYear = 0x002,
        ValidSeason = 0x004,
        ValidEventName = 0x008,
        ValidStage = 0x010,
        ValidMatchDate = 0x020,
        ValidSides = 0x040,
        ValidRaces = 0x080,
        ValidUrl = 0x100,
        ValidMatchName = 0x200,
        ValidEventFullName = 0x400,
    };

    enum Game {
        GameBroodWar,
        GameSc2,
        GameOverwatch,
    };

    enum Race {
        RaceUnknown,
        RaceProtess,
        RaceTerran,
        RaceZerg,
    };

    QString eventFullName;
    QString eventName;
    QString stage;
    QString side1Name;
    QString side2Name;
    QString url;
    QString matchName;
    QDate matchDate;
    int side1Race;
    int side2Race;
    int year;
    int season;
    int game;
    int valid;

    ~ScRecord() = default;
    ScRecord();
    ScRecord(const ScRecord&) = default;
    ScRecord& operator=(const ScRecord&) = default;
//    ScRecord(const ScRecord& other);
//    ScRecord& operator=(const ScRecord&);
//    void swap(ScRecord& other);

//    void parseEvent(const QString& str);

//    void resolveStage(const QString& str);
//    void resolveMatch(const QString& str);
    void autoComplete();
    inline bool isValid(int flags) const { return (valid & flags) == flags; }



    QDomNode toXml(QDomDocument& doc) const;
    static ScRecord fromXml(const QDomNode& node);
};

Q_DECLARE_METATYPE(ScRecord)



//QDataStream &operator<<(QDataStream &stream, const ScRecord &value);
//QDataStream &operator>>(QDataStream &stream, ScRecord &value);

