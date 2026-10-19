pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

Image {
    // Vector illustration derived from EchoMusic Profile.vue, GPL-3.0-only.
    // Theme changes regenerate the image; it is not decoded every frame.
    width: 88
    height: 76
    sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
    sourceSize.height: Math.ceil(height * Screen.devicePixelRatio)
    source: "data:image/svg+xml;utf8," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 112" fill="none">' + '<ellipse cx="64" cy="58" rx="48" ry="45" fill="' + Theme.accent + '" fill-opacity=".06"/>' + '<path d="M 13 78 C -6 58 93 14 115 34" stroke="' + Theme.accent + '" stroke-width="2" stroke-linecap="round"/>' + '<circle cx="64" cy="56" r="34" fill="' + Theme.accentText + '" stroke="' + Theme.accent + '" stroke-opacity=".3" stroke-width="2"/>' + '<g stroke="' + Theme.main + '" stroke-opacity=".22"><circle cx="64" cy="56" r="28"/><circle cx="64" cy="56" r="23"/><circle cx="64" cy="56" r="18"/></g>' + '<path d="M 39 49 A 26 26 0 0 1 59 31" stroke="white" stroke-opacity=".55" stroke-width="2" stroke-linecap="round"/>' + '<circle cx="64" cy="56" r="11" fill="' + Theme.accent + '"/><circle cx="64" cy="56" r="3" fill="' + Theme.main + '"/>' + '<path d="M 115 34 C 139 53 34 101 13 78" stroke="' + Theme.accent + '" stroke-width="2" stroke-linecap="round"/>' + '<circle cx="101" cy="65" r="4" fill="' + Theme.accent + '" stroke="' + Theme.main + '" stroke-width="2"/>' + '<g fill="' + Theme.accentText + '" opacity=".65"><path d="M 25 16 L 27 22 L 33 24 L 27 26 L 25 32 L 23 26 L 17 24 L 23 22 Z"/><circle cx="90" cy="97" r="2"/><circle cx="13" cy="48" r="1.5"/></g>' + '<path d="M 104 15 V 5 L 112 3 V 12 M 104 15 C 104 19 97 19 97 16 C 97 13 104 12 104 15 Z M 112 12 C 112 16 105 16 105 13 C 105 10 112 9 112 12 Z" stroke="' + Theme.accentText + '" stroke-width="1.5" stroke-linejoin="round"/></svg>')
}
