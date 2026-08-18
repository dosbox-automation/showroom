// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_VERSION_H
#define SHOWROOM_UI_VERSION_H

namespace showroom {

// Tracks the application, not the engine it demonstrates.
inline constexpr const char* kShowroomVersion = SHOWROOM_VERSION;

// The badge in the sidebar, set at configure time until a running engine
// can be asked what it is.
inline constexpr const char* kBundledEngineVersion = SHOWROOM_ENGINE_VERSION;

// "unknown" in a tree that was configured without git and without the
// build script passing -D BUILD_GIT_HASH.
inline constexpr const char* kShowroomGitHash = SHOWROOM_GIT_HASH;

} // namespace showroom

#endif // SHOWROOM_UI_VERSION_H
