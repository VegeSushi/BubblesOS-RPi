#ifndef _kernel_h
#define _kernel_h

#include <circle/types.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/devicenameservice.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/interrupt.h>
#include <circle/timer.h>

class CKernel
{
public:
    CKernel(void);
    ~CKernel(void);

    boolean Initialize(void);
    void Run(void);

private:
    static void KeyPressedHandler(const char* pString);

    CActLED             m_ActLED;
    CKernelOptions      m_Options;
    CDeviceNameService  m_DeviceNameService;
    CScreenDevice       m_Screen;
    CSerialDevice       m_Serial;
    CInterruptSystem    m_Interrupt;
    CTimer              m_Timer;
    CUSBHCIDevice       m_USBHCI;
    CUSBKeyboardDevice* volatile m_pKeyboard;

    // Buffer to hold typed characters
    char m_InputBuffer[64];
    int  m_nInputIndex;

    static CKernel* s_pThis;
};

#endif