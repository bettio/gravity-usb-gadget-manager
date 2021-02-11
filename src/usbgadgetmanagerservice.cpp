#include "usbgadgetmanagerservice.h"

#include "ethernetgadgetoperations.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <HemeraCore/Literals>
#include <HemeraCore/Operation>
#include <HemeraCore/USBGadgetManager>

#include <systemd/sd-daemon.h>
#include <systemd/sd-journal.h>

#include "usbgadgetmanageradaptor.h"

/* 60 seconds */
constexpr int killerInterval() { return 60 * 1000; }

USBGadgetManagerService::USBGadgetManagerService()
    : AsyncInitDBusObject(nullptr)
    , killerTimer(new QTimer(this))
    , m_activeMode(static_cast<uint>(Hemera::USBGadgetManager::Mode::None))
    // TODO: These have to be detected at runtime.
    , m_availableModes(static_cast<uint>(Hemera::USBGadgetManager::Mode::EthernetP2P | Hemera::USBGadgetManager::Mode::EthernetTethering))
{
}

USBGadgetManagerService::~USBGadgetManagerService()
{
}

void USBGadgetManagerService::initImpl()
{
    if (!QDBusConnection::systemBus().registerService(Hemera::Literals::literal(Hemera::Literals::DBus::usbGadgetManagerService()))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerServiceFailed()),
                     QStringLiteral("Could not register USB Gadget Manager service on DBus. This means either a wrong installation or a corrupted instance."));
        return;
    }
    if (!QDBusConnection::systemBus().registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::usbGadgetManagerPath()), this)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerObjectFailed()),
                     QStringLiteral("Could not register USB Gadget Manager object on DBus. This means either a wrong installation or a corrupted instance."));
        return;
    }
    new USBGadgetManagerAdaptor(this);

    connect(killerTimer, &QTimer::timeout, [] {
        sd_notify(0, "STATUS=USB Gadget Manager is shutting down due to inactivity.\n");
        QCoreApplication::instance()->quit();
    });
    killerTimer->setInterval(killerInterval());
    killerTimer->setSingleShot(true);

    // Don't start it.
    //killerTimer->start();

    setReady();
}

void USBGadgetManagerService::Activate(uint mode, const QVariantMap& arguments)
{
    if (!calledFromDBus()) {
        qWarning() << "Something's wrong with callers!";
        return;
    }

    // Do we have a lock?
    if (!m_systemWideLockOwner.isEmpty() && m_systemWideLockOwner != message().service()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("You have requested activation, but %1 is holding the system lock. "
                                      "You cannot activate or deactivate while somebody else is holding the lock.").arg(m_systemWideLockOwner));
        return;
    }

    // Is there anything active?
    if (m_activeMode != static_cast<uint>(Hemera::USBGadgetManager::Mode::None)) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("You have requested activation, but there's already an active mode on the USB Gadget. "
                                      "Call Deactivate first, then retry."));
        return;
    }

    // Is it available at runtime?
    if (!(mode & m_availableModes)) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("The requested mode is not available on this device."));
        return;
    }

    Hemera::Operation *op = nullptr;

    switch (static_cast<Hemera::USBGadgetManager::Mode>(mode)) {
        case Hemera::USBGadgetManager::Mode::EthernetP2P:
            op = new ActivateEthernetGadget(Hemera::USBGadgetManager::Mode::EthernetP2P, this);
            break;
        case Hemera::USBGadgetManager::Mode::EthernetTethering:
            op = new ActivateEthernetGadget(Hemera::USBGadgetManager::Mode::EthernetTethering, this);
            break;
        default:
            sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                           QStringLiteral("The mode you requested is either not implemented or not available."));
            return;
    }

    // It's ok if we're here, delay reply.
    setDelayedReply(true);

    QDBusMessage originMessage = message();
    QDBusConnection originConnection = connection();

    connect(op, &Hemera::Operation::finished, [this, op, originConnection, originMessage, mode] {
        if (op->isError()) {
            originConnection.send(originMessage.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            originConnection.send(originMessage.createReply());

            m_activeMode = mode;
            Q_EMIT activeModeChanged();
        }
    });
}

void USBGadgetManagerService::Deactivate()
{
    if (!calledFromDBus()) {
        qWarning() << "Something's wrong with callers!";
        return;
    }

    // Do we have a lock?
    if (!m_systemWideLockOwner.isEmpty() && m_systemWideLockOwner != message().service()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("You have requested deactivation, but %1 is holding the system lock. "
                                      "You cannot activate or deactivate while somebody else is holding the lock.").arg(m_systemWideLockOwner));
        return;
    }

    // Is there anything active?
    if (m_activeMode == static_cast<uint>(Hemera::USBGadgetManager::Mode::None)) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("You have requested deactivation, but there's no active modes on the USB Gadget."));
        return;
    }

    // Ok now.
    Hemera::Operation *op = nullptr;

    switch (static_cast<Hemera::USBGadgetManager::Mode>(m_activeMode)) {
        case Hemera::USBGadgetManager::Mode::EthernetP2P:
            op = new DeactivateEthernetGadget(Hemera::USBGadgetManager::Mode::EthernetP2P, this);
            break;
        case Hemera::USBGadgetManager::Mode::EthernetTethering:
            op = new DeactivateEthernetGadget(Hemera::USBGadgetManager::Mode::EthernetTethering, this);
            break;
        default:
            sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                           QStringLiteral("The mode you requested is either not implemented or not available."));
            return;
    }

    // It's ok if we're here, delay reply.
    setDelayedReply(true);

    QDBusMessage originMessage = message();
    QDBusConnection originConnection = connection();

    connect(op, &Hemera::Operation::finished, [this, op, originConnection, originMessage] {
        if (op->isError()) {
            originConnection.send(originMessage.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            originConnection.send(originMessage.createReply());

            m_activeMode = static_cast<uint>(Hemera::USBGadgetManager::Mode::None);
            Q_EMIT activeModeChanged();
        }
    });
}

void USBGadgetManagerService::AcquireSystemWideLock(const QString& reason)
{
    if (!calledFromDBus()) {
        qWarning() << "Something's wrong with callers!";
        return;
    }

    // Do we have a lock available already?
    if (!m_systemWideLockOwner.isEmpty()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("The lock is already held by %1.").arg(m_systemWideLockOwner));
        return;
    }

    // Check our service
    QString newOwner = message().service();
    if (newOwner.isEmpty()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                       QStringLiteral("Couldn't retrieve the identity of the acquirer. This is an internal error."));
        return;
    }

    // Ok then.
    m_systemWideLockOwner = newOwner;
    m_systemWideLockReason = reason;
    Q_EMIT systemWideLockChanged();

    // Watch out for bus changes now doe.

}

void USBGadgetManagerService::ReleaseSystemWideLock()
{
    if (!calledFromDBus()) {
        qWarning() << "Something's wrong with callers!";
        return;
    }

    // Do we have a lock available already?
    if (m_systemWideLockOwner.isEmpty()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("There is currently no lock active."));
        return;
    }

    // Check our service
    if (m_systemWideLockOwner != message().service()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                       QStringLiteral("You have been identified as %1, while the system lock is held by %2. "
                                      "Only %2 can release the lock.").arg(message().service(), m_systemWideLockOwner));
        return;
    }

    // Ok then.
    m_systemWideLockOwner.clear();
    Q_EMIT systemWideLockChanged();
}
