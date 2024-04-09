#include <Windows.h>
#include <cstdio>
#include <tlhelp32.h>
#include <Psapi.h>

#include "LittleBigMouseDaemon.h"
#include "MouseEngine.h"
#include "Hooker.h"
#include "RemoteServerSocket.h"

DWORD GetParentPid(const DWORD pid)
{
    HANDLE h = nullptr;
    PROCESSENTRY32 pe = { 0 };
    DWORD ppid = 0;
    pe.dwSize = sizeof(PROCESSENTRY32);
    h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if( Process32First(h, &pe)) 
    {
        do 
        {
            if (pe.th32ProcessID == pid) 
            {
                ppid = pe.th32ParentProcessID;
                break;
            }
        } while( Process32Next(h, &pe));
    }
    CloseHandle(h);
    return (ppid);
}

DWORD GetProcessName(const DWORD pid, LPWSTR fname, DWORD size)
{
    HANDLE h = nullptr;
    DWORD e = 0;
	h = OpenProcess
    (
    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
    FALSE,
    pid
    );

	if (h) 
    {
        if (GetModuleFileNameEx(h, nullptr, fname, size) == 0)
        {
        	e = GetLastError();
        }
        CloseHandle(h);
    }
    else
    {
        e = GetLastError();
    }
    return e;
}

std::string GetParentProcess()
{
	wchar_t fname[MAX_PATH] = {0};
	const DWORD pid = GetCurrentProcessId();
	const DWORD ppid = GetParentPid(pid);
    DWORD e = GetProcessName(ppid, fname, MAX_PATH);
	std::wstring ws(fname);
	return std::string(ws.begin(), ws.end());
}

static std::string GetEnv(const std::wstring& varName)
{
    std::wstring str;
    DWORD len = GetEnvironmentVariableW(varName.c_str(), NULL, 0);
    if (len > 0)
    {
        str.resize(len);
        str.resize(GetEnvironmentVariableW(varName.c_str(), &str[0], len));
    }
    return std::string(str.begin(), str.end());
}

static int GetEnvInt(const std::wstring& varName, int def = -1)
{
    int num = def;
    std::string envar = GetEnv(varName);
    try {
        num = std::stoi(envar);
    }
    catch (std::invalid_argument& e) {
        //std::cout << "That is not a valid number." << '\n';
    }
    catch (std::out_of_range& e) {
        //std::cout << "That is not a valid number." << '\n';
    }
    return num;
}

int main(int argc, char *argv[]){

	constexpr LPCWSTR szUniqueNamedMutex = L"LittleBigMouse_Daemon";

	HANDLE hHandle = CreateMutex(nullptr, TRUE, szUniqueNamedMutex);
	if( ERROR_ALREADY_EXISTS == GetLastError() )
	{
	  // Program already running somewhere
        #if defined(_DEBUG)
		std::cout << "Program already running\n";
		#endif
        if(hHandle)
        {
		    CloseHandle (hHandle);
        }
		return(1); // Exit program
	}

    #if !defined(_DEBUG)
    ShowWindow( GetConsoleWindow(), SW_HIDE );
    #endif

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );

    RemoteServerSocket server;
    MouseEngine engine;
    Hooker hook;

    // Gather environment variables for the engine

    int thresh = GetEnvInt(L"LBM_CC_THRESHOLD");
#if defined(_DEBUG)
    std::cout << "LBM_CC_THRESHOLD is: " << thresh << '\n';
#endif
    if (thresh != -1) {
        engine.controlCrossingThreshold = thresh;
    }

    std::string envar = GetEnv(L"LBM_CC_HORZ_EDGE");
#if defined(_DEBUG)
    std::cout << "LBM_CC_THRESHOLD is: " << envar << '\n';
#endif
    if (envar.size() != 0) {
        engine.enableControlHorzEdgeCrossing = true;
    }

    envar = GetEnv(L"LBM_CC_VERT_EDGE");
#if defined(_DEBUG)
    std::cout << "LBM_CC_VERT_EDGE is: " << envar << '\n';
#endif
    if (envar.size() != 0) {
        engine.enableControlVertEdgeCrossing = true;
    }

    envar = GetEnv(L"LBM_CC_CTRL_KEY");
#if defined(_DEBUG)
    std::cout << "LBM_CC_CTRL_KEY is: " << envar << '\n';
#endif
    if (envar.size() != 0) {
        engine.enableCtrlKeyCrossing = true;
    }

    auto p = GetParentProcess();

    // Test if daemon was started from UI
    bool uiMode = p.find("LittleBigMouse") != std::string::npos;
    if(!uiMode)
    {
	    for(int i=0; i<argc; i++)
	    {
			if(strcmp(argv[i], "--ignore_current") == 0)
		    {
	    		uiMode = true;
				break;
			}
		}
	}

    auto daemon = LittleBigMouseDaemon( &server , &engine, &hook );

    if(uiMode)
    {
        #if defined(_DEBUG)
	    std::cout << "Starting in UI mode\n";
        #endif
        try
        {
		    daemon.Run("");
        }
		catch(const std::exception& e)
		{
        	std::cerr << e.what() << '\n';
        }
	}
	else
	{
        #if defined(_DEBUG)
		std::cout << "Starting in Daemon mode\n";
        #endif
		daemon.Run("\\Mgth\\LittleBigMouse\\Current.xml");
	}

    if(hHandle)
    {
        ReleaseMutex (hHandle);
	    CloseHandle (hHandle);
    }
#if defined(_DEBUG)
    system("pause");
#endif
	return 0;
}
