#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
/// Read-only system UI metrics. Qt.styleHints is exposed as generic QObject in
/// QML; a typed projection avoids dynamic meta-property access in controls.
class EchoPageScrollMetrics final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int wheelStep READ wheelStep NOTIFY wheelStepChanged FINAL)
public:
    explicit EchoPageScrollMetrics(QObject *parent = nullptr);
    int wheelStep() const;
signals:
    void wheelStepChanged();
};
} // namespace EchoMusic
