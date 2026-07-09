#include "Stasis.h"

#define LOG_FILE L"Stasis.log"

static FILE* g_LogFile = NULL;
static CRITICAL_SECTION g_LogCs;

void InitLog(void)
{
    InitializeCriticalSection(&g_LogCs);
    g_LogFile = _wfopen(LOG_FILE, L"w, ccs=UNICODE");
    if (g_LogFile)
        fwprintf(g_LogFile, L"=== Stasis Log Started ===\n");
    g_State.logFile = g_LogFile;
}

void CloseLog(void)
{
    if (g_LogFile)
    {
        fwprintf(g_LogFile, L"=== Stasis Log Closed ===\n");
        fclose(g_LogFile);
        g_LogFile = NULL;
    }
    DeleteCriticalSection(&g_LogCs);
}

void LogEvent(const WCHAR* format, ...)
{
    EnterCriticalSection(&g_LogCs);
    if (!g_LogFile)
    {
        LeaveCriticalSection(&g_LogCs);
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    fwprintf(g_LogFile, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, format);
    vfwprintf(g_LogFile, format, args);
    va_end(args);
    fwprintf(g_LogFile, L"\n");
    fflush(g_LogFile);

    // 简单截断：若文件过大则重新截取最后500行
    fseek(g_LogFile, 0, SEEK_END);
    long size = ftell(g_LogFile);
    if (size > 50 * 1024)
    {
        fclose(g_LogFile);
        FILE* fSrc = _wfopen(LOG_FILE, L"r, ccs=UNICODE");
        if (fSrc)
        {
            WCHAR* lines[1000] = {0};
            int lineCount = 0;
            WCHAR buf[512];
            while (fgetws(buf, 512, fSrc) && lineCount < 1000)
            {
                size_t len = wcslen(buf);
                lines[lineCount] = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
                if (lines[lineCount])
                {
                    wcscpy_s(lines[lineCount], len + 1, buf);
                    lineCount++;
                }
            }
            fclose(fSrc);

            g_LogFile = _wfopen(LOG_FILE, L"w, ccs=UNICODE");
            if (g_LogFile)
            {
                int start = lineCount > LOG_MAX_LINES ? lineCount - LOG_MAX_LINES : 0;
                for (int i = start; i < lineCount; i++)
                    fputws(lines[i], g_LogFile);
                for (int i = 0; i < lineCount; i++) free(lines[i]);
            }
        }
        if (!g_LogFile)
            g_LogFile = _wfopen(LOG_FILE, L"a, ccs=UNICODE");
    }
    LeaveCriticalSection(&g_LogCs);
}