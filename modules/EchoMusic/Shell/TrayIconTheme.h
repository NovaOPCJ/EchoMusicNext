#pragma once
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QIcon>
#include <functional>

namespace EchoMusic
{
// System surfaces follow the desktop, independently of ThemeStore's app skin.
class TrayIconTheme final : public QObject, public QAbstractNativeEventFilter
{
public:
    explicit TrayIconTheme(std::function<void()> update, QObject *parent = nullptr);
    ~TrayIconTheme() override;
    static QIcon icon();
    static QIcon monochrome(const QColor &color);

private:
    bool eventFilter(QObject *, QEvent *) override;
    bool nativeEventFilter(const QByteArray &, void *, qintptr *) override;
    std::function<void()> m_update;
};
} // namespace EchoMusic
