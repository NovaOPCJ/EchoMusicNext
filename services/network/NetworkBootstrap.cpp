#include "NetworkBootstrap.h"
#include "HttpProxyRequest.h"
#include <QByteArray>
#include <QList>
namespace EchoMusic
{
void initializeNetworkEnvironment()
{
    // Distribution libcurl 7.81 predates thread-safe global initialization.
    // This sets up library globals only, before Qt/Rust create their workers;
    // it does not open sockets, read credentials or initialize business state.
    HttpProxyRequest::initializeRuntime();
    auto entries = qgetenv("no_proxy").split(',');
    entries += qgetenv("NO_PROXY").split(',');
    if (!entries.contains(QByteArrayLiteral("127.0.0.1")))
        entries.append(QByteArrayLiteral("127.0.0.1"));
    QByteArray combined;
    for (const auto &entry : entries) {
        if (entry.isEmpty())
            continue;
        if (!combined.isEmpty())
            combined += ',';
        combined += entry;
    }
    qputenv("no_proxy", combined);
}
} // namespace EchoMusic
