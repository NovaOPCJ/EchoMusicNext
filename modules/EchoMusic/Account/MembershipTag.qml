pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls

EchoTag {
    required property string kind
    property color musicTone: "#10b981"
    tone: kind === "none" ? EchoTag.Neutral : EchoTag.Custom
    customTone: kind === "super" ? "#f97316" : kind === "deluxe" ? "#e8a317" : kind === "concept" ? "#8b5cf6" : musicTone
}
