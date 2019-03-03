include(../../common.pri)
include(../starfish-lib/src.pri)
include(../../3rd-party/harbour-vodman/src/vodman-lib/vodman.pri)

TARGET = harbour-starfish

CONFIG += sailfishapp


SOURCES += harbour-starfish.cpp \
    ScVodman.cpp \
    ScApp.cpp \
    ScVodDataManager.cpp \
    ScVodDataManagerWorker.cpp \
    ScVodDatabaseDownloader.cpp \
    ScIcons.cpp \
    ScSqlVodModel.cpp \
    ScStopwatch.cpp \
    ScDatabaseStoreQueue.cpp \
    ScDatabase.cpp \
    ScRecentlyWatchedVideos.cpp \
    ScMatchItem.cpp \
    ScUrlShareItem.cpp



HEADERS += \
    ScVodman.h \
    ScApp.h \
    ScVodDataManager.h \
    ScVodDataManagerWorker.h \
    ScVodDataManagerState.h \
    ScVodDatabaseDownloader.h \
    ScIcons.h \
    ScSqlVodModel.h \
    ScStopwatch.h \
    ScDatabaseStoreQueue.h \
    ScDatabase.h \
    ScRecentlyWatchedVideos.h \
    ScMatchItem.h \
    ScUrlShareItem.h

#../../rpm/harbour-starfish.changes.run.in \
#../../rpm/harbour-starfish.yaml \
#    ../../rpm/harbour-starfish.changes.in \
#    ../../rpm/harbour-starfish.spec \



DISTFILES += qml/harbour-starfish.qml \
    qml/cover/CoverPage.qml \
    classifier.json \
    database.json \
    icons.json \
    sql_patches.json \
    harbour-starfish.desktop \
    media/* \
    icons/* \
    icons.json \
    qml/pages/StartPage.qml \
    qml/pages/FilterItem.qml \
    qml/pages/FilterPage.qml \
    qml/pages/SelectionPage.qml \
    qml/Global.qml \
    qml/qmldir \
    qml/pages/TournamentPage.qml \
    qml/pages/StageItem.qml \
    qml/pages/MatchItem.qml \
    qml/pages/StagePage.qml \
    qml/pages/VideoPlayerPage.qml \
    qml/pages/BasePage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/SelectFormatDialog.qml \
    qml/pages/NewPage.qml \
    qml/pages/EntryPage.qml \
    qml/pages/AboutPage.qml \
    qml/pages/ToolsPage.qml \
    qml/pages/OpenVideoPage.qml \
    qml/pages/ActiveDownloadPage.qml \
    qml/pages/StatsPage.qml \
    qml/TopMenu.qml \
    qml/Global.qml \
    qml/FormatComboBox.qml \
    qml/ProgressOverlay.qml \
    qml/SeenButton.qml \
    qml/ProgressMaskEffect.qml \
    qml/ProgressImageBlendEffect.qml \
    qml/HighlightingListView.qml \
    qml/VodDataManagerBusyIndicator.qml \
    qml/RecentlyWatchedVideoView.qml \
    qml/RecentlyWatchedVideoUpdater.qml \
    qml/ContentPageHeader.qml \
    qml/pages/VodDetailPage.qml \
    qml/pages/MoveDataDirectoryPage.qml \
    qml/MatchItemMemory.qml \
    qml/Strings.qml \
    translations/harbour-starfish.ts \
    translations/harbour-starfish-de.ts

DEFINES += SAILFISH_DATADIR="/usr/share/$${TARGET}"


media.path = /usr/share/$${TARGET}/media
media.files += media/sc2.png media/bw.png
media.files += media/protoss.png media/terran.png media/zerg.png media/random.png
INSTALLS += media

icons.path = /usr/share/$${TARGET}/icons
icons.files = icons/warning.png icons/flash.png
INSTALLS += icons

gzicons.target = icons.json.gz
gzicons.commands = cat $$PWD/icons.json | gzip --best > $$PWD/$$gzicons.target

gzclassifier.target = classifier.json.gz
gzclassifier.commands = cat $$PWD/classifier.json | gzip --best > $$PWD/$$gzclassifier.target

gzsqlpatches.target = sql_patches.json.gz
gzsqlpatches.commands = cat $$PWD/sql_patches.json | gzip --best > $$PWD/$$gzsqlpatches.target

QMAKE_EXTRA_TARGETS += gzicons gzclassifier gzsqlpatches
PRE_TARGETDEPS += $$gzicons.target $$gzclassifier.target $$gzsqlpatches.target

misc.files = $$gzicons.target $$gzclassifier.target ../../COPYING
misc.path = /usr/share/$${TARGET}
misc.depends = gzicons gzclassifier
INSTALLS += misc


SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n
CONFIG += sailfishapp_i18n_idbased
CONFIG += sailfishapp_i18n_unfinished

# German translation is enabled as an example. If you aren't
# planning to localize your app, remember to comment out the
# following TRANSLATIONS line. And also do not forget to
# modify the localized app name in the the .desktop file.
TRANSLATIONS += translations/harbour-starfish.ts
TRANSLATIONS += translations/harbour-starfish-de.ts


QT *= network sql qml xml

LIBS += -lz

