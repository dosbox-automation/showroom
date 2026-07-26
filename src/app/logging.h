// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_APP_LOGGING_H
#define SHOWROOM_APP_LOGGING_H

// The logger, safe to include next to Qt.
//
// Qt defines `emit` as an empty macro and LogHandler has a virtual
// member function called emit(), so including <QWidget> before
// imported/log.h turns that declaration into a syntax error. The
// imported file keeps augra-engine's exact text - that is the whole
// reason it was copied rather than shared - so the collision is
// resolved here instead of in her code.
//
// Every file in the Qt-linked layer includes this header rather than
// imported/log.h. Including the logger directly happens to work when
// the include order puts it first, and that is not a property worth
// depending on.

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
