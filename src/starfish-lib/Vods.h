/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QWeakPointer>
#include <QDate>

class QDomDocument;
class QDomNode;


class ScEnums : public QObject
{
    Q_OBJECT
public:
    enum Game {
        Game_Unknown,
        Game_Broodwar,
        Game_Sc2
    };
    Q_ENUMS(Game)

    enum XmlFormatVersion {
        XmlFormatVersion_1 = 1,
        XmlFormatVersion_Current = XmlFormatVersion_1,
    };
};

class ScEvent;
class ScStage;
class ScSerie;
class ScMatch;

struct ScMatchData;
struct ScStageData;
struct ScEventData;


struct ScBaseData
{
    QString url;
    QString name;
};

struct ScMatchData : public ScBaseData
{
    QString side1;
    QString side2;
    QString race1;
    QString race2;
    QDate matchDate;
    int matchNumber;
    QWeakPointer<ScStageData> owner;
};


class ScMatch
{
    Q_GADGET
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(QString side1 READ side1 CONSTANT)
    Q_PROPERTY(QString side2 READ side2 CONSTANT)
    Q_PROPERTY(QString race1 READ race1 CONSTANT)
    Q_PROPERTY(QString race2 READ race2 CONSTANT)
    Q_PROPERTY(QDate date READ date CONSTANT)
    Q_PROPERTY(int number READ number CONSTANT)

public:
    ~ScMatch() = default;
    ScMatch();
    ScMatch(QSharedPointer<ScMatchData> ptr);
    ScMatch(const ScMatch&) = default;
    ScMatch& operator=(const ScMatch&) = default;

public:
    QString name() const { return d->name; }
    QString url() const { return d->url; }
    QString side1() const { return d->side1; }
    QString side2() const { return d->side2; }
    QString race1() const { return d->race1; }
    QString race2() const { return d->race2; }
    QDate date() const { return d->matchDate; }
    int number() const { return d->matchNumber; }
    ScStage stage() const;
    bool isValid() const { return !!d; }
    QDomNode toXml(QDomDocument& doc) const;
    static ScMatch fromXml(const QDomNode& node, int version = ScEnums::XmlFormatVersion_Current);

public:
    inline ScMatchData& data() { return *d; }
    inline const ScMatchData& data() const { return *d; }

private:
    QSharedPointer<ScMatchData> d;
};


struct ScStageData : public ScBaseData
{
    QList<ScMatch> matches;
    QWeakPointer<ScEventData> owner;
    int stageNumber;
};

class ScStage
{
    Q_GADGET
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(int number READ number CONSTANT)
    Q_PROPERTY(QList<ScMatch> matches READ matches CONSTANT)

public:
    ~ScStage() = default;
    ScStage();
    ScStage(QSharedPointer<ScStageData> ptr);
    ScStage(const ScStage&) = default;
    ScStage& operator=(const ScStage&) = default;

public:
    QString name() const { return d->name; }
    QString url() const { return d->url; }
    int number() const { return d->stageNumber; }

    QList<ScMatch> matches() const { return d->matches; }
    ScEvent event() const;
    QWeakPointer<ScStageData> toWeak() const { return d.toWeakRef(); }
    bool isValid() const { return !!d; }
    QDomNode toXml(QDomDocument& doc) const;
    static ScStage fromXml(const QDomNode& node, int version = ScEnums::XmlFormatVersion_Current);

public:
    inline ScStageData& data() { return *d; }
    inline const ScStageData& data() const { return *d; }

private:
    QSharedPointer<ScStageData> d;
};

struct ScEventData : public ScBaseData
{
    QList<ScStage> stages;
    QString fullName;
    QDate lastModified;
    int season;
    int game;
    int year;
};

class ScEvent
{
    Q_GADGET
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(QString fullName READ fullName CONSTANT)

    Q_PROPERTY(int season READ season CONSTANT)
    Q_PROPERTY(int year READ year CONSTANT)
    Q_PROPERTY(ScEnums::Game game READ game CONSTANT)
    Q_PROPERTY(QList<ScStage> stages READ stages CONSTANT)


public:
    ~ScEvent() = default;
    ScEvent();
    ScEvent(QSharedPointer<ScEventData> ptr);
    ScEvent(const ScEvent&) = default;
    ScEvent& operator=(const ScEvent&) = default;

public:
    QString name() const { return d->name; }
    QString fullName() const { return d->fullName; }
    QString url() const { return d->url; }
//    QString fullName() const { return d->fullName; }
    QList<ScStage> stages() const { return d->stages; }
    int season() const { return d->season; }
    ScEnums::Game game() const { return (ScEnums::Game)d->game; }
    int year() const { return d->year; }
    bool isDefault() const { return d->name.isEmpty(); }
    QWeakPointer<ScEventData> toWeak() const { return d.toWeakRef(); }
    bool isValid() const { return !!d; }
    QDomNode toXml(QDomDocument& doc) const;
    static ScEvent fromXml(const QDomNode& node, int version = ScEnums::XmlFormatVersion_Current);

public:
    inline ScEventData& data() { return *d; }
    inline const ScEventData& data() const { return *d; }

private:
    QSharedPointer<ScEventData> d;
};

//struct ScSerieData : public ScBaseData
//{
//    QList<ScEvent> events;
//};

//class ScSerie
//{
//    Q_GADGET
//    Q_PROPERTY(QString name READ name CONSTANT)
//    Q_PROPERTY(QString url READ url CONSTANT)
//    Q_PROPERTY(QList<ScEvent> events READ events CONSTANT)

//public:
//    ~ScSerie() = default;
//    ScSerie();
//    ScSerie(const ScSerie&) = default;
//    ScSerie& operator=(const ScSerie&) = default;

//public:
//    QString name() const { return d->name; }
//    QString url() const { return d->url; }
//    QList<ScEvent> events() const { return d->events; }

//public:
//    inline ScSerieData& data() { return *d; }
//    inline const ScSerieData& data() const { return *d; }

//private:
//    QSharedPointer<ScSerieData> d;
//};

Q_DECLARE_METATYPE(ScMatch)
Q_DECLARE_METATYPE(ScStage)
Q_DECLARE_METATYPE(ScEvent)
//Q_DECLARE_METATYPE(ScSerie)
