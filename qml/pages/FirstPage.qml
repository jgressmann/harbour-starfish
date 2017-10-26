/*
  Copyright (C) 2013 Jolla Ltd.
  Contact: Thomas Perl <thomas.perl@jollamobile.com>
  All rights reserved.

  You may use this file under the terms of BSD license as follows:

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Jolla Ltd nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

import QtQuick 2.0
import QtQml.Models 2.2
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0

Page {
    id: page

    function sc2LinksRowCountChanged() {
        console.debug("sc2 rows: " + Sc2LinksDotCom.rowCount())
    }

    Connections {
        target: Sc2LinksDotCom
        onRowsInserted: sc2LinksRowCountChanged()
        onRowsRemoved: sc2LinksRowCountChanged()
    }

    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        anchors.fill: parent



        // Why is this necessary?
        contentWidth: parent.width

        VerticalScrollDecorator {}

        ListModel {
                        id: qmlModel
                        ListElement { type: "Dog"; age: 8; foo: "bar"; game: "bw" }
                        ListElement { type: "Cat"; age: 5; game: "sc2" }
                    }


        //https://stackoverflow.com/questions/16389831/how-to-access-listviews-current-item-from-qml/31206992#31206992
        DelegateModel {
            id: visualModel
            model: qmlModel
            delegate: Item {
                id: x
                //property string side1: visualModel.items.get(visualModel.modelIndex(index)).side1

                Component.onCompleted: {
                    return
                console.debug("visual model: " + visualModel)
                    //myDelegateModel.items.get(currentIndex).model;
                    console.debug("delegate model index: " + index)

                    var mi = visualModel.modelIndex(index)
                    console.debug("delegate model model index: " + mi)
                    var data = visualModel.model.data(mi)
                    console.debug("data: " + data)
                    var m = visualModel.items.get(index).model
                    console.debug("model: " + m)
                    console.debug("model.side1: " + m.side1)
                    var item = visualModel.items.get(index)
                    console.debug("items[0]: " + item[0])
                    console.debug("items[0].side1: " + item.side1)
                    console.debug("x.side1: " + x.side1)
                    //console.debug("side1: " + side1)
                    console.debug("model: " + model)
                    console.debug("model.side1: " + model[0])
//                    console.debug("model.get: " + model.get)
//                    console.debug("model.get.side1: " + model.get.side1)
                    //visualModel.model.data(ModelIndex {  } )
                    //console.debug("x.side1: " + x.side1)
                }
            }
        }

//        property var currentSelectedItem

//            onCurrentItemChanged{
//                    // Update the currently-selected item
//                    currentSelectedItem = myDelegateModel.items.get(currentIndex).model;
//                    // Log the Display Role
//                    console.log(currentSelectedItem.display);
//            }

        Label {
            id: title
            text: qsTr("Game")
            font.pixelSize: Theme.fontSizeLarge
            width: parent.width - 2*Theme.paddingLarge
            y: Theme.paddingMedium
            x: Theme.paddingLarge
            truncationMode: TruncationMode.Elide
            color: Theme.highlightColor
        }







        function getMediaPath(partial) {
            return "/usr/share/harbour-starfish/media/" + partial
        }

        DistinctModel {
            id: games
            sourceModel: Sc2LinksDotCom
            role: "game"

            onRowsInserted: {
                console.debug("rows added rows: " + rowCount())
            }

            onRowsRemoved: {
                console.debug("rows remove rows: " + rowCount())

            }

            onDataChanged: {
                console.debug("data changed rows: " + rowCount())
            }
        }

        SqlVodModel {
            id: distinctGames
            vodModel: Sc2LinksDotCom
            columns: ["game"]
            select: "select distinct game from vods"
        }

        Rectangle {
            color: "red"
            anchors { left: parent.left; right: parent.right; top: title.bottom; bottom: parent.bottom; }


        SilicaGridView {
            id: column

            model: distinctGames
//            anchors { left: parent.left; right: parent.right; top: title.bottom; bottom: parent.bottom; }
            anchors.fill: parent
            delegate: Component {
                Column {
//                    height: 400
                    width: parent.width
                    //Image { source: "image://" + icon; anchors.horizontalCenter: parent.horizontalCenter }

//                    Row {
//                        spacing: 10
//                        Label {
//                            text: side1
//                        }
//                        Label {
//                            text: "vs"
//                        }
//                        Label {
//                            text: side2
//                        }
//                    }
                    Label {
                        text: {
                            console.debug("view label text eval " + index + ": " + game)
                            if (game === 1) return "Broodwar"
                            if (game === 2) return "Sc2"
                            return game
                        }
                    }

//                    Image {
//                        source: "qrc:/icons/256x256/" + icon
//                    }

//                    Image {
//                        source: "icons/256x256/" + icon
//                    }


//                    Image {
//                        source: StandardPaths.data + "/icons/256x256/" + icon
//                    }

//                    Image {
//                        source: getMediaPath(icon)
//                    }

                    Component.onCompleted: {
                        return
                        console.debug("column.model: " + column.model)
                        console.debug("model: " + model)
                        //myDelegateModel.items.get(currentIndex).model;
                        console.debug("delegate model index: " + index)
                        console.debug("model.side1: " + model.side1)
                        console.debug("model[0]: " + model[0])
                        console.debug("model.get: " + model.get)
                        //console.debug("current index: " + currentIndex)
                        var item = column.model.data(index)
                        console.debug("item: " + item)
                        console.debug("side1: " + side1)
                        console.debug("game: " + game)
                        console.debug("StandardPaths.data: " + StandardPaths.data)
                        console.debug("StandardPaths.genericData: " + StandardPaths.genericData)
                        console.debug("StandardPaths.documents: " + StandardPaths.documents)
                    }
                }
            }
        }
        }
    }
}

