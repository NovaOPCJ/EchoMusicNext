pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell
import EchoMusic.Playback

SettingsSection {
    id: section
    required property ShellSettings settings
    required property WindowController windowService
    required property PlaybackService playback
    readonly property PlaybackAudioControls audio: playback.audioControls
    heading: qsTr("音效管理")
    glyph: "adjustments"
    SettingsRow {
        objectName: "settingsEffectFilesRow"
        heading: qsTr("音效文件")
        description: qsTr("%1 个文件，按本地导入、歌手音效、耳机音效和音效市场分类管理").arg(section.audio.impulseResponses.length)
        // Settings actions use the shared neutral fill, shadow and interaction states.
        EchoSoftButton {
            objectName: "manageEffectFilesAction"
            text: qsTr("管理文件")
            colors: Theme.floating
            foreground: Theme.floating.text
            onClicked: fileDialogOwner.open()
        }
    }
    SettingsDivider {
        objectName: "settingsEffectFilesDivider"
    }
    SettingsRow {
        heading: qsTr("第三方音效引擎")
        description: section.audio.providers.count > 0 || section.audio.providerId.length > 0 ? qsTr("已安装 %1 个音效引擎").arg(section.audio.providers.count) : qsTr("未安装第三方音效引擎")
        EchoSoftButton {
            objectName: "manageDspProvidersAction"
            text: qsTr("管理引擎")
            colors: Theme.floating
            foreground: Theme.floating.text
            onClicked: providerDialogOwner.open()
        }
    }
    EchoPopupLoader {
        id: fileDialogOwner
        objectName: "audioEffectFilesDialogOwner"
        hostItem: section
        sourceComponent: Component {
            AudioEffectFilesDialog {
                id: fileDialog
                audio: section.audio
                uiScale: section.settings.uiScale
            }
        }
    }
    EchoPopupLoader {
        id: providerDialogOwner
        objectName: "dspProviderDialogOwner"
        hostItem: section
        sourceComponent: Component {
            DspProviderDialog {
                id: providerDialog
                audio: section.audio
                uiScale: section.settings.uiScale
            }
        }
    }
    Component.onDestruction: {
        fileDialogOwner.close();
        providerDialogOwner.close();
    }
}
