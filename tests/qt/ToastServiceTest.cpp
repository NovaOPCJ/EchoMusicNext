#include "ToastService.h"
#include <QtTest>
using namespace EchoMusic;
class ToastServiceTest : public QObject
{
    Q_OBJECT
private slots:
    void queueRetainsVisibleAndNewestAndDeduplicates()
    {
        ToastService service;
        const auto visible = service.show("第一条", ToastService::Warning, 0);
        service.show("最早等待", ToastService::Info, 0);
        const auto next = service.show("最新等待一", ToastService::Info, 0);
        service.show("最新等待二", ToastService::Info, 0);
        QCOMPARE(service.queuedCount(), 3);
        QCOMPARE(service.noticeId(), visible);
        QCOMPARE(service.show(" 第一条 ", ToastService::Warning, 0), visible);
        QCOMPARE(service.repetitions(), 2);
        service.dismiss(visible);
        QCOMPARE(service.noticeId(), next);
        QCOMPARE(service.message(), QString("最新等待一"));
        service.shutdown();
        QVERIFY(!service.visible());
        QCOMPARE(service.show("已关闭"), quint64(0));
    }
    void passiveNoticesExpireAndActionsPauseOnlyWhileAlive()
    {
        ToastService service;
        const auto passive = service.show("无需操作", ToastService::Warning, 30);
        service.pause(passive);
        QTRY_VERIFY(!service.visible());
        auto owner = std::make_unique<QObject>();
        int calls = 0;
        const auto action = service.showAction(
            *owner, "可恢复", "重试", [&] { ++calls; }, ToastService::Warning, 80);
        service.pause(action);
        QTest::qWait(100);
        QVERIFY(service.visible());
        service.resume(action);
        QTRY_VERIFY(!service.visible());
        const auto stale = service.showAction(*owner, "已停用", "重试", [&] { ++calls; });
        owner.reset();
        service.triggerAction(stale);
        QCOMPARE(calls, 0);
        QVERIFY(!service.visible());
        QObject active;
        const auto current = service.showAction(active, "有效", "继续", [&] { ++calls; });
        service.triggerAction(current);
        service.triggerAction(current);
        QCOMPARE(calls, 1);
    }
    void presentationCountsUnicodeCharactersAndTitles()
    {
        ToastService service;
        auto id = service.show(QString::fromUcs4(U"😀").repeated(30), ToastService::Info, 0);
        QVERIFY(!service.standard());
        service.dismiss(id);
        id = service.show(QString::fromUcs4(U"😀").repeated(31), ToastService::Info, 0);
        QVERIFY(service.standard());
        service.dismiss(id);
        service.show("短消息", ToastService::Warning, 0, ToastService::Mini, "标题");
        QVERIFY(service.standard());
    }
};
QTEST_GUILESS_MAIN(ToastServiceTest)
#include "ToastServiceTest.moc"
