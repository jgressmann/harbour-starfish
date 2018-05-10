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

#include "Vods.h"

#include <QDomNode>

ScMatch::ScMatch()
    : d(new ScMatchData())
{}

ScMatch::ScMatch(QSharedPointer<ScMatchData> ptr)
    : d(ptr)
{}

ScStage ScMatch::stage() const {
    return ScStage(d->owner.toStrongRef());
}

QDomNode ScMatch::toXml(QDomDocument& doc) const {
    if (!isValid()) {
        return QDomNode();
    }

    auto matchElement = doc.createElement(QStringLiteral("match"));
    matchElement.setAttribute(QStringLiteral("name"), d->name);
    matchElement.setAttribute(QStringLiteral("url"), d->url);
    matchElement.setAttribute(QStringLiteral("date"), d->matchDate.toString(QStringLiteral("yyyy-MM-dd")));
    matchElement.setAttribute(QStringLiteral("side1"), d->side1);
    matchElement.setAttribute(QStringLiteral("side2"), d->side2);
    matchElement.setAttribute(QStringLiteral("number"), d->matchNumber);

    return matchElement;
}

ScMatch ScMatch::fromXml(const QDomNode& node, int version) {
    Q_UNUSED(version);

    ScMatch result;
    ScMatchData& data = result.data();
    auto attrs = node.attributes();
    data.name = attrs.namedItem(QStringLiteral("name")).nodeValue();
    data.url = attrs.namedItem(QStringLiteral("url")).nodeValue();
    data.matchDate = QDate::fromString(attrs.namedItem(QStringLiteral("date")).nodeValue(), QStringLiteral("yyyy-MM-dd"));
    data.side1 = attrs.namedItem(QStringLiteral("side1")).nodeValue();
    data.side2 = attrs.namedItem(QStringLiteral("side2")).nodeValue();

    bool numberOk = false;
    data.matchNumber = attrs.namedItem(QStringLiteral("number")).nodeValue().toInt(&numberOk);

    return result;
}

ScStage::ScStage()
    : d(new ScStageData())
{}

ScStage::ScStage(QSharedPointer<ScStageData> ptr)
    : d(ptr)
{}

QDomNode ScStage::toXml(QDomDocument& doc) const {
    if (!isValid()) {
        return QDomNode();
    }

    auto stageElement = doc.createElement(QStringLiteral("stage"));
    stageElement.setAttribute(QStringLiteral("name"), d->name);
    stageElement.setAttribute(QStringLiteral("url"), d->url);
    stageElement.setAttribute(QStringLiteral("number"), d->stageNumber);

    QDomElement matchesElement = doc.createElement(QStringLiteral("matches"));
    foreach (const auto& match, d->matches) {
        QDomNode matchNode = match.toXml(doc);
        if (matchNode.isNull()) {
            return QDomNode();
        }

        matchesElement.appendChild(matchNode);
    }

    stageElement.appendChild(matchesElement);

    return stageElement;
}

ScStage ScStage::fromXml(const QDomNode& node, int version) {
    Q_UNUSED(version);

    ScStage result;
    ScStageData& data = result.data();
    auto attrs = node.attributes();
    data.name = attrs.namedItem(QStringLiteral("name")).nodeValue();
    data.url = attrs.namedItem(QStringLiteral("url")).nodeValue();
    data.stageNumber = attrs.namedItem(QStringLiteral("number")).nodeValue().toInt();
    auto matches = node.namedItem(QStringLiteral("matches")).childNodes();
    for (int i = 0; i < matches.size(); ++i) {
        auto match = ScMatch::fromXml(matches.item(i), version);
        if (!match.isValid()) {
            result.d.reset();
            break;
        }

        match.data().owner = result.toWeak();
        data.matches << match;
    }

    return result;
}

ScEvent ScStage::event() const {
    return ScEvent(d->owner.toStrongRef());
}

ScEvent::ScEvent()
    : d(new ScEventData())
{}

ScEvent::ScEvent(QSharedPointer<ScEventData> ptr)
    : d(ptr)
{}

QDomNode ScEvent::toXml(QDomDocument& doc) const {
    if (!isValid()) {
        return QDomNode();
    }

    auto eventElement = doc.createElement(QStringLiteral("event"));
    eventElement.setAttribute(QStringLiteral("name"), d->name);
    eventElement.setAttribute(QStringLiteral("full_name"), d->fullName);
    eventElement.setAttribute(QStringLiteral("url"), d->url);
    eventElement.setAttribute(QStringLiteral("year"), d->year);
    eventElement.setAttribute(QStringLiteral("season"), d->season);
    eventElement.setAttribute(QStringLiteral("game"), d->game == ScEnums::Game_Broodwar ? QStringLiteral("Broodwar") : QStringLiteral("StarCraft II"));

    QDomElement stagesElement = doc.createElement(QStringLiteral("stages"));
    foreach (const auto& stage, d->stages) {
        QDomNode stageNode = stage.toXml(doc);
        if (stageNode.isNull()) {
            return QDomNode();
        }

        stagesElement.appendChild(stageNode);
    }

    eventElement.appendChild(stagesElement);

    return eventElement;
}

ScEvent ScEvent::fromXml(const QDomNode& node, int version) {
    Q_UNUSED(version);


    ScEvent result;
    ScEventData& data = result.data();
    auto attrs = node.attributes();
    data.name = attrs.namedItem(QStringLiteral("name")).nodeValue();
    data.fullName = attrs.namedItem(QStringLiteral("full_name")).nodeValue();
    data.url = attrs.namedItem(QStringLiteral("url")).nodeValue();
    data.year = attrs.namedItem(QStringLiteral("year")).nodeValue().toInt();
    data.season = attrs.namedItem(QStringLiteral("season")).nodeValue().toInt();
    auto game = attrs.namedItem(QStringLiteral("game")).nodeValue();
    if (game == QStringLiteral("Broodwar")) {
        data.game = ScEnums::Game_Broodwar;
    } else {
        data.game = ScEnums::Game_Sc2;
    }

    auto stages = node.namedItem(QStringLiteral("stages")).childNodes();
    for (int i = 0; i < stages.size(); ++i) {
        auto stage = ScStage::fromXml(stages.item(i), version);
        if (!stage.isValid()) {
            result.d.reset();
            break;
        }

        stage.data().owner = result.toWeak();
        data.stages << stage;
    }

    return result;
}

//ScSerie::ScSerie()
//    : d(new ScSerieData())
//{}

