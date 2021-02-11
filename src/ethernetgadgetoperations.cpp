#include "ethernetgadgetoperations.h"

#include <HemeraCore/Literals>

#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

// connman
#include <connman-qt5/networkmanager.h>

#define ETHERNET_GADGET_MODULE "g_ether"

NetworkTechnology *getTechnologyReady(const QString &name)
{
    NetworkManager *manager = NetworkManagerFactory::createInstance();

    NetworkTechnology *technology = manager->getTechnology(name);

    // Signal might come in more than one time.
    while (!technology) {
        // Wait for it to come up
        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        QObject::connect(manager, &NetworkManager::technologiesChanged, &e, &QEventLoop::quit);
        QObject::connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        if (!t.isActive()) {
            return nullptr;
        }
        technology = manager->getTechnology(name);
    }

    // We have it.
    if (technology->name().isEmpty()) {
        // We have to wait for them to come up
        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        QObject::connect(technology, &NetworkTechnology::propertiesReady, &e, &QEventLoop::quit);
        QObject::connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        if (!t.isActive()) {
            return nullptr;
        }
    }

    // Power it up.
    if (!technology->powered()) {
        technology->setPowered(true);
        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        QObject::connect(technology, &NetworkTechnology::poweredChanged, &e, &QEventLoop::quit);
        QObject::connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        // There's a bug here in how libconnman-qt manages properties, for any reason. So, check the timer.
        if (!technology->powered() && !t.isActive()) {
            return nullptr;
        }
    }

    return technology;
}

ActivateEthernetGadget::ActivateEthernetGadget(Hemera::USBGadgetManager::Mode mode, QObject* parent)
    : Operation(parent)
    , m_mode(mode)
{
}

ActivateEthernetGadget::~ActivateEthernetGadget()
{
}

void ActivateEthernetGadget::startImpl()
{
    // We start by configuring kernel modules
    configureKernelModules();
}

void ActivateEthernetGadget::configureKernelModules()
{
    // Is the module already loaded?
    QProcess process;
    process.execute(QStringLiteral("/sbin/lsmod"));
    process.waitForFinished(2000);
    if (process.readAllStandardOutput().contains(ETHERNET_GADGET_MODULE)) {
        // Ready for connman
        QTimer::singleShot(0, this, SLOT(configureConnman()));
        return;
    }

    // Load it.
    QProcess loadProcess;
    loadProcess.execute(QStringLiteral("/sbin/modprobe"), QStringList() << QLatin1String(ETHERNET_GADGET_MODULE));
    loadProcess.waitForFinished();
    if (loadProcess.exitCode() != 0) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                             QLatin1String(loadProcess.readAllStandardError()));
        return;
    }

    // Good to go. Let's handle connman now.
    QTimer::singleShot(0, this, SLOT(configureConnman()));
}

void ActivateEthernetGadget::configureConnman()
{
    // First of all, get our technology
    NetworkTechnology *gadgetTechnology = getTechnologyReady(QStringLiteral("gadget"));
    if (!gadgetTechnology) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                             QLatin1String("Could not retrieve gadget on the Network Manager"));
        return;
    }

    // Ok, now. Let's configure
    if (m_mode == Hemera::USBGadgetManager::Mode::EthernetP2P) {
        // We have to generate a random IP.
        m_randomRangeP2P1 = qrand() % 255;
        m_randomRangeP2P2 = (qrand()  % 255) & 248;

        NetworkManager *manager = NetworkManagerFactory::createInstance();
        QVector< NetworkService* > services = manager->getServices(QStringLiteral("gadget"));
        if (services.size() <= 0) {
            // Some grace time before we die. The service might be on its way
            QEventLoop e;
            QTimer t;
            t.setSingleShot(true);
            t.start(5000);
            QObject::connect(manager, &NetworkManager::servicesChanged, &e, &QEventLoop::quit);
            QObject::connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
            e.exec();
            services = manager->getServices(QStringLiteral("gadget"));
            if (services.size() <= 0) {
                setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                                     QLatin1String("No networking services found for the Gadget."));
                return;
            }
        }
        NetworkService *service = services.first();

        {
            QVariantMap ipv4Config;
            ipv4Config.insert(QStringLiteral("Method"), QStringLiteral("manual"));
            ipv4Config.insert(QStringLiteral("Address"), QStringLiteral("169.254.%1.%2").arg(m_randomRangeP2P1).arg(m_randomRangeP2P2 + 1));
            ipv4Config.insert(QStringLiteral("Netmask"), QStringLiteral("255.255.255.248"));
            service->setIpv4Config(ipv4Config);

            // Wait for config to change
            QEventLoop e;
            QTimer t;
            t.setSingleShot(true);
            t.start(5000);
            connect(service, &NetworkService::ipv4ConfigChanged, &e, &QEventLoop::quit);
            connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
            e.exec();
            if (service->ipv4Config().value(QStringLiteral("Method")) != QStringLiteral("manual")) {
                setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                                    QLatin1String("Could not configure IPv4 for Gadget."));
                return;
            }
        }


        // We have to try and connect.
        service->requestConnect();

        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        connect(service, &NetworkService::connectedChanged, &e, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        if (!service->connected()) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                                 QLatin1String("Could not connect Gadget to static network route."));
            return;
        }
    } else {
        // That's way easier.
        gadgetTechnology->setTethering(true);
        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        connect(gadgetTechnology, &NetworkTechnology::tetheringChanged, &e, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        if (!gadgetTechnology->tethering()) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                                 QLatin1String("Could not set up Tethering on the Gadget."));
            return;
        }
    }

    // Yo
    configureDHCP();
}

void ActivateEthernetGadget::configureDHCP()
{
    if (m_mode == Hemera::USBGadgetManager::Mode::EthernetTethering) {
        // Connman already did this for us.
        setFinished();
        return;
    }

    QString configurationFilePayload = QStringLiteral(
"port=0\n"
"interface=%1\n"
"bind-interfaces\n"
"dhcp-range=169.254.%2.%3,169.254.%2.%4,255.255.255.248,12h\n"
"dhcp-option=3\n"
"dhcp-option=6\n"
    ).arg(QStringLiteral("usb0")).arg(m_randomRangeP2P1).arg(m_randomRangeP2P2 + 2).arg(m_randomRangeP2P2 + 4);

    {
        QFile configFile(QStringLiteral("/tmp/dnsmasq-volatile.conf"));
        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                                    QLatin1String("Could not write configuration gadget for P2P connection."));
            return;
        }
        configFile.write(configurationFilePayload.toLatin1());
        configFile.flush();
        configFile.close();
    }

    // We can now start our service
    QProcess process;
    process.start(QStringLiteral("systemctl start dnsmasq-usb-gadget.service"));
    process.waitForFinished();

    if (process.exitCode() != 0) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                             QLatin1String(process.readAllStandardError()));
        return;
    }

    // Whew.
    setFinished();
}



///////////////////

DeactivateEthernetGadget::DeactivateEthernetGadget(Hemera::USBGadgetManager::Mode mode, QObject* parent)
    : Operation(parent)
    , m_mode(mode)
{
}

DeactivateEthernetGadget::~DeactivateEthernetGadget()
{
}

void DeactivateEthernetGadget::startImpl()
{
    // Bring down dnsmasq
    QProcess unloadProcess;
    unloadProcess.start(QStringLiteral("systemctl stop dnsmasq-usb-gadget.service"));
    unloadProcess.waitForFinished();

    if (unloadProcess.exitCode() != 0) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                             QLatin1String(unloadProcess.readAllStandardError()));
        return;
    }

    // First of all, get our technology
    NetworkTechnology *gadgetTechnology = getTechnologyReady(QStringLiteral("gadget"));
    if (!gadgetTechnology) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                             QLatin1String("Could not retrieve gadget on the Network Manager"));
        return;
    }

    if (m_mode == Hemera::USBGadgetManager::Mode::EthernetP2P) {
        NetworkManager *manager = NetworkManagerFactory::createInstance();
        QVector< NetworkService* > services = manager->getServices(QStringLiteral("gadget"));
        if (services.size() <= 0) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                                 QLatin1String("No networking services found for the Gadget. The cable is likely unplugged."));
            return;
        }
        NetworkService *service = services.first();

        // We have to disconnect.
        service->requestDisconnect();

        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        connect(service, &NetworkService::connectedChanged, &e, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        if (service->connected()) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                                 QLatin1String("Could not disconnect Gadget from static network route."));
            return;
        }
    } else {
        // Shut down tethering
        gadgetTechnology->setTethering(false);
        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        connect(gadgetTechnology, &NetworkTechnology::tetheringChanged, &e, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        if (gadgetTechnology->tethering()) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                                 QLatin1String("Could not bring down Tethering on the Gadget."));
            return;
        }
    }

    // Power it down.
    if (gadgetTechnology->powered()) {
        gadgetTechnology->setPowered(false);
        QEventLoop e;
        QTimer t;
        t.setSingleShot(true);
        t.start(5000);
        connect(gadgetTechnology, &NetworkTechnology::poweredChanged, &e, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &e, &QEventLoop::quit);
        e.exec();
        if (gadgetTechnology->powered()) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                                 QLatin1String("Could not power down Gadget on the Network Manager"));
            return;
        }
    }

    // Now, remove the module safely.
    QProcess process;
    process.execute(QStringLiteral("/sbin/rmmod"), QStringList() << QLatin1String(ETHERNET_GADGET_MODULE));
    process.waitForFinished();
    if (process.exitCode() != 0) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                             QLatin1String(process.readAllStandardError()));
        return;
    }

    // We're done.
    setFinished();
}

