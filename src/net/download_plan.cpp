// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/download_plan.h"

#include <QString>
#include <QUrl>

namespace showroom {

std::optional<DownloadPlan> downloadPlanFor(const GameDefinition& game)
{
    if (game.sources().empty()) {
        return std::nullopt;
    }
    const GameSource& source = game.sources().front();

    std::string raw_name;
    if (source.filename.has_value()) {
        raw_name = *source.filename;
    } else {
        // fileName() decodes percent-escapes, so sanitizing sees the real
        // name, not its encoding.
        raw_name = QUrl(QString::fromStdString(source.url)).fileName().toStdString();
    }
    const std::string filename = sanitizedPathComponent(raw_name);
    if (filename.empty()) {
        return std::nullopt;
    }

    return DownloadPlan{source.url, filename, source.size};
}

} // namespace showroom
