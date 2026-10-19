pragma ComponentBehavior: Bound
pragma Singleton
import QtQuick
import QtQuick.Templates as T

// Qt owns the actual popup stack, modal blocking, Escape and focus restoration.
// This window-scoped notification only coordinates product rules such as
// dismissing search suggestions before another surface opens; it stores no stack.
QtObject {
    // Native Qt dialogs have no Templates.Popup instance. Their notification
    // passes a null surface with the owning window's overlay instead.
    signal popupOpening(T.Popup surface, Item overlay)
    // App-level system/tray menus can be invoked while another app window is
    // presenting Mini or desktop lyrics. Cancel hints/search across those hosts.
    signal nativeMenuOpening
    // Hints do not join the business popup lifecycle; only one can explain the
    // current pointer target in a window, including nested hover targets.
    signal hintOpening(T.Popup surface, Item overlay)
}
