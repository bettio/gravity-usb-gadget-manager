#ifndef USBGADGETMANAGERSERVICE_H
#define USBGADGETMANAGERSERVICE_H

#include <HemeraCore/AsyncInitDBusObject>

#include <QtCore/QStringList>
#include <QtCore/QByteArray>

#include <QtDBus/QDBusContext>

class QTimer;
class USBGadgetManagerService : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.USBGadgetManager")

    Q_PROPERTY(uint activeMode                READ activeMode                NOTIFY activeModeChanged)
    Q_PROPERTY(uint availableModes            READ availableModes)
    Q_PROPERTY(bool canDetectCableHotplugging READ canDetectCableHotplugging)
    Q_PROPERTY(uint usbCableStatus            READ usbCableStatus            NOTIFY usbCableStatusChanged)
    Q_PROPERTY(bool systemWideLockActive      READ isSystemWideLockActive    NOTIFY systemWideLockChanged)
    Q_PROPERTY(QString systemWideLockOwner    READ systemWideLockOwner       NOTIFY systemWideLockChanged)
    Q_PROPERTY(QString systemWideLockReason   READ systemWideLockReason      NOTIFY systemWideLockChanged)

public:
    explicit USBGadgetManagerService();
    virtual ~USBGadgetManagerService();

    void Activate(uint mode, const QVariantMap &arguments);
    void Deactivate();

    void AcquireSystemWideLock(const QString &reason);
    void ReleaseSystemWideLock();

    inline bool isSystemWideLockActive() const { return !m_systemWideLockOwner.isEmpty(); }
    inline QString systemWideLockOwner() const { return m_systemWideLockOwner; }
    inline QString systemWideLockReason() const { return m_systemWideLockReason; }

    inline uint activeMode() const { return m_activeMode; }
    inline uint availableModes() const { return m_availableModes; }
    inline bool canDetectCableHotplugging() const { return m_canDetectCableHotplugging; }
    inline uint usbCableStatus() const { return m_activeMode; }

protected:
    virtual void initImpl() override final;

Q_SIGNALS:
    void activeModeChanged();
    void systemWideLockChanged();
    void usbCableStatusChanged();

private:
    QTimer *killerTimer;

    QString m_systemWideLockOwner;
    QString m_systemWideLockReason;

    uint m_activeMode;
    uint m_availableModes;
    bool m_canDetectCableHotplugging;
    uint m_usbCableStatus;
};

#endif // USBGADGETMANAGERSERVICE_H
