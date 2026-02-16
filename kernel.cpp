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
    if (!m_Serial.Initialize(115200)) return FALSE;
    if (!m_Interrupt.Initialize())    return FALSE;
    if (!m_Timer.Initialize())        return FALSE;
    if (!m_USBHCI.Initialize())       return FALSE;

    return TRUE;
}

void CKernel::Run(void)
{
    static const char msg[] = "\nBubblesOS (Rpi 4) initialized.\nCommands: debug, help\n> ";
    m_Screen.Write(msg, sizeof(msg) - 1);
    m_Serial.Write(msg, sizeof(msg) - 1);

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

        // --- 1. Handle Backspace (ASCII 8 or 127) ---
        if (c == '\b' || c == 127)
        {
            if (s_pThis->m_nInputIndex > 0)
            {
                s_pThis->m_nInputIndex--;
                // Visual erase: Move cursor back, print space, move back again
                const char eraseSeq[] = {'\b', ' ', '\b'};
                s_pThis->m_Screen.Write(eraseSeq, 3);
                s_pThis->m_Serial.Write(eraseSeq, 3);
            }
            continue;
        }

        // --- 2. Handle Enter (Execute) ---
        if (c == '\r' || c == '\n')
        {
            s_pThis->m_Screen.Write("\n", 1);
            s_pThis->m_Serial.Write("\n", 1);

            s_pThis->m_InputBuffer[s_pThis->m_nInputIndex] = '\0';

            // Use our manual comparison function instead of strcmp
            if (compare_strings(s_pThis->m_InputBuffer, "debug") == 0)
            {
                const char dMsg[] = "Status: BubblesOS kernel is active.\n";
                s_pThis->m_Screen.Write(dMsg, sizeof(dMsg) - 1);
            }
            else if (compare_strings(s_pThis->m_InputBuffer, "help") == 0)
            {
                const char hMsg[] = "Commands: debug, help\n";
                s_pThis->m_Screen.Write(hMsg, sizeof(hMsg) - 1);
            }
            else if (s_pThis->m_nInputIndex > 0)
            {
                const char uMsg[] = "Unknown command\n";
                s_pThis->m_Screen.Write(uMsg, sizeof(uMsg) - 1);
            }

            s_pThis->m_nInputIndex = 0;
            s_pThis->m_Screen.Write("> ", 2);
            s_pThis->m_Serial.Write("> ", 2);
        }
        // --- 3. Handle Regular Characters ---
        else if (c >= 32 && c <= 126)
        {
            if (s_pThis->m_nInputIndex < (int)sizeof(s_pThis->m_InputBuffer) - 1)
            {
                s_pThis->m_InputBuffer[s_pThis->m_nInputIndex++] = c;
                s_pThis->m_Screen.Write(&c, 1);
                s_pThis->m_Serial.Write(&c, 1);
            }
        }
    }
}