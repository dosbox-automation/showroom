// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_NET_DOWNLOADER_H
#define SHOWROOM_NET_DOWNLOADER_H

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QUrl>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace showroom {

// One HTTP transfer to <destination>.part, renamed into place only when
// the byte count proves out. A kept .part is the resume point for the
// next attempt; the server's Last-Modified lands on the finished file
// (house rule on timestamps).
class Downloader : public QObject {
    Q_OBJECT

public:
    explicit Downloader(QObject* parent = nullptr);
    ~Downloader() override;

    // A declared size runs a pre-flight free-space check, minus whatever
    // the .part already holds. Virtual so the window tests run against a
    // fake that never opens a socket.
    virtual bool start(const QUrl& url, const std::filesystem::path& destination,
                       std::string& error,
                       std::optional<std::uint64_t> expected_size_bytes = std::nullopt);
    // Aborts the transfer and keeps the .part for a later resume.
    virtual void cancel();
    virtual bool isRunning() const;
    bool isActive() const { return isRunning(); }

signals:
    void progress(qint64 received_bytes, qint64 total_bytes);
    void finished(const QString& path);
    void failed(const QString& reason);
    void cancelled();

private:
    void onReadyRead();
    void onFinished();
    void finishTransfer();
    void failTransfer(const QString& reason, bool keep_part);
    void cleanupReply();

    QNetworkAccessManager network_;
    QNetworkReply* reply_ = nullptr;
    QFile part_;
    std::filesystem::path destination_;
    std::filesystem::path part_path_;
    std::uint64_t resume_from_ = 0;
    int http_status_ = 0;
    bool body_accepted_ = false;
    bool cancelling_ = false;
};

} // namespace showroom

#endif // SHOWROOM_NET_DOWNLOADER_H
