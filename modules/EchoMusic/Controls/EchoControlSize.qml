pragma ComponentBehavior: Bound
pragma Singleton
import QtQuick

// Shared field and button size roles; custom captions and rows own their geometry.
QtObject {
    enum Size {
        Small,
        Compact,
        Standard,
        Large
    }
    function heightFor(size: int): real {
        return size === EchoControlSize.Small ? 28 : size === EchoControlSize.Compact ? 32 : size === EchoControlSize.Large ? 56 : 36;
    }
    function buttonPaddingFor(size: int): real {
        return size === EchoControlSize.Small ? 8 : size === EchoControlSize.Compact ? 10 : size === EchoControlSize.Large ? 24 : 12;
    }
    function fieldPaddingFor(size: int): real {
        return size === EchoControlSize.Small ? 8 : size === EchoControlSize.Compact ? 10 : size === EchoControlSize.Large ? 24 : 12;
    }
}
