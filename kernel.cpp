#include "kernel.h"
#include <circle/string.h>
#include <assert.h>

extern "C" {
    #include "ubasic/ubasic.h"
    #include "ubasic/tokenizer.h"
}

// Static pointer for callbacks
CKernel* CKernel::s_pThis = 0;
static CScreenDevice* g_pScreen = 0;

// Helper: Check if input starts with cmd followed by space or null
static int is_command(const char* input, const char* cmd)
{
    while (*cmd)
    {
        if (*input++ != *cmd++) return 0;
    }
    return (*input == '\0' || *input == ' ');
}

// Helper: Manual number to string conversion for RAM display
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

CString CKernel::ReadFileContents(const char* pFilename)
{
    CString Content;
    unsigned hFile = m_FileSystem.FileOpen(pFilename);
    
    if (hFile == 0) 
    {
        return Content; 
    }

    char Buffer[512]; 
    unsigned nRead;

    while ((nRead = m_FileSystem.FileRead(hFile, Buffer, sizeof(Buffer) - 1)) > 0) 
    {
        Buffer[nRead] = '\0';
        Content += Buffer;
    }

    m_FileSystem.FileClose(hFile);
    return Content;
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
  m_bFileSystemMounted(FALSE),
  m_nInputIndex(0),
  m_bCommandReady(FALSE)
{
    s_pThis = this;
    g_pScreen = &m_Screen;
    m_ActLED.Blink(5); 
}

// Fixed: Linker was missing this implementation
CKernel::~CKernel(void)
{
    s_pThis = 0;
}

// Fixed: Explicit boolean return to match declaration
boolean CKernel::Initialize(void)
{
    if (!m_Screen.Initialize())       return FALSE;
    if (!m_Interrupt.Initialize())    return FALSE;
    if (!m_Timer.Initialize())        return FALSE;
    if (!m_USBHCI.Initialize())       return FALSE;

    return TRUE;
}

void CKernel::Run(void)
{
    m_Screen.Write("\nBubblesOS (Rpi 4) initialized.\n", 32);

    // Initial mount attempt
    CDevice* pPartition = m_DeviceNameService.GetDevice("umsd1-1", TRUE);
    if (pPartition && m_FileSystem.Mount(pPartition))
    {
        m_bFileSystemMounted = TRUE;
        m_Screen.Write("USB mounted: umsd1-1\n", 21);
    }
    else
    {
        m_Screen.Write("No USB storage detected\n", 25);
    }

    m_Screen.Write("> ", 2);

    while (true)
    {
        m_USBHCI.UpdatePlugAndPlay();

        // Hot-plug keyboard detection
        if (!m_pKeyboard)
        {
            m_pKeyboard = (CUSBKeyboardDevice*)m_DeviceNameService.GetDevice("ukbd1", FALSE);
            if (m_pKeyboard)
            {
                m_pKeyboard->RegisterKeyPressedHandler(KeyPressedHandler);
            }
        }

        // --- Execute commands in main thread context (Fixes ls crash) ---
        if (m_bCommandReady)
        {
            ExecuteCommand(m_InputBuffer);
            
            // Reset for next command
            m_nInputIndex = 0;
            m_bCommandReady = FALSE;
            m_Screen.Write("> ", 2);
        }
    }
}

void CKernel::KeyPressedHandler(const char* pString)
{
    if (s_pThis == 0 || s_pThis->m_bCommandReady) return;

    for (const char* p = pString; *p; ++p)
    {
        char c = *p;

        if (c == '\b' || c == 127)
        {
            if (s_pThis->m_nInputIndex > 0)
            {
                s_pThis->m_nInputIndex--;
                s_pThis->m_Screen.Write("\b \b", 3);
            }
        }
        else if (c == '\r' || c == '\n')
        {
            s_pThis->m_Screen.Write("\n", 1);
            s_pThis->m_InputBuffer[s_pThis->m_nInputIndex] = '\0';
            s_pThis->m_bCommandReady = TRUE; // Hand off to Run()
        }
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

void CKernel::ExecuteCommand(const char* input)
{
    // --- DEBUG ---
    if (is_command(input, "debug"))
    {
        CMachineInfo *pInfo = CMachineInfo::Get();
        CMemorySystem *pMemory = CMemorySystem::Get();
        char buf[64];

        m_Screen.Write("--- System Debug ---\n", 21);
        
        m_Screen.Write("Model: ", 7);
        const char* modelName = pInfo->GetMachineName();
        m_Screen.Write(modelName, strlen(modelName));
        m_Screen.Write("\n", 1);

        m_Screen.Write("RAM: ", 5);
        StringFormatNumber(pMemory->GetMemSize() / (1024 * 1024), buf, sizeof(buf));
        m_Screen.Write(buf, strlen(buf));
        m_Screen.Write(" MB Total\n", 10);
    }
    // --- LS (Fixed logic using Circle strings) ---
    else if (is_command(input, "ls"))
    {
        if (!m_bFileSystemMounted) 
        {
            m_Screen.Write("No filesystem mounted\n", 22);
        }
        else 
        {
            TDirentry Direntry;
            TFindCurrentEntry CurrentEntry;

            unsigned nResult = m_FileSystem.RootFindFirst(&Direntry, &CurrentEntry);

            if (nResult == 0) 
            {
                m_Screen.Write("Directory empty or error\n", 25);
            } 
            else 
            {
                int count = 0;
                while (nResult != 0) 
                {
                    if (!(Direntry.nAttributes & FS_ATTRIB_SYSTEM)) 
                    {
                        CString FileName;
                        FileName.Format("%-14s", Direntry.chTitle);
                        m_Screen.Write((const char*)FileName, FileName.GetLength());
                        
                        if (++count % 5 == 0) m_Screen.Write("\n", 1);
                    }
                    nResult = m_FileSystem.RootFindNext(&Direntry, &CurrentEntry);
                }
                m_Screen.Write("\n", 1);
            }
        }
    }
    // --- CAT ---
    else if (is_command(input, "cat"))
    {
        const char* arg = &input[3];
        while (*arg == ' ') arg++;

        if (*arg == '\0') 
        {
            m_Screen.Write("Usage: cat <file>\n", 18);
        }
        else 
        {
            CString fileData = ReadFileContents(arg);
        
            if (fileData.GetLength() == 0) 
            {
                m_Screen.Write("File not found or empty\n", 24);
            }
            else 
            {
                m_Screen.Write((const char*)fileData, fileData.GetLength());
                m_Screen.Write("\n", 1);
            }
        }
    }
    // --- DEVS ---
    else if (is_command(input, "devs"))
    {
        m_Screen.Write("Discovered Devices:\n", 20);
        m_DeviceNameService.ListDevices(&m_Screen);
    }
    else if (is_command(input, "run"))
    {
        const char* filename = &input[3];
        while (*filename == ' ') filename++; // Skip spaces

        if (*filename == '\0') {
            m_Screen.Write("Usage: run <file.bas>\n", 22);
        } else {
            CString content = ReadFileContents(filename);
            if (content.GetLength() == 0) {
                m_Screen.Write("File not found.\n", 16);
            } else {
                m_Screen.Write("Executing...\n", 13);
            
                ubasic_init((const char*)content);
            
                while(!ubasic_finished()) {
                    ubasic_run();
                }
            
                m_Screen.Write("\nDone.\n", 7);
            }
        }
    }
    else if (strlen(input) > 0)
    {
        m_Screen.Write("Unknown command\n", 16);
    }
}

extern "C" {
    void circle_basic_print(const char *s) {
        if (g_pScreen && s) {
            g_pScreen->Write(s, strlen(s));
        }
    }

    void circle_basic_print_num(int n) {
        if (g_pScreen) {
            char buf[16];
            StringFormatNumber((unsigned int)n, buf, sizeof(buf));
            g_pScreen->Write(buf, strlen(buf));
        }
    }
}