# NOTICE:
#
# Application name defined in TARGET has a corresponding QML filename.
# If name defined in TARGET is changed, the following needs to be done
# to match new name:
#   - corresponding QML filename must be changed
#   - desktop icon filename must be changed
#   - desktop filename must be changed
#   - icon definition filename in desktop file must be changed
#   - translation filenames have to be changed

# The name of your application
TARGET = harbour-starfish

CONFIG *= sailfishapp
DEFINES += SAILFISH=1

SOURCES += src/harbour-starfish.cpp \
    src/Sc2LinksDotCom.cpp \
    src/DistinctModel.cpp \
    src/SqlVodModel.cpp

DISTFILES += qml/harbour-starfish.qml \
    qml/cover/CoverPage.qml \
    qml/pages/FirstPage.qml \
    qml/pages/SecondPage.qml \
    rpm/harbour-starfish.changes.in \
    rpm/harbour-starfish.changes.run.in \
    rpm/harbour-starfish.spec \
    rpm/harbour-starfish.yaml \
    translations/*.ts \
    harbour-starfish.desktop \
    media/*.png \
    media/foo \
    qml/pages/StartPage.qml \
    qml/pages/GamesPage.qml \
    qml/pages/Game.qml \
    qml/pages/FilteredItem.qml \
    qml/pages/FilterPage.qml \
    qml/pages/SelectionPage.qml \
    qml/Global.qml \
    qml/qmldir \
    qml/pages/TournamentPage.qml \
    qml/pages/StageItem.qml \
    qml/pages/MatchItem.qml

DEFINES += SAILFISH_DATADIR="/usr/share/$${TARGET}"

media.path = /usr/share/$${TARGET}/media
media.files = media/*

INSTALLS += media

SAILFISHAPP_ICONS = 86x86 108x108 128x128 256x256

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n

# German translation is enabled as an example. If you aren't
# planning to localize your app, remember to comment out the
# following TRANSLATIONS line. And also do not forget to
# modify the localized app name in the the .desktop file.
TRANSLATIONS += translations/harbour-starfish-de.ts

HEADERS += \
    src/Sc2LinksDotCom.h \
    src/DistinctModel.h \
    src/SqlVodModel.h


QT *= network sql

#RESOURCES += \
#    icons.qrc
