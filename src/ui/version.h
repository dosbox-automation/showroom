// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_UI_VERSION_H
#define SHOWROOM_UI_VERSION_H

namespace showroom {

// The showroom's own version, from the CMake project version. It tracks
// the application, not the engine it demonstrates.
inline constexpr const char* kShowroomVersion = SHOWROOM_VERSION;

// The version of the bundled dosbox-automation, which is the badge in
// the sidebar: the showroom exists to show that engine off, so its
// version is the headline.
//
// Set at configure time until phase 4, where a real engine process gets
// started and can be asked what it is. A build-time constant is honest
// about being a build-time constant; a hardcoded literal in a widget
// would quietly go stale.
inline constexpr const char* kBundledEngineVersion = SHOWROOM_ENGINE_VERSION;

} // namespace showroom

#endif // SHOWROOM_UI_VERSION_H
