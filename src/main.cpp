#include <QtCore/QCoreApplication>

#include <HemeraCore/Operation>

#include "usbgadgetmanagerservice.h"

#include <systemd/sd-daemon.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    USBGadgetManagerService *usbGadgetManagerService = new USBGadgetManagerService;

    QObject::connect(usbGadgetManagerService->init(), &Hemera::Operation::finished, [] (Hemera::Operation *op) {
        if (op->isError()) {
            sd_notifyf(0, "STATUS=Could not initialize USB Gadget Manager. Reported error was: %s - %s.\n"
                          "ERRNO=15", op->errorName().toLatin1().constData(), op->errorMessage().toLatin1().constData());
            QCoreApplication::instance()->exit(15);
            return;
        }
        sd_notify(0, "STATUS=USB Gadget Manager is active.\n");
    });

    int ret = app.exec();

    delete usbGadgetManagerService;

    return ret;
}
