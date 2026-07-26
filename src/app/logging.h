// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_APP_LOGGING_H
#define SHOWROOM_APP_LOGGING_H

// Qt's empty `emit` macro collides with LogHandler::emit(), and the
// imported logger keeps augra-engine's exact text, so the collision is
// resolved here. Include this, never imported/log.h directly.

#ifdef emit
#define SHOWROOM_QT_EMIT_WAS_DEFINED
#undef emit
#endif

#include "imported/log.h"

#ifdef SHOWROOM_QT_EMIT_WAS_DEFINED
#define emit
#undef SHOWROOM_QT_EMIT_WAS_DEFINED
#endif

#endif // SHOWROOM_APP_LOGGING_H
