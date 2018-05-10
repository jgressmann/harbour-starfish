#include "ScClassifier.h"

QString ScClassifier::icon(const ScRecord& record) {
    if (record.isValid(ScRecord::ValidGame)) {
        switch (game) {
        case ScRecord::GameBroodWar:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/bw.png");
        case ScRecord::GameSc2:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/sc2.png");
        default:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/random.png");
        }
    }
}
