#pragma once
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
namespace EchoMusic
{
struct UpdateAsset
{
    QString name, kind;
    qint64 size = 0;
    QByteArray sha512;
    QUrl url;
};
struct PublishedRelease
{
    QString version, notes;
    QUrl url, manifestUrl;
    bool prerelease = false;
};
namespace ReleaseManifest
{
QString platform();
QString architecture();
QString manifestName(const QString &os, const QString &arch);
bool validVersion(const QString &version);
int compareVersions(const QString &a, const QString &b);
QList<PublishedRelease> releases(const QByteArray &bytes, const QString &os, const QString &arch,
                                 QString *error);
QList<UpdateAsset> assets(const QByteArray &bytes, const PublishedRelease &release,
                          const QString &os, const QString &arch, QString *error);
} // namespace ReleaseManifest
} // namespace EchoMusic
