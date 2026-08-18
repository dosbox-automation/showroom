// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/download_plan.h"

#include <QString>
#include <QUrl>

namespace showroom {

std::vector<DownloadPlan> downloadPlansFor(const GameDefinition& game)
{
    std::vector<DownloadPlan> plans;
    for (const GameSource& source : game.sources()) {
        std::string raw_name;
        if (source.filename.has_value()) {
            raw_name = *source.filename;
        } else {
            // fileName() decodes percent-escapes, so sanitizing sees the
            // real name, not its encoding.
            raw_name = QUrl(QString::fromStdString(source.url))
                               .fileName()
                               .toStdString();
        }
        const std::string filename = sanitizedPathComponent(raw_name);
        if (filename.empty()) {
            continue;
        }
        plans.push_back(DownloadPlan{source.url,
                                     filename,
                                     source.size,
                                     source.install_type,
                                     source.target_subdir});
    }
    return plans;
}

std::optional<DownloadPlan> downloadPlanFor(const GameDefinition& game)
{
    auto plans = downloadPlansFor(game);
    if (plans.empty()) {
        return std::nullopt;
    }
    return std::move(plans.front());
}

std::optional<DownloadPlan> archivePlanOnDisk(const GameDefinition& game,
                                              const std::filesystem::path& downloads_dir)
{
    std::error_code ec;
    for (auto& plan : downloadPlansFor(game)) {
        if (std::filesystem::is_regular_file(downloads_dir / plan.filename, ec)) {
            return std::move(plan);
        }
    }
    return std::nullopt;
}

} // namespace showroom
