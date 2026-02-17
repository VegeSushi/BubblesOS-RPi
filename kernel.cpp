#include "kernel.h"
#include <assert.h>

// Static pointer for callbacks
CKernel* CKernel::s_pThis = 0;

// A simple, manual string comparison function to avoid cstring/library issues
static int compare_strings(const char* s1, const char* s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void StringFormatNumber(unsigned int n, char* buf, size_t bufSize) {
    if (bufSize == 0) return;
    if (n == 0) {
        buf[0] = '0'; buf[1] = '\0';
        return;
    }
    char tmp[12];
    int i = 0;
    while (n > 0 && i < 11) {
        tmp[i++] = (n % 10) + '0';
        n /= 10;
    }
    int j = 0;
    while (i > 0 && (size_t)j < bufSize - 1) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

CKernel::CKernel(void)
: m_ActLED(),
  m_Options(),
  m_DeviceNameService(),
  m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
  m_Serial(),
  m_Interrupt(),
  m_Timer(&m_Interrupt),
  m_USBHCI(&m_Interrupt, &m_Timer, TRUE),
  m_pKeyboard(0),
  m_nInputIndex(0)
{
    s_pThis = this;
    m_ActLED.Blink(5); 
}

CKernel::~CKernel(void)
{
    s_pThis = 0;
}

boolean CKernel::Initialize(void)
{
    if (!m_Screen.Initialize())       return FALSE;
    // Serial initialization removed
    if (!m_Interrupt.Initialize())    return FALSE;
    if (!m_Timer.Initialize())        return FALSE;
    if (!m_USBHCI.Initialize())       return FALSE;

    return TRUE;
}

void CKernel::Run(void)
{
    // Original boot message preserved
    static const char msg[] = "\nBubblesOS (Rpi 4) initialized.\n> ";
    m_Screen.Write(msg, sizeof(msg) - 1);

    while (true)
    {
        m_USBHCI.UpdatePlugAndPlay();

        if (!m_pKeyboard)
        {
            m_pKeyboard = (CUSBKeyboardDevice*)m_DeviceNameService.GetDevice("ukbd1", FALSE);
            if (m_pKeyboard)
            {
                m_pKeyboard->RegisterKeyPressedHandler(KeyPressedHandler);
            }
        }
    }
}

void CKernel::KeyPressedHandler(const char* pString)
{
    if (s_pThis == 0) return;

    for (const char* p = pString; *p; ++p)
    {
        char c = *p;

        // --- 1. Handle Backspace ---
        if (c == '\b' || c == 127)
        {
            if (s_pThis->m_nInputIndex > 0)
            {
                s_pThis->m_nInputIndex--;
                const char eraseSeq[] = {'\b', ' ', '\b'};
                s_pThis->m_Screen.Write(eraseSeq, 3);
            }
            continue;
        }

        // --- 2. Handle Enter ---
        if (c == '\r' || c == '\n')
        {
            s_pThis->m_Screen.Write("\n", 1);
            s_pThis->m_InputBuffer[s_pThis->m_nInputIndex] = '\0';

            if (compare_strings(s_pThis->m_InputBuffer, "debug") == 0)
            {
                CMachineInfo *pInfo = CMachineInfo::Get();
                CMemorySystem *pMemory = CMemorySystem::Get();
                char buf[64];

                s_pThis->m_Screen.Write("--- System Debug ---\n", 21);

                // 1. Board Info
                s_pThis->m_Screen.Write("Model: ", 7);
                const char* modelName = pInfo->GetMachineName();
                s_pThis->m_Screen.Write(modelName, strlen(modelName));
                s_pThis->m_Screen.Write("\n", 1);

                // 2. RAM Info
                s_pThis->m_Screen.Write("RAM: ", 5);
                size_t nRAMBytes = pMemory->GetMemSize();
                StringFormatNumber(nRAMBytes / (1024 * 1024), buf, sizeof(buf));
                s_pThis->m_Screen.Write(buf, strlen(buf));
                s_pThis->m_Screen.Write(" MB Total\n", 10);
            }
            else if (s_pThis->m_nInputIndex > 0)
            {
                const char uMsg[] = "Unknown command\n";
                s_pThis->m_Screen.Write(uMsg, sizeof(uMsg) - 1);
            }

            s_pThis->m_nInputIndex = 0;
            s_pThis->m_Screen.Write("> ", 2);
        }
        // --- 3. Handle Regular Characters ---
        else if (c >= 32 && c <= 126)
        {
            if (s_pThis->m_nInputIndex < (int)sizeof(s_pThis->m_InputBuffer) - 1)
            {
                s_pThis->m_InputBuffer[s_pThis->m_nInputIndex++] = c;
                s_pThis->m_Screen.Write(&c, 1);
            }
        }
    }
}