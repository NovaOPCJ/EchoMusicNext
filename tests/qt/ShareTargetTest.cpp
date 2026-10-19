#include "ShareTarget.h"
#include <QUrlQuery>
#include <QtTest>
using namespace EchoMusic;
class ShareTargetTest final : public QObject
{
    Q_OBJECT
private slots:
    void typedAppAndWebLinksKeepTheCompleteTarget()
    {
        for (auto type :
             {ShareResource::Song, ShareResource::Playlist, ShareResource::Artist,
              ShareResource::Album, ShareResource::Plugin, ShareResource::ListenTogether}) {
            const QString id = type == ShareResource::Song       ? QString(32, 'A')
                               : type == ShareResource::Playlist ? "collection_3_900001_12_0"
                               : type == ShareResource::Plugin   ? "echo.spectrum-visualizer"
                                                                 : "1565598054776708260";
            ShareTarget target{type, id, {}};
            auto app = shareAppUrl(target);
            QVERIFY(app.toString().startsWith("echomusicnext://" + shareResourceName(type) + '/'));
            auto parsed = parseShareTarget(app.toString(QUrl::FullyEncoded));
            QVERIFY(parsed);
            QVERIFY(*parsed == target);
            parsed = parseShareTarget(shareWebUrl(target).toString(QUrl::FullyEncoded));
            QVERIFY(parsed);
            QVERIFY(*parsed == target);
            target.query = {{"roomType", "0"},
                            {"roomName", "晚风 & 月亮 + 100%"},
                            {"source", "https://example.org/index.json?name=空 格&kind=a+b"}};
            for (const auto &url : {shareAppUrl(target), shareWebUrl(target)}) {
                parsed = parseShareTarget(url.toString(QUrl::FullyEncoded));
                QVERIFY2(parsed, qPrintable(url.toString(QUrl::FullyEncoded)));
                QCOMPARE(parsed->id, target.id);
                QCOMPARE(parsed->query, target.query);
            }
        }
    }
    void invitationTextAndAlreadyIssuedLinksCanBeReceived()
    {
        const QString hash(32, 'b');
        auto target = parseShareTarget("小明 给你分享了歌曲，快去看看吧\n"
                                       "echomusic://song/" +
                                       hash + "?albumId=42，");
        QVERIFY(target);
        QCOMPARE(target->type, ShareResource::Song);
        QCOMPARE(target->id, hash);
        QCOMPARE(target->query.value("albumId"), QString("42"));
        target = parseShareTarget("echomusicnext://song?id=" + hash + "&albumAudioId=123");
        QVERIFY(target);
        QCOMPARE(target->id, hash);
        QCOMPARE(target->query.value("albumAudioId"), QString("123"));
        QVERIFY(!target->query.contains("id"));
        target = parseShareTarget("https://hoowhoami.github.io/EchoMusic/share/song/" + hash);
        QVERIFY(target);
        QCOMPARE(target->id, hash);
        target = parseShareTarget("echomusicnext://"
                                  "plugin?source=https%3A%2F%2Fexample.org%2Findex.json&id=echo."
                                  "spectrum&version=1.0.0&path=spectrum");
        QVERIFY(target);
        QCOMPARE(target->type, ShareResource::Plugin);
        QCOMPARE(target->query.value("source"), QString("https://example.org/index.json"));
    }
    void malformedOrUnrelatedLinksDoNotBecomeResources()
    {
        for (const auto &text :
             {"echomusicnext://song/not-a-hash", "echomusicnext://plugin/../bad",
              "echomusicnext://album/123/456", "echomusicnext://album/123#fragment",
              "echomusicnext://name@album/123", "echomusicnext://album:99/123",
              "echomusicnext://song?id=aa&id=bb", "echomusicnext://unknown/123",
              "https://evil.example/EchoMusicNext/share/?type=album&id=123",
              "https://hoowhoami.github.io/EchoMusicNext/shares/?type=album&id=123",
              "https://hoowhoami.github.io/EchoMusicNext/share/?type=album&id=1&id=2"})
            QVERIFY2(!parseShareTarget(QString::fromUtf8(text)), text);
        QVERIFY(!parseShareTarget(QString(65537, 'a')));
    }
    void sourceShareCopyKeepsResourceLabelNicknameAndDetails()
    {
        const ShareTarget target{
            ShareResource::ListenTogether, "12345", {{"roomType", "0"}, {"roomName", "晚风"}}};
        const auto text = shareText(target, "晚风", "小明");
        QVERIFY(text.startsWith("小明 给你分享了一起听房间「晚风」，快去看看吧\n"));
        const auto parsed = parseShareTarget(text);
        QVERIFY(parsed);
        QVERIFY(*parsed == target);
        QVERIFY(shareText(target).startsWith("EchoMusic 给你分享了一起听房间，"));
        QVERIFY(shareText({ShareResource::Song, "invalid", {}}).isEmpty());
    }
};
QTEST_GUILESS_MAIN(ShareTargetTest)
#include "ShareTargetTest.moc"
