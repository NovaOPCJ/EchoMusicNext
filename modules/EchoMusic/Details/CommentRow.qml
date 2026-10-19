pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Account
import EchoMusic.Theme

Rectangle {
    id: row
    required property var entry
    required property string rootId
    required property CommentsController controller
    property bool compact: false
    property bool replyRow: false
    property bool resourceActive: true
    // The controller owns expansion across delegate recycling and enrichment.
    readonly property bool contentExpanded: !!entry.content_expanded
    readonly property var membershipPlate: {
        const membership = entry.membership || {};
        if (membership.user_type !== null && membership.user_type !== undefined && (membership.user_type & 16))
            return {
                kind: "super",
                text: qsTr("超级VIP")
            };
        const products = entry.products || [];
        for (const product of ["svip", "wvip", "tvip", "qvip"]) {
            const value = products.find(item => item.product === product && item.active === true);
            if (value)
                return {
                    kind: "concept",
                    text: product === "svip" ? qsTr("概念VIP") : product === "wvip" ? qsTr("周卡") : product === "tvip" ? qsTr("畅听VIP") : qsTr("季卡")
                };
        }
        if (entry.young_vip_type === 2 || entry.young_vip_type === 3)
            return {
                kind: "deluxe",
                text: qsTr("豪华VIP")
            };
        if (entry.young_vip_type === 1)
            return {
                kind: "music",
                text: qsTr("年费音乐包")
            };
        if ([1, 2, 3, 4, 6].indexOf(entry.vip_type) >= 0)
            return {
                kind: "deluxe",
                text: "VIP"
            };
        if (membership.m_type > 0)
            return {
                kind: "music",
                text: qsTr("音乐包")
            };
        return {
            kind: "none",
            text: ""
        };
    }
    readonly property var identityChips: {
        const info = entry.identity || {};
        const chips = [];
        for (const badge of [
            {
                flag: "student_status",
                text: qsTr("学生"),
                tone: "#0ea5e9"
            },
            {
                flag: "actor_status",
                text: qsTr("演员"),
                tone: "#07c160"
            },
            {
                flag: "biz_status",
                text: qsTr("认证"),
                tone: "#07c160"
            },
            {
                flag: "tme_star_status",
                text: qsTr("明星"),
                tone: "#07c160"
            }
        ])
            if (info[badge.flag] > 0)
                chips.push(badge);
        const auth = String(info.auth_info || "").trim();
        if (auth.length && [qsTr("达人"), qsTr("演唱者"), qsTr("歌手"), qsTr("学生")].indexOf(auth) < 0 && !chips.some(chip => chip.text === auth))
            chips.push({
                text: auth,
                tone: "#8b5cf6"
            });
        return chips;
    }
    function likeText(value: real): string {
        return value < 10000 ? String(value) : (value / 10000).toFixed(value >= 100000 ? 0 : 1).replace(/\.0$/, "") + "w";
    }
    signal replyRequested(var target, string rootId)
    signal deleteRequested(var target, string rootId, Item anchor)
    implicitHeight: content.implicitHeight + (replyRow ? 24 : compact ? 28 : 40)
    radius: compact ? Theme.popoverRadius : Theme.itemRadius
    color: Theme.withAlpha(Theme.colors.tone, replyRow ? .025 : .05)
    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: row.replyRow ? 12 : row.compact ? 14 : 20
        anchors.leftMargin: row.replyRow ? (row.compact ? 56 : 68) : row.compact ? 14 : 20
        spacing: 12
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: false
            spacing: 0
            EchoCover {
                objectName: "commentAvatar"
                Layout.preferredWidth: row.replyRow ? 28 : row.compact ? 30 : 36
                Layout.preferredHeight: width
                resourceActive: row.resourceActive
                source: row.entry.avatar_url || ""
                cornerRadius: width / 2
                placeholderGlyph: "user"
                placeholderGlyphSize: 14
                Image {
                    width: 16
                    height: 16
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    visible: !!row.entry.identity && String(row.entry.identity.pic || "").length > 0
                    source: visible && row.resourceActive && row.entry.identity ? row.entry.identity.pic || "" : ""
                    asynchronous: true
                    sourceSize: Qt.size(32, 32)
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 2
                    EchoTextTagRow {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: row.entry.user_name || ""
                        font: Theme.font(row.compact ? (row.replyRow ? 11 : 12) : 14, Theme.semiboldWeight)
                        color: Theme.text
                        spacing: 8
                        MembershipTag {
                            visible: row.membershipPlate.kind !== "none"
                            kind: row.membershipPlate.kind
                            text: row.membershipPlate.text
                        }
                        Repeater {
                            model: row.identityChips
                            delegate: EchoTag {
                                required property var modelData
                                text: modelData.text
                                tone: EchoTag.Custom
                                customTone: modelData.tone
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: [row.entry.created_at || "", row.entry.location || ""].filter(v => v.length).join(" · ")
                        color: Theme.secondaryText
                        font: Theme.font(10, Theme.regularWeight)
                        elide: Text.ElideRight
                    }
                }
                EchoBadge {
                    // The reply label uses 11px/600, 1.5 line height and 6px/10px padding.
                    Layout.preferredHeight: font.pixelSize * 1.5 + (row.compact ? 8 : 12)
                    horizontalPadding: row.compact ? 8 : 10
                    foreground: Theme.accentText
                    visible: !!row.entry.likes && row.entry.likes.count !== null && row.entry.likes.count !== undefined
                    count: row.entry.likes ? row.entry.likes.count : undefined
                    displayText: row.entry.likes && row.entry.likes.count !== null ? row.likeText(row.entry.likes.count) : ""
                    glyph: "thumb-up"
                    pill: true
                    font: Theme.font(row.compact ? 10 : 11, Theme.semiboldWeight)
                    tone: EchoBadge.Muted
                }
            }
            TextEdit {
                id: body
                objectName: "commentContent"
                Layout.fillWidth: true
                Layout.topMargin: row.compact ? 8 : 12
                Layout.preferredHeight: contentHeight
                readOnly: true
                textFormat: TextEdit.PlainText
                CommentTextStyle {
                    document: body.textDocument
                    lineHeight: body.font.pixelSize * 1.75
                }
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                font: Theme.font(row.compact ? 12 : 14, Theme.regularWeight)
                color: Theme.text
                text: {
                    const value = String(row.entry.body === undefined ? row.entry.content || "" : row.entry.body);
                    return Array.from(value).length > 120 && !row.contentExpanded ? Array.from(value).slice(0, 120).join("") + "…" : value;
                }
            }
            EchoSoftButton {
                visible: Array.from(String(row.entry.body === undefined ? row.entry.content || "" : row.entry.body)).length > 120
                plain: true
                size: EchoControlSize.Small
                text: row.contentExpanded ? qsTr("收起") : qsTr("展开")
                onClicked: row.controller.toggleContent(String(row.entry.id || ""))
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                visible: String(row.entry.quote_name || "").length > 0
                implicitHeight: quote.implicitHeight + 16
                color: Theme.mutedSurface
                radius: Theme.detailRadius
                Label {
                    id: quote
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    text: (row.entry.quote_name || "") + "：" + (row.entry.quote_content || "")
                    wrapMode: Text.Wrap
                    color: Theme.secondaryText
                    font: Theme.font(12, Theme.regularWeight)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 16
                EchoSoftButton {
                    objectName: "commentThreadToggle"
                    visible: !row.replyRow && (row.entry.reply_count > 0 || !!row.entry.context_reply)
                    plain: true
                    // Reply actions have no button inset.
                    implicitHeight: 24
                    leftPadding: 0
                    rightPadding: 0
                    font: Theme.font(row.compact ? 11 : 12, Theme.mediumWeight)
                    foreground: Theme.accentText
                    text: row.entry.thread_expanded ? qsTr("收起回复") : row.entry.context_reply ? qsTr("查看上下文") : qsTr("查看%1条回复").arg(row.entry.reply_count)
                    onClicked: row.controller.toggleThread(row.rootId)
                }
                EchoSoftButton {
                    objectName: "commentReply"
                    visible: row.replyRow || !row.entry.context_reply
                    plain: true
                    implicitHeight: 24
                    leftPadding: 0
                    rightPadding: 0
                    font: Theme.font(row.compact ? 11 : 12, Theme.mediumWeight)
                    foreground: Theme.accentText
                    text: qsTr("回复")
                    onClicked: row.replyRequested(row.entry, row.rootId)
                }
                Item {
                    Layout.fillWidth: true
                }
                EchoIconButton {
                    id: deleteButton
                    objectName: "commentDelete"
                    visible: row.controller.canDelete(row.entry)
                    text: qsTr("删除评论")
                    glyph: "trash"
                    enabled: !row.controller.busy
                    onClicked: row.deleteRequested(row.entry, row.rootId, deleteButton)
                }
            }
        }
    }
}
