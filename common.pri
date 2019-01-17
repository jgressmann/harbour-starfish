# known to qmake
VER_MAJ = 1
VER_MIN = 0
VER_PAT = 8

VERSION = $${VER_MAJ}.$${VER_MIN}.$${VER_PAT}

STARFISH_NAMESPACE=org.duckdns.jgressmann
DEFINES += STARFISH_APP_NAME=\"\\\"\"harbour-starfish\"\\\"\"
DEFINES += STARFISH_VERSION_MAJOR=$$VER_MAJ
DEFINES += STARFISH_VERSION_MINOR=$$VER_MIN
DEFINES += STARFISH_VERSION_PATCH=$$VER_PAT
DEFINES += STARFISH_NAMESPACE=\"\\\"\"$$STARFISH_NAMESPACE\"\\\"\"

INCLUDEPATH += $$PWD/src/starfish-lib

DISTFILES += \
    $$PWD/setup-build-engine.sh


CONFIG(debug, debug|release) {

    QMAKE_CXXFLAGS += -O0
}

