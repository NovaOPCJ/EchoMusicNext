pragma ComponentBehavior: Bound
import QtQuick

// Dialog-owned input drafts, separate from the library-owned running job.
QtObject {
    property int mode: 0
    property int targetMode: 0
    property string targetId: ""
    property string linkText: ""
    property string newName: ""
    property string duplicateName: ""
    property string fileMessage: ""
    property list<url> files: []
}
