import QtQuick

Window {
    width: 320
    height: 240
    visible: true
    property int imageStatus: picture.status
    function reloadConsumers() {
        picture.source = "http://image-source.invalid/pixel.png?second=1";
    }
    Image {
        id: picture
        width: 2
        height: 2
        asynchronous: true
        cache: false
        source: "http://image-source.invalid/pixel.png"
    }
}
