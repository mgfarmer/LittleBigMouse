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

static int GetEnvInt(const std::wstring& varName, int def = 0)
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

static void configureControlledCross(MouseEngine* engine) {
    /*
    * LBM_CC_HORZ_EDGE <- Defined or not, value is not used
    *
    * This envar, when defined, makes all horizontal edges into
    * controlled crossings. The primary use case is when you
    * have vertically stacked monitors. For instance, I have 3
    * side-by-side monitors, with a fourth monitor above the middle
    * monitor. Without controlled crossings interacting with the
    * main menu of full screen apps (near the top edge), or interacting 
    * with the taskbar on the upper monitor (near the bottom edge) is 
    * frustrating because the cursor too easily jumps to the monitor 
    * above or below the interaction zone. This is especially true
    * when using full-screen RDP which requires holding the cursor at 
    * the very top of the screen.
    */
    engine->enableControlHorzEdgeCrossing = false;
    std::string envar = GetEnv(L"LBM_CC_HORZ_EDGE");
#if defined(_DEBUG)
    std::cout << "LBM_CC_THRESHOLD is: " << envar << '\n';
#endif
    if (envar.size() != 0) {
        engine->enableControlHorzEdgeCrossing = true;
    }

    /*
    * LBM_CC_VERT_EDGE <- Defined or not, value is not used
    * 
    * This envar, when defined, makes all vertical edges into
    * controlled crossings.  It is implemented for completeness,
    * though I have not come up with a good use case for it.
    */
    engine->enableControlVertEdgeCrossing = false;
    envar = GetEnv(L"LBM_CC_VERT_EDGE");
#if defined(_DEBUG)
    std::cout << "LBM_CC_VERT_EDGE is: " << envar << '\n';
#endif
    if (envar.size() != 0) {
        engine->enableControlVertEdgeCrossing = true;
    }

    /*
    * LBM_CC_CTRL_KEY <- Defined or not, value is not used
    * 
    * The envar, when defined, allows crossing a controlled edge
    * immediately if the Ctrl key is pressed while moving the mouse.
    * This can be used in concert with the LBM_CC_THRESHOLD envar.
    */
    engine->enableCtrlKeyCrossing = false;
    envar = GetEnv(L"LBM_CC_CTRL_KEY");
#if defined(_DEBUG)
    std::cout << "LBM_CC_CTRL_KEY is: " << envar << '\n';
#endif
    if (envar.size() != 0) {
        engine->enableCtrlKeyCrossing = true;
    }

    /*
    * LBM_CC_THRESHOLD <- Integer > 0
    *
    * This threshold controls how much effort is required to push the mouse
    * cursor across a controlled edge.  Try values around 200 to 250.  The
    * value that works best for you depends a lot on how you habitually
    * move your mouse so some tuning may be required.
    *
    * The cursor will stall at the edge until you push long enough, then
    * it will cross.
    * 
    * When undefined, or the value is less than 1, no amount of pushing
    * will allow the cursor across.
    */
    engine->controlCrossingThreshold = 0;
    int thresh = GetEnvInt(L"LBM_CC_THRESHOLD");
#if defined(_DEBUG)
    std::cout << "LBM_CC_THRESHOLD is: " << thresh << '\n';
#endif
    if (thresh > 0) {
        engine->controlCrossingThreshold = thresh;
    }

    /*
    * Make sure the user does not configure in a way that prevent all edge
    * crossings. If so, then disable edge crossing checks completely.  One
    * (or both) of LBM_CC_THRESHOLD and LBM_CC_CTRL_KEY must be defined
    * otherwise no controlled edges would ever allow crossings.
    */
    if (engine->enableControlHorzEdgeCrossing || engine->enableControlVertEdgeCrossing) {
        if (!engine->enableCtrlKeyCrossing && engine->controlCrossingThreshold < 1) {
            engine->enableControlHorzEdgeCrossing = false;
            engine->enableControlVertEdgeCrossing = false;
        }
    }
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

    configureControlledCross(&engine);

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
