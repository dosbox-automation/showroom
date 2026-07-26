// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("dosbox-automation showroom"));
    window.show();

    return QApplication::exec();
}
