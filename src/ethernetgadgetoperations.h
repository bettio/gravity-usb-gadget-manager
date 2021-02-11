#ifndef ACTIVATEETHERNETGADGET_H
#define ACTIVATEETHERNETGADGET_H

#include <HemeraCore/Operation>

#include <HemeraCore/USBGadgetManager>

class NetworkTechnology;

class ActivateEthernetGadget : public Hemera::Operation
{
    Q_OBJECT

public:
    explicit ActivateEthernetGadget(Hemera::USBGadgetManager::Mode mode, QObject* parent = nullptr);
    virtual ~ActivateEthernetGadget();

protected:
    virtual void startImpl();

private Q_SLOTS:
    void configureKernelModules();
    void configureConnman();
    void configureDHCP();

private:
    Hemera::USBGadgetManager::Mode m_mode;

    // Random IP P2P
    int m_randomRangeP2P1;
    int m_randomRangeP2P2;
};

class DeactivateEthernetGadget : public Hemera::Operation
{
    Q_OBJECT

public:
    explicit DeactivateEthernetGadget(Hemera::USBGadgetManager::Mode mode, QObject* parent = nullptr);
    virtual ~DeactivateEthernetGadget();

protected:
    virtual void startImpl();

private:
    Hemera::USBGadgetManager::Mode m_mode;
};

#endif // ACTIVATEETHERNETGADGET_H
