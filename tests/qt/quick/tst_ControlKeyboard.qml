pragma ComponentBehavior: Bound
import QtQuick
import QtTest
import EchoMusic.Controls

Item {
    id: root
    width: 400
    height: 200
    EchoCheckBox {
        id: checkbox
        x: 20
        y: 20
    }
    EchoTabBar {
        id: tabs
        x: 20
        y: 80
        width: 350
        labels: ["第一项", "不可用", "Last"]
        availableTabs: [true, false, true]
        onPicked: index => currentIndex = index
    }
    EchoDetailTabs {
        id: detailTabs
        x: 20
        y: 140
        width: 350
        labels: ["歌曲", "Albums", "统计"]
        counts: [1848, 49, ""]
        currentIndex: 1
        onPicked: index => currentIndex = index
    }
    TestCase {
        name: "ControlKeyboard"
        when: windowShown
        function init() {
            checkbox.enabled = true;
            checkbox.tristate = false;
            checkbox.checked = false;
            tabs.currentIndex = 0;
            detailTabs.currentIndex = 1;
        }
        function test_checkboxKeyboardAndDisabledPointer() {
            checkbox.forceActiveFocus();
            tryCompare(checkbox, "activeFocus", true);
            keyClick(Qt.Key_Space);
            compare(checkbox.checked, true);
            keyClick(Qt.Key_Space);
            compare(checkbox.checked, false);
            checkbox.enabled = false;
            mouseClick(checkbox);
            compare(checkbox.checked, false);
        }
        function test_tristateKeyboard() {
            checkbox.tristate = true;
            checkbox.forceActiveFocus();
            keyClick(Qt.Key_Space);
            compare(checkbox.checkState, Qt.PartiallyChecked);
            keyClick(Qt.Key_Space);
            compare(checkbox.checkState, Qt.Checked);
            keyClick(Qt.Key_Space);
            compare(checkbox.checkState, Qt.Unchecked);
        }
        function test_tabsSkipUnavailableAndWrap() {
            const first = findChild(tabs, "tab-0");
            const last = findChild(tabs, "tab-2");
            verify(first !== null && last !== null);
            first.forceActiveFocus();
            keyClick(Qt.Key_Right);
            compare(tabs.currentIndex, 2);
            tryCompare(last, "activeFocus", true);
            keyClick(Qt.Key_Right);
            compare(tabs.currentIndex, 0);
            tryCompare(first, "activeFocus", true);
            keyClick(Qt.Key_End);
            compare(tabs.currentIndex, 2);
            keyClick(Qt.Key_Home);
            compare(tabs.currentIndex, 0);
        }
        function test_detailInitialIndicatorAndKeyboard() {
            const indicator = findChild(detailTabs, "detailTabIndicator");
            const albums = findChild(detailTabs, "detailTab-1");
            verify(indicator !== null && albums !== null);
            tryVerify(() => detailTabs.activeTabItem === albums && indicator.width > 20);
            tryCompare(indicator, "width", albums.width);
            albums.forceActiveFocus();
            keyClick(Qt.Key_Right);
            compare(detailTabs.currentIndex, 2);
            keyClick(Qt.Key_Right);
            compare(detailTabs.currentIndex, 0);
            keyClick(Qt.Key_Left);
            compare(detailTabs.currentIndex, 2);
            keyClick(Qt.Key_Home);
            compare(detailTabs.currentIndex, 0);
            keyClick(Qt.Key_End);
            compare(detailTabs.currentIndex, 2);
        }
    }
}
