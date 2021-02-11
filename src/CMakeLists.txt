set(USBGadgetManager_SRCS
    main.cpp
    ethernetgadgetoperations.cpp
    usbgadgetmanagerservice.cpp
)

qt5_add_dbus_adaptor(USBGadgetManager_SRCS ${HEMERAQTSDK_DBUS_INTERFACES_DIR}/com.ispirata.Hemera.USBGadgetManager.xml
                     usbgadgetmanagerservice.h USBGadgetManagerService)

# final lib
add_executable(gravity-usb-gadget-manager ${USBGadgetManager_SRCS})

target_link_libraries(gravity-usb-gadget-manager
                      Qt5::Core
                      Qt5::DBus
                      HemeraQt5SDK::Core
                      ${UDEV_LIBS}
                      ${LIBSYSTEMD_DAEMON_LIBRARIES}
                      ${LIBSYSTEMD_JOURNAL_LIBRARIES}
                      ${LIBCONNMANQT5_LIBRARIES})

configure_file(dbus-com.ispirata.Hemera.USBGadgetManager.service.in "${CMAKE_CURRENT_BINARY_DIR}/dbus-com.ispirata.Hemera.USBGadgetManager.service" @ONLY)

# Install phase
install(TARGETS gravity-usb-gadget-manager
        EXPORT  USBGadgetManagerTargets
        RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
        COMPONENT gravity-usb-gadget-manager)

install(FILES com.ispirata.Hemera.USBGadgetManager.service
        DESTINATION ${GRAVITY_USB_GADGET_MANAGER_DBUS_SYSTEM_ACTIVATION_DIR}
        COMPONENT gravity-usb-gadget-manager)

install(FILES com.ispirata.Hemera.USBGadgetManager.conf
        DESTINATION ${HA_DBUS_SYSTEM_POLICY_DIR}
        COMPONENT gravity-usb-gadget-manager)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dbus-com.ispirata.Hemera.USBGadgetManager.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT gravity-usb-gadget-manager)

# Services
install(FILES gadget-mac-address.service dnsmasq-usb-gadget.service
        DESTINATION ${HA_SYSTEMD_SYSTEM_DIR}
        COMPONENT data)
