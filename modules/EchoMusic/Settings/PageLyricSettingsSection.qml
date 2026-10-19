pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell
import EchoMusic.Playback

SettingsSection {
    id: section
    property real uiScale: 1
    required property PlaybackService playback
    heading: qsTr("播放页")
    glyph: "play"
    LyricColorSettingsRow {
        uiScale: section.uiScale
        heading: qsTr("歌词颜色")
        description: qsTr("播放页皮肤未单独设置颜色时使用的已播与未播字色")
        coverColor: section.playback.coverColor
        playedColor: section.playback.lyricAppearance.playedColor || "#ffffff"
        unplayedColor: section.playback.lyricAppearance.unplayedColor || "#ffffff"
        followsCover: section.playback.lyricAppearance.followCoverColor
        unplayedFollowsCover: section.playback.lyricAppearance.unplayedFollowCoverColor
        customized: section.playback.lyricAppearance.playedColor.length > 0 || section.playback.lyricAppearance.unplayedColor.length > 0 || followsCover || unplayedFollowsCover
        onColorChosen: (unplayed, value, follow) => {
            if (unplayed) {
                section.playback.lyricAppearance.unplayedFollowCoverColor = follow;
                if (!follow)
                    section.playback.lyricAppearance.unplayedColor = value;
            } else {
                section.playback.lyricAppearance.followCoverColor = follow;
                if (!follow)
                    section.playback.lyricAppearance.playedColor = value;
            }
        }
        onResetRequested: section.playback.lyricAppearance.resetColors()
    }
    SettingsDivider {}
    SettingsRow {
        heading: qsTr("封面模糊背景")
        description: qsTr("使用模糊封面作为背景，关闭后使用主题色")
        EchoSwitch {
            colors: Theme.floating
            checked: section.playback.lyricAppearance.backgroundBlur
            onToggled: section.playback.lyricAppearance.backgroundBlur = checked
        }
    }
    SettingsDivider {}
    SettingsRow {
        heading: qsTr("背景律动")
        description: qsTr("让模糊背景色块流动，会增加性能消耗")
        EchoSwitch {
            colors: Theme.floating
            enabled: section.playback.lyricAppearance.backgroundBlur
            checked: section.playback.lyricAppearance.backgroundRhythm
            onToggled: section.playback.lyricAppearance.backgroundRhythm = checked
        }
    }
}
