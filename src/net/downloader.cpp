// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/downloader.h"

#include "app/logging.h"

#include <QDateTime>
#include <QNetworkRequest>

#include <chrono>
#include <system_error>

namespace showroom {
namespace {

constexpr const char* kLogComponent = "downloader";

} // namespace

Downloader::Downloader(QObject* parent) : QObject(parent) {}

Downloader::~Downloader()
{
    if (reply_ != nullptr) {
        reply_->disconnect(this);
        reply_->abort();
    }
}

bool Downloader::start(const QUrl& url, const std::filesystem::path& destination,
                       std::string& error,
                       std::optional<std::uint64_t> expected_size_bytes)
{
    error.clear();
    if (reply_ != nullptr) {
        error = "a transfer is already running";
        return false;
    }
    if (!url.isValid() || url.isEmpty()) {
        error = "invalid url";
        return false;
    }

    destination_ = destination;
    part_path_ = destination;
    part_path_ += ".part";

    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);

    resume_from_ = 0;
    if (std::filesystem::is_regular_file(part_path_, ec)) {
        resume_from_ = std::filesystem::file_size(part_path_, ec);
        if (ec) {
            resume_from_ = 0;
        }
    }

    if (expected_size_bytes.has_value()) {
        const std::uint64_t still_needed = *expected_size_bytes > resume_from_
                                                 ? *expected_size_bytes - resume_from_
                                                 : 0;
        const auto space = std::filesystem::space(destination.parent_path(), ec);
        // An unanswerable probe does not block: the transfer itself will
        // report a full disk, this check only fails it earlier and cleaner.
        if (!ec && space.available < still_needed) {
            error = "not enough free space: need " + std::to_string(still_needed)
                  + " bytes, " + std::to_string(space.available) + " available";
            return false;
        }
    }

    http_status_ = 0;
    body_accepted_ = false;
    cancelling_ = false;

    QNetworkRequest request(url);
    if (resume_from_ > 0) {
        request.setRawHeader("Range",
                             "bytes=" + QByteArray::number(qint64(resume_from_)) + "-");
    }

    log_info(kLogComponent,
             "starting download: %s -> %s%s",
             url.toString().toUtf8().constData(),
             destination_.string().c_str(),
             resume_from_ > 0 ? (std::string(" (resuming from ")
                                 + std::to_string(resume_from_) + ")")
                                        .c_str()
                              : "");

    reply_ = network_.get(request);
    connect(reply_, &QNetworkReply::readyRead, this, &Downloader::onReadyRead);
    connect(reply_, &QNetworkReply::finished, this, &Downloader::onFinished);
    connect(reply_,
            &QNetworkReply::downloadProgress,
            this,
            [this](qint64 received, qint64 total) {
                // A 206 reports only the tail; the tile wants the whole
                // file's numbers.
                const auto base = http_status_ == 206 ? qint64(resume_from_) : 0;
                emit progress(base + received, total < 0 ? total : base + total);
            });
    return true;
}

void Downloader::cancel()
{
    if (reply_ == nullptr) {
        return;
    }
    cancelling_ = true;
    reply_->abort();
}

bool Downloader::isRunning() const
{
    return reply_ != nullptr;
}

void Downloader::onReadyRead()
{
    if (http_status_ == 0) {
        http_status_ = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // Anything but content is drained and judged in onFinished, so a
        // 404 body can never become a .part file.
        if (http_status_ == 200 || http_status_ == 206) {
            part_.setFileName(QString::fromStdString(part_path_.string()));
            // A 200 answer to a Range request restarts from zero: the
            // kept bytes would otherwise prefix a full second copy.
            const auto mode = http_status_ == 206
                                    ? QIODevice::WriteOnly | QIODevice::Append
                                    : QIODevice::WriteOnly | QIODevice::Truncate;
            if (!part_.open(mode)) {
                failTransfer(QStringLiteral("cannot write %1: %2")
                                     .arg(part_.fileName(), part_.errorString()),
                             true);
                return;
            }
            if (http_status_ != 206) {
                resume_from_ = 0;
            }
            body_accepted_ = true;
        }
    }
    if (!body_accepted_) {
        reply_->readAll();
        return;
    }
    const QByteArray chunk = reply_->readAll();
    // Flushed per chunk so an interrupted transfer resumes from what the
    // disk really holds, not from what a buffer claimed.
    if (part_.write(chunk) != chunk.size() || !part_.flush()) {
        failTransfer(QStringLiteral("cannot write %1: %2")
                             .arg(part_.fileName(), part_.errorString()),
                     true);
    }
}

void Downloader::onFinished()
{
    if (reply_ == nullptr) {
        return;
    }
    if (cancelling_) {
        part_.close();
        cleanupReply();
        emit cancelled();
        return;
    }
    if (reply_->error() != QNetworkReply::NoError) {
        // Qt's errorString drops the status number; the number is what a
        // report is diagnosed from.
        const int status =
                reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString reason = status >= 400 ? QStringLiteral("server answered %1: %2")
                                                       .arg(status)
                                                       .arg(reply_->errorString())
                                             : reply_->errorString();
        failTransfer(reason, true);
        return;
    }

    onReadyRead();
    if (reply_ == nullptr) {
        return;
    }

    if (!body_accepted_) {
        failTransfer(QStringLiteral("server answered %1").arg(http_status_), true);
        return;
    }

    const auto declared = reply_->header(QNetworkRequest::ContentLengthHeader);
    part_.close();
    if (declared.isValid()
        && quint64(part_.size()) != resume_from_ + declared.toULongLong()) {
        failTransfer(QStringLiteral("transfer ended %1 bytes short of %2")
                             .arg(resume_from_ + declared.toULongLong()
                                  - quint64(part_.size()))
                             .arg(declared.toULongLong()),
                     true);
        return;
    }
    finishTransfer();
}

void Downloader::finishTransfer()
{
    const QDateTime last_modified =
            reply_->header(QNetworkRequest::LastModifiedHeader).toDateTime();

    std::error_code ec;
    std::filesystem::remove(destination_, ec);
    std::filesystem::rename(part_path_, destination_, ec);
    if (ec) {
        failTransfer(QStringLiteral("cannot move %1 into place: %2")
                             .arg(QString::fromStdString(part_path_.string()),
                                  QString::fromStdString(ec.message())),
                     true);
        return;
    }

    if (last_modified.isValid()) {
        const auto stamp = std::chrono::clock_cast<std::chrono::file_clock>(
                std::chrono::sys_seconds(
                        std::chrono::seconds(last_modified.toSecsSinceEpoch())));
        std::filesystem::last_write_time(destination_, stamp, ec);
    }

    std::error_code size_ec;
    const auto final_size = std::filesystem::file_size(destination_, size_ec);
    log_info(kLogComponent,
             "download complete: %s (%llu bytes)",
             destination_.string().c_str(),
             size_ec ? 0ULL : static_cast<unsigned long long>(final_size));

    const QString path = QString::fromStdString(destination_.string());
    cleanupReply();
    emit finished(path);
}

void Downloader::failTransfer(const QString& reason, bool keep_part)
{
    part_.close();
    if (!keep_part) {
        std::error_code ec;
        std::filesystem::remove(part_path_, ec);
    }
    log_error(kLogComponent,
              "download failed: %s (%s)",
              destination_.string().c_str(),
              reason.toUtf8().constData());
    if (reply_ != nullptr) {
        reply_->disconnect(this);
        reply_->abort();
    }
    cleanupReply();
    emit failed(reason);
}

void Downloader::cleanupReply()
{
    if (reply_ != nullptr) {
        reply_->disconnect(this);
        reply_->deleteLater();
        reply_ = nullptr;
    }
}

} // namespace showroom
