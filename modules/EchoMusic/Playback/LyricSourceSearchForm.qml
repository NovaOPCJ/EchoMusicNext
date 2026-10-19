pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Playback

GridLayout {
    id: form
    required property LyricSourceController sources
    property bool narrow: false
    objectName: "lyricSourceSearchForm"
    columns: narrow ? 2 : 3
    columnSpacing: narrow ? 8 : 12
    rowSpacing: 8
    readonly property real fieldsWidth: Math.max(0, width - (narrow ? columnSpacing : searchActions.implicitWidth + columnSpacing * 2))
    ColumnLayout {
        Layout.preferredWidth: form.fieldsWidth * (form.narrow ? .5 : 115 / 215)
        Layout.fillWidth: true
        Layout.fillHeight: false
        Layout.minimumWidth: 0
        spacing: 6
        Label {
            text: qsTr("歌名")
            font: Theme.font(12, Theme.regularWeight)
            color: Theme.floatingSecondaryText
        }
        EchoTextField {
            objectName: "lyricSourceSearchTitle"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            colors: Theme.floating
            clearable: true
            text: form.sources.searchTitle
            placeholderText: qsTr("输入歌名")
            Accessible.name: qsTr("歌名")
            enabled: !form.sources.applying
            onTextEdited: form.sources.searchTitle = text
            onAccepted: if (form.sources.canSearch && !form.sources.loading)
                form.sources.search()
        }
    }
    ColumnLayout {
        Layout.preferredWidth: form.fieldsWidth * (form.narrow ? .5 : 100 / 215)
        Layout.fillWidth: true
        Layout.fillHeight: false
        Layout.minimumWidth: 0
        spacing: 6
        Label {
            text: qsTr("歌手")
            font: Theme.font(12, Theme.regularWeight)
            color: Theme.floatingSecondaryText
        }
        EchoTextField {
            objectName: "lyricSourceSearchArtist"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            colors: Theme.floating
            clearable: true
            text: form.sources.searchArtist
            placeholderText: qsTr("输入歌手")
            Accessible.name: qsTr("歌手")
            enabled: !form.sources.applying
            onTextEdited: form.sources.searchArtist = text
            onAccepted: if (form.sources.canSearch && !form.sources.loading)
                form.sources.search()
        }
    }
    RowLayout {
        id: searchActions
        Layout.fillHeight: false
        Layout.columnSpan: form.narrow ? 2 : 1
        Layout.alignment: Qt.AlignRight | Qt.AlignBottom
        spacing: 4
        EchoButton {
            objectName: "lyricSourceSearchAction"
            colors: Theme.floating
            text: qsTr("搜索")
            loading: form.sources.loading
            enabled: form.sources.canSearch && !form.sources.applying && !form.sources.loading
            onClicked: form.sources.search()
        }
    }
}
