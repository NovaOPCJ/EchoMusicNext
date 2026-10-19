#include "ThemeImageStore.h"
#include <QCoreApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 3)
        return 2;
    // This limit is process-wide in Qt. Keep large-image allocation isolated
    // from the GUI/cover decoders; normalize() separately caps source pixels.
    QImageReader::setAllocationLimit(1024);
    const auto result =
        EchoMusic::ThemeImageStore::normalize(app.arguments().at(1), app.arguments().at(2));
    const QJsonObject response{{"version", 1},
                               {"id", result.id},
                               {"error", result.error},
                               {"width", result.size.width()},
                               {"height", result.size.height()},
                               {"sample", result.sample.name(QColor::HexRgb)}};
    const auto bytes = QJsonDocument(response).toJson(QJsonDocument::Compact);
    return std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout) == size_t(bytes.size())
               ? 0
               : 3;
}
