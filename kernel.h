#ifndef _kernel_h
#define _kernel_h

#include <circle/types.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/devicenameservice.h>
#include <circle/machineinfo.h>
#include <circle/util.h>
#include <circle/memory.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/macros.h>
#include <circle/usb/usbmassdevice.h>

class CKernel
{
public:
    CKernel(void);
    ~CKernel(void);

    boolean Initialize(void);
    void Run(void);

private:
    static void KeyPressedHandler(const char* pString);
    void ExecuteCommand(const char* pCommand);

    CActLED             m_ActLED;
    CKernelOptions      m_Options;
    CDeviceNameService  m_DeviceNameService;
    CScreenDevice       m_Screen;
    CSerialDevice       m_Serial;
    CInterruptSystem    m_Interrupt;
    CTimer              m_Timer;
    CUSBHCIDevice       m_USBHCI;
    CFATFileSystem      m_FileSystem;
    CUSBKeyboardDevice* volatile m_pKeyboard;
    char m_InputBuffer[64];
    boolean m_bFileSystemMounted;
    int  m_nInputIndex;
    volatile bool m_bCommandReady;

    static CKernel* s_pThis;
};

#endif