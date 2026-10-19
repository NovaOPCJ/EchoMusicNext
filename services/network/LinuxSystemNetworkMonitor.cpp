// Include GIO before Qt's signals macro (GDBusInterfaceInfo has a signals member).
#include <gio/gio.h>
#include "SystemNetworkMonitor.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <thread>

namespace EchoMusic
{
namespace
{
class LinuxSystemNetworkMonitor final : public QObject
{
public:
    LinuxSystemNetworkMonitor(QObject *parent, std::function<void(bool)> notify)
        : QObject(parent), m_notify(std::move(notify)), m_context(g_main_context_new())
    {
        // GSettings is event-driven and schema-aware. Do not watch the complete
        // dconf database: unrelated appearance changes must not close playback.
        // Its private GLib context also works with Qt builds without GLib dispatch.
        m_thread = std::thread([this] {
            g_main_context_push_thread_default(m_context);
            auto *loop = g_main_loop_new(m_context, FALSE);
            m_loop = loop;
            QList<GSettings *> settings;
            auto *source = g_settings_schema_source_get_default();
            auto *schema =
                source ? g_settings_schema_source_lookup(source, "org.gnome.system.proxy", TRUE)
                       : nullptr;
            if (schema) {
                auto *root = g_settings_new_full(schema, nullptr, nullptr);
                settings.append(root);
                auto **children = g_settings_list_children(root);
                for (auto **child = children; child && *child; ++child)
                    settings.append(g_settings_get_child(root, *child));
                g_strfreev(children);
                g_settings_schema_unref(schema);
                for (auto *entry : settings) {
                    g_signal_connect(
                        entry, "changed", G_CALLBACK(+[](GSettings *, gchar *, gpointer data) {
                            auto *self = static_cast<LinuxSystemNetworkMonitor *>(data);
                            QMetaObject::invokeMethod(
                                self, [self] { self->m_notify(true); }, Qt::QueuedConnection);
                        }),
                        this);
                    // GSettings emits changed only for keys read while connected.
                    GSettingsSchema *entrySchema = nullptr;
                    g_object_get(entry, "settings-schema", &entrySchema, nullptr);
                    auto **keys = g_settings_schema_list_keys(entrySchema);
                    for (auto **key = keys; key && *key; ++key)
                        g_variant_unref(g_settings_get_value(entry, *key));
                    g_strfreev(keys);
                    g_settings_schema_unref(entrySchema);
                }
            }
            g_main_loop_run(loop);
            m_loop = nullptr;
            for (auto *entry : settings)
                g_object_unref(entry);
            g_main_loop_unref(loop);
            g_main_context_pop_thread_default(m_context);
        });
        for (const auto &directory :
             QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
            m_kdeFiles.append(QDir(directory).filePath("kioslaverc"));
        m_kdeFiles.removeDuplicates();
        refreshKde(false);
        connect(&m_files, &QFileSystemWatcher::fileChanged, this, [this] { refreshKde(true); });
        connect(&m_files, &QFileSystemWatcher::directoryChanged, this,
                [this] { refreshKde(true); });
    }
    ~LinuxSystemNetworkMonitor() override
    {
        // An attached idle source is delivered after run() starts, even if this
        // object is destroyed during worker startup. invoke/quit before run()
        // would lose the quit and hang the join.
        auto *wake = g_idle_source_new();
        g_source_set_priority(wake, G_PRIORITY_HIGH);
        g_source_set_callback(
            wake,
            +[](gpointer data) -> gboolean {
                auto *self = static_cast<LinuxSystemNetworkMonitor *>(data);
                if (self->m_loop)
                    g_main_loop_quit(self->m_loop);
                return G_SOURCE_REMOVE;
            },
            this, nullptr);
        g_source_attach(wake, m_context);
        g_source_unref(wake);
        m_thread.join();
        g_main_context_unref(m_context);
    }

private:
    void refreshKde(bool notify)
    {
        QHash<QString, QByteArray> signatures;
        QStringList paths;
        for (const auto &fileName : m_kdeFiles) {
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly)) {
                signatures.insert(fileName, QCryptographicHash::hash(file.read(1024 * 1024),
                                                                     QCryptographicHash::Sha256));
                paths.append(fileName);
            }
            auto directory = QFileInfo(fileName).dir();
            while (!directory.exists() && directory.cdUp()) {
            }
            paths.append(directory.absolutePath());
        }
        paths.removeDuplicates();
        auto current = m_files.files() + m_files.directories();
        for (const auto &path : current)
            if (!paths.contains(path))
                m_files.removePath(path);
        for (const auto &path : paths)
            if (!current.contains(path))
                m_files.addPath(path);
        const bool changed = signatures != m_signatures;
        m_signatures = std::move(signatures);
        if (notify && changed)
            m_notify(true);
    }
    std::function<void(bool)> m_notify;
    GMainContext *m_context;
    GMainLoop *m_loop = nullptr;
    std::thread m_thread;
    QFileSystemWatcher m_files;
    QStringList m_kdeFiles;
    QHash<QString, QByteArray> m_signatures;
};
} // namespace
QObject *createSystemNetworkMonitor(QObject *parent, std::function<void(bool)> notify)
{
    return new LinuxSystemNetworkMonitor(parent, std::move(notify));
}
} // namespace EchoMusic
