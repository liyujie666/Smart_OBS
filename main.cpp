#include "mainwindow.h"
#include <QApplication>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
// #include "VLD/vld.h"

int main(int argc, char *argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    QApplication a(argc, argv);
    qRegisterMetaType<CameraSetting>("CameraSetting");
    qRegisterMetaType<ScreenSetting>("ScreenSetting");
    qRegisterMetaType<FileSetting>("FileSetting");
    MainWindow w;
    w.show();
    int ret = a.exec();
    AudioMixerDialog::destroyInstance();
    return ret;
}
