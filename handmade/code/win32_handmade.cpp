
/*
 TODO: This is NOT a FINAL Platform layer
 
 -Saved game locations
 -Getting a handle to our own executable file
 -Asset loading path
 -Threading
 -Raw Input(Support multiple keyboards)
 -ClipCursor() (for multimonitor support)
 -QueryCancelAutoPlay
 -WM_ACTIVATEAPP (for when we are not the active application)
 -Blit speed improvements (BitBLit)
 -Hardware acceleration (OpenGL  or Direct3D or both)
 -GetKeyboardLayout (for French keyboards, international WASD support)
 -ChangeDisplaySettings option if we detect slow fullscreen blit
 
 just a partial list of stuff
 
*/

#include "handmade_platform.h"


#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"


struct platform_work_queue_entry
{
    platform_work_queue_callback * CallBack;
    void * Data;
};

struct platform_work_queue
{
    HANDLE SemaphoreHandle;
    //NOTE: This is a general counter
    uint32 volatile CompletionGoal;
    uint32 volatile CompletionCount;
    
    uint32 volatile NextEntryToWrite;
    uint32 volatile NextEntryToRead;
    
    platform_work_queue_entry Entries[256];
};


//TODO: this is a global for now
//global_variable initializes to 0 by default
global_variable bool32 GlobalPause = false;
global_variable bool32 GlobalRunning = true ;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondarySoundBuffer;
global_variable int64 TotalTimeCountFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable platform_work_queue HighPriorityQueue = {};
global_variable platform_work_queue LowPriorityQueue = {};


#if HANDMADE_INTERNAL
global_variable debug_table GlobalDebugTable_ = {};
debug_table * GlobalDebugTable = &GlobalDebugTable_;
#endif


global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };


inline void Win32CompleteAllWork(platform_work_queue * Queue);



//NOTE: XInputGetState support
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *DynamicXInputGetState = XInputGetStateStub;
#define XInputGetState DynamicXInputGetState

//NOTE: XInputSetState support
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *DynamicXInputSetState = XInputSetStateStub;
#define XInputSetState DynamicXInputSetState

//NOTE: DirectSound support
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

//------------------------------------------------------------------------------------------------------
//CONCAT STRINGS & GET EXE FILE NAME


internal void Win32GetEXEFileName(win32_state * Win32State)
{
    //Gets the executable's filename location
    
    DWORD SizeOfFileName = GetModuleFileName(0,Win32State->EXEFilename, sizeof(Win32State->EXEFilename));
    
    Win32State->LastSlashEXEFileName = Win32State->EXEFilename;
    for (char * Scan = Win32State->EXEFilename; *Scan; ++Scan)
    {
        if (*Scan == '\\')
        {
            Win32State->LastSlashEXEFileName = Scan + 1;
        }
    }
}


internal void Win32BuildEXEPathFileName(win32_state *  Win32State, char * FileName, int DestSize, char * Dest)
{
    ConcatStringsA(Win32State->LastSlashEXEFileName - Win32State->EXEFilename, Win32State->EXEFilename,
                   StringLength(FileName), FileName, DestSize, Dest);
}

//------------------------------------------------------------------------------------------------------
//IN/OUT FILE-SYSTEM

#if HANDMADE_INTERNAL
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0,MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_file_content DebugFileContent ={};
    
    HANDLE FileHandle = CreateFileA(Filename,GENERIC_READ ,FILE_SHARE_READ, 0, OPEN_EXISTING, 0,0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        //Handle exists
        LARGE_INTEGER FileSize;
        
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            //TODO: Defines for maximum values UInt32Max
            uint32 FileSize32 = SafeTruncateToUint32(FileSize.QuadPart);
            
            DebugFileContent.Content = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if(DebugFileContent.Content)
            {
                DWORD BytesRead;
                if(ReadFile(FileHandle,DebugFileContent.Content,FileSize32, &BytesRead, 0) && 
                   (FileSize32 == BytesRead))
                {
                    DebugFileContent.ContentSize = FileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(DebugFileContent.Content);
                    DebugFileContent.Content = 0;
                }
            }
            else
            {
                //TODO:Logging
            }
        }
        else
        {
            //TODO:Logging
        }
        CloseHandle(FileHandle);
    }
    else
    {
        //TODO:Logging
    }
    return DebugFileContent;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;
    HANDLE FileHandle = CreateFileA(Filename,GENERIC_WRITE ,0, 0, CREATE_ALWAYS, 0,0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if(WriteFile(FileHandle,Memory,MemorySize32, &BytesWritten, 0) && (MemorySize32 == BytesWritten))
        {
            Result = true;
        }
        else
        {
            Result = false;
        }
        CloseHandle(FileHandle);
    }
    else
    {
        //TODO:Logging
    }
    return Result;
}


DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(DEBUGExecuteSystemCommmand)
{
    debug_executing_process ExecutingProcess = {};
    debug_process_state ProcessState = {};
    
    STARTUPINFO StartupInfo = {};
    PROCESS_INFORMATION ProcessInfo = {};
    
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE; 
    
    if(CreateProcessA(Command, 
                      CommandLine, 
                      0, 
                      0, 
                      false,
                      0,
                      0,
                      Path,
                      &StartupInfo,
                      &ProcessInfo))
    {
        *(HANDLE*)&ExecutingProcess.OSHandle = ProcessInfo.hProcess;
    }
    else
    {
        DWORD LastError = GetLastError();
        *(HANDLE*)&ExecutingProcess.OSHandle = INVALID_HANDLE_VALUE;
    }
    
    return ExecutingProcess;
}


DEBUG_PLATFORM_GET_PROCESS_STATE(DEBUGGetProcessState)
{
    debug_process_state Result = {};
    HANDLE ProcessHandle = *(HANDLE*)&ExecutingProcess.OSHandle;
    
    if(ProcessHandle != INVALID_HANDLE_VALUE)
    {
        Result.ProcessStarted = true;
        if(WaitForSingleObject(ProcessHandle, 0) == WAIT_OBJECT_0)
        {
            DWORD ExitCode = 0;
            if(GetExitCodeProcess(ProcessHandle, &ExitCode))
            {
                Result.ExitCode = ExitCode;
            }
            
            CloseHandle(ProcessHandle);
        }
        else
        {
            Result.IsRunning = true;
        }
        
    }
    
    return Result;
}
#endif

struct win32_platform_file_handle
{
    HANDLE Win32Handle;
};

struct win32_platform_file_group
{
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
};

internal 
BEGIN_PLATFORM_GIVE_ME_ALL_FILES_OF_EXTENSION(Win32BeginGiveMeAllFilesOfExtension)
{
    platform_file_group Result = {};
    win32_platform_file_group *  Platform = (win32_platform_file_group*)VirtualAlloc(0, sizeof(win32_platform_file_group), 
                                                                                     MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    
    Result.Platform = Platform;
    
    wchar_t * WildCard = L"*.*";
    
    switch (FileType)
    {
        case PlatformFileType_AssetFile:
        {
            WildCard = L"*.hha";
        }break;
        
        case PlatformFileType_SaveFile:
        {
            WildCard = L"*.hhs";
        }break;
        default:
        {
            InvalidCodePath;
        }break;
    }
    
    Result.FileCount = 0;
    
    WIN32_FIND_DATAW FindData = Platform->FindData;
    HANDLE FindHandle = FindFirstFileW(WildCard, &FindData);
    while(FindHandle != INVALID_HANDLE_VALUE)
    {
        //TODO: File processing
        ++Result.FileCount; 
        
        if(!FindNextFileW(FindHandle, &FindData))
        {
            break;
        }
    }
    
    FindClose(FindHandle);
    
    Platform->FindHandle = FindFirstFileW(WildCard, &Platform->FindData);
    return Result;
}

internal 
END_PLATFORM_GIVE_ME_ALL_FILES_OF_EXTENSION(Win32EndGiveMeAllFilesOfExtension)
{
    win32_platform_file_group * Win32FileGroup = (win32_platform_file_group*)FileGroup->Platform;
    Assert(Win32FileGroup);
    
    FindClose(Win32FileGroup->FindHandle);
    VirtualFree(Win32FileGroup, 0, MEM_RELEASE);
}

internal  
PLATFORM_OPEN_NEXT_FILE(Win32OpenNextFile)
{
    win32_platform_file_group * Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
    win32_platform_file_handle *  Platform = 0;
    
    platform_file_handle Result = {};
    
    Assert(Win32FileGroup);
    
    if(Win32FileGroup->FindHandle != INVALID_HANDLE_VALUE)
    {
        wchar_t * FileName = Win32FileGroup->FindData.cFileName;
        
        //TODO: If we want someday implement a memory arena for win32 platform 
        Platform = (win32_platform_file_handle*)VirtualAlloc(0, sizeof(win32_platform_file_handle), 
                                                             MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        Result.Platform = Platform;
        
        if(Platform)
        {
            Platform->Win32Handle = CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0,0);
            Result.NoErrors = (Platform->Win32Handle != INVALID_HANDLE_VALUE);
        }
        
        if(!FindNextFileW(Win32FileGroup->FindHandle, &Win32FileGroup->FindData))
        {
            FindClose(Win32FileGroup->FindHandle);
            Win32FileGroup->FindHandle = INVALID_HANDLE_VALUE;
        }
    }
    
    return (Result);
}

internal 
PLATFORM_FILE_ERROR(Win32FileError)
{
#if HANDMADE_INTERNAL
    OutputDebugStringA("WIN32 FILE ERROR: ");
    OutputDebugStringA(Message);
    OutputDebugStringA("\n");
#endif
    Handle->NoErrors = false;
}

internal
PLATFORM_READ_DATA_FROM_FILE(Win32ReadDataFromFile)
{
    win32_platform_file_handle * FileHandle = (win32_platform_file_handle*)Source->Platform;
    if(FileHandle)
    {
        if(PlatformNoFileErrors(Source))
        {
            OVERLAPPED Overlapped = {};
            Overlapped.Offset = ((Offset >> 0) & 0xFFFFFFFF);
            Overlapped.OffsetHigh = ((Offset >> 32) & 0xFFFFFFFF);
            
            uint32 FileSize32 = SafeTruncateToUint32(Size);
            
            DWORD BytesRead = 0;
            if((ReadFile(FileHandle->Win32Handle, Dest, FileSize32, &BytesRead , &Overlapped)
                && (FileSize32 == BytesRead)))
            {
                //NOTE: File Read succeded
            }
            else
            {
                Win32FileError(Source, "Error opening file.\n");
            }
        }
    }
}
/*

internal void 
Win32FileError(platform_file_handle *  Handle, char * ErrorMessage)
{
CloseHandle(FileHandle);
}
*/


//------------------------------------------------------------------------------------------------------
//LOAD & INITIALIZATION XINPUT-DLL


inline FILETIME Win32GetLastWriteTime(char * Filename)
{
    FILETIME LastWriteTime = {};
    
    WIN32_FILE_ATTRIBUTE_DATA FileAttributeData = {};
    if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &FileAttributeData))
    {
        LastWriteTime = FileAttributeData.ftLastWriteTime;
    }
    
    return LastWriteTime;
}


internal win32_game_code 
Win32LoadGameCode(char * SourceDLLName, char * TempDLLName, char * LockFileName)
{
    win32_game_code Result = {};
    
    WIN32_FILE_ATTRIBUTE_DATA Ignored = {};
    //NOTE: Since VS locks DLL while executing. We copy the DLL file code instructions to a new file so we
    //can catch memoy adresses' function calls  
    if(!GetFileAttributesEx(LockFileName, GetFileExInfoStandard, &Ignored))
    {
        Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
        
        CopyFile(SourceDLLName, TempDLLName , FALSE);
        
        Result.GameCodeDLL = LoadLibraryA(TempDLLName);
        if(Result.GameCodeDLL)
        {
            Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(Result.GameCodeDLL,"GameUpdateAndRender");
            Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL,"GameGetSoundSamples");
            
#if HANDMADE_INTERNAL            
            Result.DEBUGFrameEnd = (game_debug_frame_end *)GetProcAddress(Result.GameCodeDLL,"GameDEBUGFrameEnd");
#endif
            
            Result.IsValid = (Result.GetSoundSamples && Result.UpdateAndRender);
        }
    }
    
    if(!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }
    
    return(Result);
}

internal void Win32UnloadGameCode(win32_game_code * GameCode)
{
    if(GameCode->GameCodeDLL)
    {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }
    
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
    
#if HANDMADE_INTERNAL    
    GameCode->DEBUGFrameEnd = 0;
#endif
    
}

internal void Win32LoadXInput(void)
{
    //NOTE: Test this on Windows 8
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if(!XInputLibrary)
    {
        //TODO: Diagnostic
        XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }
    if(!XInputLibrary)
    {
        //TODO: Diagnostic
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    
    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary,"XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary,"XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}
        //TODO: Diagnostic
    }
    else
    {
        //TODO: Diagnostic
    }
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    //NOTE: Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    
    if(DSoundLibrary)
    {
        //NOTE: Get a DirectSound object
        direct_sound_create * DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary,"DirectSoundCreate");
        
        //TODO: Double-check that this works on XP - DirectSound8 or 7?
        LPDIRECTSOUND DirectSound;
        //NOTE: SUCCEEDED Macro is true if What returns from DirectSoundCreate is >= 0
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0,&DirectSound, 0)))
        {
            WAVEFORMATEX SoundFormat = {};
            SoundFormat.wFormatTag =WAVE_FORMAT_PCM;
            SoundFormat.nChannels = 2;
            SoundFormat.nSamplesPerSec = SamplesPerSecond;
            
            SoundFormat.wBitsPerSample = 16;
            SoundFormat.nBlockAlign =SoundFormat.nChannels * SoundFormat.wBitsPerSample / 8;
            SoundFormat.nAvgBytesPerSec = SoundFormat.nSamplesPerSec * SoundFormat.nBlockAlign;
            SoundFormat.cbSize = 0;
            
            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                //NOTE: Create a primary buffer
                DSBUFFERDESC  SoundBufferDescription = {};
                SoundBufferDescription.dwSize = sizeof(DSBUFFERDESC);
                SoundBufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                //DSBCAPS_GLOBALFOCUS;
                
                LPDIRECTSOUNDBUFFER PrimarySoundBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&SoundBufferDescription,&PrimarySoundBuffer,0)))
                {
                    if(SUCCEEDED(PrimarySoundBuffer->SetFormat(&SoundFormat)))
                    {
                        OutputDebugStringA("Primary Buffer format was set\n");
                    }
                    else
                    {
                        //TODO: Diagnostic
                    }
                }
                else
                {
                    //TODO: Diagnostic
                }
            }
            else
            {
                //TODO: Diagnostic
            }
            
            //NOTE:Create a secondary buffer :
            DSBUFFERDESC  SoundBufferDescription = {};
            SoundBufferDescription.dwSize = sizeof(DSBUFFERDESC);
            SoundBufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS;
            SoundBufferDescription.dwBufferBytes = BufferSize;
            SoundBufferDescription.lpwfxFormat = &SoundFormat;
            
            if(SUCCEEDED(DirectSound->CreateSoundBuffer(&SoundBufferDescription,&GlobalSecondarySoundBuffer,0)))
            {
                //NOTE: Start it playing
                OutputDebugStringA("Secondary Buffer created successfully\n");
            }
            else
            {
                //TODO: Diagnostic
            }
        }
        else
        {
            //TODO: Diagnostic
        } 
    }
    else
    {
        //TODO:Diagnostic
    }
}

//------------------------------------------------------------------------------------------------------
//WINDOWPROCEDURES

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Dimension;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Dimension.Width = ClientRect.right;
    Dimension.Height = ClientRect.bottom;
    
    return Dimension;
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;
    
    Buffer->Width = Align16(Width);
    Buffer->Pitch = Align16(Buffer->Width*BytesPerPixel);
    Buffer->Height = Height;
    
    int BitmapMemorySize = (Buffer->Pitch * Buffer->Height);
    
    //NOTE: When the biHeight is NEGATIVE, this is the clue to windows to treat this bitmap as top-down
    //meaning that the first three bytes of the image are the UPPER-LEFT corner color pixel of the screen.
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;//Make pixels align on 4 byte boundaries
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    //MEM_COMMIT:Reserves page memory and makes system to keep track of memory for being used by the user.
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    
    
    //TODO: Probably clear this to black
}

internal void Win32UpdateWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight){
    if((WindowWidth >= Buffer->Width * 1.2) && (WindowHeight >= Buffer->Height * 1.2))
    {
        
        StretchDIBits(DeviceContext,
                      0, 0, WindowWidth, WindowHeight,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory,
                      &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
        
    }
    else
    {
#if 0
        int OffsetX = 10;
        int OffsetY = 10;
        
        PatBlt(DeviceContext, 0,0, WindowWidth, OffsetY, BLACKNESS);
        PatBlt(DeviceContext, 0,0, OffsetX, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, 0,OffsetY + Buffer->Height, WindowWidth, WindowHeight - (OffsetY + Buffer->Height), BLACKNESS);
        PatBlt(DeviceContext, OffsetX + Buffer->Width,0, WindowWidth - (OffsetX + Buffer->Width), WindowHeight, BLACKNESS);
#else
        int OffsetX = 0;
        int OffsetY = 0;
#endif
        
        //NOTE:For prototyping purposes, we're always going to blit 1-to-1 pixels to make sure we don't introduce artifacts with stretching while we are leraning to code the renderer
        StretchDIBits(DeviceContext,
                      OffsetX, OffsetY, Buffer->Width, Buffer->Height,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory,
                      &Buffer->Info,
                      DIB_RGB_COLORS, SRCCOPY);
        
    }
}

//------------------------------------------------------------------------------------------------------
//MAIN-WINDOWS-CALLBACK-MESSAGES

internal LRESULT CALLBACK Win32MainWindowCallBack(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch (Message)
    {
        case WM_SIZE:
        {
            
        }break;
        
        case WM_CLOSE:
        {
            //TODO:Handle this with a message to the user
            GlobalRunning = false;
        }break;
        
        case WM_SETCURSOR:
        {
            if(DEBUGGlobalShowCursor)
            {
                Result = DefWindowProcA(Window,Message,WParam,LParam);
            }
            else
            {
                SetCursor(0);	
            }
            
        }break;
        
        case WM_ACTIVATEAPP:
        {
#if 0
            if(WParam == TRUE)
            {
                SetLayeredWindowAttributes(Window, RGB(0,0,0),255,LWA_ALPHA);
            }
            else
            {
                SetLayeredWindowAttributes(Window, RGB(0,0,0),128,LWA_ALPHA);	
            }
#endif
        }break;
        
        case WM_DESTROY:
        {
            //TODO: Handle this with as an error - recreate window
            GlobalRunning = false;
        }break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came in through a non-dispatch message");
        }break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32UpdateWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window,&Paint);
        }break;
        
        default:
        {
            //OutputDebugStringA("default\n");
            Result = DefWindowProcA(Window,Message,WParam,LParam);
        }break;
    }
    
    return(Result);
}

//------------------------------------------------------------------------------------------------------
//WIN32 SOUNDBUFFER PROCESSING

internal void Win32ClearSoundBuffer(win32_sound_output * SoundOutput)
{
    void * Region1;
    DWORD Region1Size;
    void * Region2;
    DWORD Region2Size;
    if(DS_OK == GlobalSecondarySoundBuffer->Lock(
        0, SoundOutput->SoundBufferSize, &
        Region1,&Region1Size,
        &Region2,&Region2Size,0))
    {
        uint8 * DestSample = (uint8 *) Region1;
        for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        DestSample = (uint8 *) Region2;
        for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        
        if(DS_OK == GlobalSecondarySoundBuffer->Unlock(Region1,Region1Size, Region2, Region2Size))
        {
            //OutputDebugStringA("Correct Sound Unlock");
        }
        
    }
}



internal void Win32FillSoundBuffer(win32_sound_output * SoundOutput, game_sound_output * GameSourceBuffer, 
                                   DWORD ByteToLock, DWORD  BytesToWrite)
{
    //TODO:More strenuous test
    void * Region1;
    DWORD Region1Size;
    void * Region2;
    DWORD Region2Size;
    if(DS_OK == GlobalSecondarySoundBuffer->Lock(
        ByteToLock, BytesToWrite, &
        Region1,&Region1Size,
        &Region2,&Region2Size,0))
    {
        //Write to the Regions
        int16 * DestSample = (int16 *) Region1;
        int16 * SourceSample = (int16 * )GameSourceBuffer->Samples;
        
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        //TODO:COllapse these for loops
        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        
        if(Region2)
        {
            DestSample = (int16 *) Region2;
            for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
            {
                *DestSample++ = *SourceSample++;
                *DestSample++ = *SourceSample++;
                ++SoundOutput->RunningSampleIndex;
            }
        }
        
        if(DS_OK == GlobalSecondarySoundBuffer->Unlock(Region1,Region1Size, Region2, Region2Size))
        {
            //OutputDebugStringA("Correct Sound Unlock");
        }
        
    }
}

//------------------------------------------------------------------------------------------------------
//CONTROL INPUT PROCESSING

internal void Win32ProcessKeyboardMessage(game_button_state * NewState, bool32 IsDown)
{
    if(NewState->ButtonEndedDown != IsDown)
    {
        ++NewState->HalfTransitionCount;
        NewState->ButtonEndedDown =  IsDown;
    }
    
}

internal void Win32ProcessXInputDigitalButton(WORD XInputButtonState, 
                                              game_button_state * NewState, 
                                              game_button_state * OldState, 
                                              WORD ButtonBit)
{
    //NOTE: Is HalfTransitionCount should be treated the same as keyboard state
    NewState->HalfTransitionCount += (OldState->ButtonEndedDown != NewState->ButtonEndedDown) ? 1:0;
    NewState->ButtonEndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
}

internal real32 Win32ProcessXInputStickValue(SHORT StickDirection, SHORT DeadZoneThreshold)
{
    real32 Value = 0;
    if(StickDirection < -DeadZoneThreshold)
    {
        Value = (real32)(StickDirection + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
    }
    else if(StickDirection > DeadZoneThreshold)
    {
        Value = (real32)(StickDirection - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold);
    }
    return Value;
}

internal void Win32GetInputFileLocation(win32_state * Win32State, bool32 InputStream, int SlotIndex, 
                                        int DestSize, char * Dest)
{
    //Assert(SlotLocation == 1);
    char Temp[64];
    wsprintf(Temp, "live_loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input":"state");
    Win32BuildEXEPathFileName(Win32State, Temp, DestSize, Dest);
}

internal win32_replay_buffer * 
Win32GetReplayBuffer(win32_state * Win32State, int unsigned Index)
{
    Assert(Index > 0);
    Assert(Index < ArrayCount(Win32State->ReplayBuffers));
    win32_replay_buffer * ReplayBuffer = &Win32State->ReplayBuffers[Index];
    return ReplayBuffer;
}

internal void
Win32BeginRecordingInput(win32_state * Win32State,
                         int InputRecordingIndex)
{
    
    win32_replay_buffer * ReplayBuffer = Win32GetReplayBuffer(Win32State, InputRecordingIndex);
    
    if(ReplayBuffer->MemoryBlock)
    {
        Win32State->InputRecordingIndex = InputRecordingIndex;
        Win32State->RecordingHandle = ReplayBuffer->FileHandle;
        
        char FileName[WIN32_STATE_FILE_NAME_SIZE];
        Win32GetInputFileLocation(Win32State, true, InputRecordingIndex , sizeof(FileName), FileName);		
        
        Win32State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE,0, 0, CREATE_ALWAYS, 0,0);
        
#if 0		
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = Win32State->TotalGameMemorySize;
        
        SetFilePointerEx(Win32State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif
        
        CopyMemory(ReplayBuffer->MemoryBlock, Win32State->GameMemoryBlock, 
                   (size_t)Win32State->TotalGameMemorySize);
        
    }
}

internal void
Win32EndRecordingInput(win32_state * Win32State)
{
    CloseHandle(Win32State->RecordingHandle);
    Win32State->InputRecordingIndex = 0;
}

internal void
Win32BeginPlayBackInput(win32_state * Win32State,
                        int InputPlayingIndex)
{
    win32_replay_buffer * ReplayBuffer = Win32GetReplayBuffer(Win32State, InputPlayingIndex);
    
    if(ReplayBuffer->MemoryBlock)
    {
        Win32State->InputPlayingIndex = InputPlayingIndex;
        Win32State->PlayBackHandle = ReplayBuffer->FileHandle;
        
        char FileName[WIN32_STATE_FILE_NAME_SIZE];
        Win32GetInputFileLocation(Win32State, true, InputPlayingIndex , sizeof(FileName), FileName);		
        
        Win32State->PlayBackHandle = CreateFileA(FileName, GENERIC_READ,0, 0, OPEN_EXISTING, 0,0);
        
#if 0		
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = Win32State->TotalGameMemorySize;
        
        SetFilePointerEx(Win32State->PlayBackHandle, FilePosition, 0, FILE_BEGIN);
#endif	
        CopyMemory(Win32State->GameMemoryBlock, ReplayBuffer->MemoryBlock , (size_t)Win32State->TotalGameMemorySize);
    }
}

internal void
Win32EndPlayBackInput(win32_state * Win32State)
{
    CloseHandle(Win32State->PlayBackHandle);
    Win32State->InputPlayingIndex = 0;
}

internal void
Win32RecordingInput(win32_state * Win32State,
                    game_input * NewInput)
{
    DWORD BytesWritten;
    WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_state * Win32State, game_input * NewInput, game_memory * Memory)
{
    DWORD BytesRead;
    if(ReadFile(Win32State->PlayBackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
    {
        //NOTE: there's still input 
        if(BytesRead == 0)
        {
            //NOTE: We've hit the end of the stream, replay at the beginning
            Memory->LoopMemoryReset = true;
            int PlayBackIndex = Win32State->InputPlayingIndex;
            Win32EndPlayBackInput(Win32State);
            Win32BeginPlayBackInput(Win32State, PlayBackIndex);
            ReadFile(Win32State->PlayBackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
        
    }
}

//TOGGLE FULL-SCREEN
//---------------------------------------------------------------------------------------------
internal void 
ToggleFullScreen(HWND Window)
{
    //NOTE:This follows Raymond Chen's prescription for fullscreen toggling, see:
    //https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353 
    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW) 
    {
        MONITORINFO MonitorInfo = { sizeof ( MonitorInfo ) };
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window,
                                             MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) 
        {
            SetWindowLong(Window, GWL_STYLE,
                          Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } 
    else 
    {
        SetWindowLong(Window, GWL_STYLE,
                      Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}


//------------------------------------------------------------------------------------------------------
//KEYBOARD MESSAGES
internal void Win32ProcessPendingMessages(win32_state * Win32State, game_controller_input * KeyboardController)
{
    //TODO(Alex): Handle mouse input here also!
    MSG Message;
    while(PeekMessage(&Message,0,0,0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            }break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKeyCode = (uint32)Message.wParam;
                //the previous key state .1 if the button was pressed down before the message was sent,0 otherwise
                
                bool32 WasDownBefore = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                
                if(WasDownBefore != IsDown)
                {
                    switch(VKeyCode)
                    {
                        case 'W':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                        }break;
                        
                        case 'A':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                        }break;
                        
                        case 'S':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                        }break;
                        
                        case 'D':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                        }break;
                        
                        case 'Q':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                        }break;
                        
                        case 'E':
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                        }break;
                        
                        case VK_UP:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                        }break;
                        
                        case VK_LEFT:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                        }break;
                        
                        case VK_DOWN:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                        }break;
                        
                        case VK_RIGHT:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                        }break;
                        
                        case VK_ESCAPE:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                        }break;
                        
                        case VK_SPACE:
                        {
                            Win32ProcessKeyboardMessage(&KeyboardController->Start,
                                                        IsDown);
                        }break;
                        
#if HANDMADE_INTERNAL
                        case 'P':
                        {
                            if(IsDown)
                            {
                                GlobalPause = !GlobalPause;
                            }
                            
                        }break;
                        
                        case 'L':
                        {
                            if(IsDown)
                            {
                                if(Win32State->InputPlayingIndex == 0)
                                {
                                    if(Win32State->InputRecordingIndex == 0)
                                    {
                                        Win32BeginRecordingInput(Win32State, 1);
                                    }
                                    else
                                    {
                                        Win32EndRecordingInput(Win32State);
                                        Win32BeginPlayBackInput(Win32State, 1);
                                    }	
                                }
                                else
                                {
                                    Win32EndPlayBackInput(Win32State);
                                }
                            }
                            
                        }
                        break;
                        
                        /*case 'K':
                        {
                        Win32State->InputPlayingIndex= 0;
                        }
                        break;
                        */
#endif
                        default:
                        break;
                    }
                    
                    if(IsDown)
                    {
                        bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                        if((VKeyCode == VK_F4)&& AltKeyWasDown)
                        {
                            GlobalRunning = false;
                        }
                        
                        if((VKeyCode == VK_RETURN) && AltKeyWasDown)
                        {
                            if(Message.hwnd)
                            {
                                ToggleFullScreen(Message.hwnd);	
                            }
                            
                        }	
                    }
                }
                
            }break;
            
            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            }break;
        }
        
    }
}



inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER WallClock;
    QueryPerformanceCounter(&WallClock);
    return WallClock;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 SecondsElapsed = ((real32)(End.QuadPart - Start.QuadPart) / (real32)TotalTimeCountFrequency);
    return SecondsElapsed;
}


//------------------------------------------------------------------------------------------------------
//DEBUGSYNC

internal void 
Win32DebugDrawLine(win32_offscreen_buffer * BackBuffer, int X, int  Top, int Bottom, uint32 Color)
{
    if(Top <= 0)
    {
        Top = 0;
    }
    if(Bottom > BackBuffer->Height)
    {
        Bottom = BackBuffer->Height;
    }
    if((X >= 0) && (X < BackBuffer->Width))
    {
        uint8 * Pixel = ((uint8 * )BackBuffer->Memory + (X* BackBuffer->BytesPerPixel) + (Top*BackBuffer->Pitch));
        for(int Y = Top; Y < Bottom; ++Y)
        {
            *(uint32*)Pixel = Color;
            Pixel += BackBuffer->Pitch;
        }
    }
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer, win32_sound_output * SoundOutput, real32 PlayCursorRatio, int PadX,  int Top, int Bottom, DWORD Cursor, uint32 Color)
{
    real32 XReal32 = PlayCursorRatio*(real32)Cursor;
    int X = PadX + (int)(XReal32);
    
    Win32DebugDrawLine(BackBuffer, X,  Top, Bottom, Color);
}

#if 0
internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer,
                      int TimeMarkerSize ,win32_debug_time_marker * TimeMarker,
                      int  CurrentMarkerIndex , win32_sound_output *SoundOutput,  real32 TargetSecondsPerFrame)
{
    //TODO: Draw where we 're writing out sound
    int PadX = 16;
    int PadY = 16;
    
    int LineHeight = 64;
    real32 PlayCursorRatio =  ((real32)BackBuffer->Width -2.0f*(real32)PadX)/(real32)SoundOutput->SoundBufferSize;
    for(int TimeMarkerIndex= 0;
        TimeMarkerIndex < TimeMarkerSize;
        ++TimeMarkerIndex)
    {
        win32_debug_time_marker * ThisMarker = &TimeMarker[TimeMarkerIndex];
        Assert((int)ThisMarker->OutputPlayCursor < SoundOutput->SoundBufferSize);
        Assert((int)ThisMarker->OutputWriteCursor < SoundOutput->SoundBufferSize);
        Assert((int)ThisMarker->OutputByteToLock < SoundOutput->SoundBufferSize);
        Assert((int)ThisMarker->OutputBytesToWrite < SoundOutput->SoundBufferSize);
        Assert((int)ThisMarker->FlipPlayCursor < SoundOutput->SoundBufferSize);
        Assert((int)ThisMarker->FlipWriteCursor < SoundOutput->SoundBufferSize);
        
        
        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xF00F0FFF;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;
        
        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if(TimeMarkerIndex == CurrentMarkerIndex)
        {
            Top += (PadY + LineHeight);
            Bottom += (PadY + LineHeight);
            
            int FirstTop = Top;
            
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, Top, Bottom,ThisMarker->OutputPlayCursor, PlayColor);
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, Top, Bottom,ThisMarker->OutputWriteCursor, WriteColor);
            
            Top += PadY + LineHeight;
            Bottom += PadY + LineHeight;
            
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, Top, Bottom,ThisMarker->OutputByteToLock
                                       , PlayColor);
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, Top, Bottom,ThisMarker->OutputByteToLock + ThisMarker->OutputBytesToWrite, WriteColor);
            
            Top += PadY + LineHeight;
            Bottom += PadY + LineHeight;
            
            Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, FirstTop, Bottom,ThisMarker->ExpectedFlipPlayCursor , ExpectedFlipColor);
        }
        
        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + (480*SoundOutput->BytesPerSample), PlayWindowColor);
        Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, PlayCursorRatio, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
    }
}

#endif

//NOTE: We say to the compiler that the values for this variables could be changed 
//from other part of the system(Threads) 

//NOTE: WriteBarrier and ReadBarrier are markups for the compiler to prevent reorder memory access operations
//_mm_sfence is processor instruction that prevents intrunsction to be reorderedd

inline void 
Win32AddEntry(platform_work_queue * Queue, platform_work_queue_callback * CallBack, void * Data)
{
    //TODO: Shall we let any thread to push work to the queue?
    uint32 NewNextEntryToWrite = ((Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries)); 
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
    
    platform_work_queue_entry * Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Data = Data;
    Entry->CallBack = CallBack;
    //NOTE: We use memory write barriers, to prevent the compiler to reorder the writes 
    _WriteBarrier();
    ++Queue->CompletionGoal;
    Queue->NextEntryToWrite = NewNextEntryToWrite;
    
    //NOTE: Some way to wake up our threads
    ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}
inline bool32 
Win32DoNextWorkQueueEntry(platform_work_queue * Queue)
{
    bool32 WeShouldSleep = false;
    uint32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    uint32 NewNextEntryToRead = ((OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries)); 
    if(OriginalNextEntryToRead != Queue->NextEntryToWrite)
    {
        //NOTE: We assure here that every thread will get a unique value for NextEntryIndex
        uint32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->NextEntryToRead, 
                                                  NewNextEntryToRead, 
                                                  OriginalNextEntryToRead);
        if(Index == OriginalNextEntryToRead)
        {
            platform_work_queue_entry * Entry = Queue->Entries + Index;
            Entry->CallBack(Queue, Entry->Data);
            InterlockedIncrement((LONG volatile *)&Queue->CompletionCount);
        }
    }
    else
    {
        WeShouldSleep = true;
    }
    return WeShouldSleep;
}

//NOTE: This is for main thread 
inline void 
Win32CompleteAllWork(platform_work_queue * Queue)
{
    while(Queue->CompletionGoal != Queue->CompletionCount)
    {
        Win32DoNextWorkQueueEntry(Queue);
    }
    Queue->CompletionCount = 0;
    Queue->CompletionGoal = 0;
}

inline PLATFORM_WORK_QUEUE_CALLBACK(DoWorkThread)
{
    char Buffer[256];
    wsprintf(Buffer, "Thread: %u, %s\n", GetCurrentThreadId(), (char * )Data);
    OutputDebugString(Buffer);
}

//NOTE: This is for threads
DWORD WINAPI 
ThreadProc(void * lpParameter)
{
    platform_work_queue * Queue = (platform_work_queue *)lpParameter;
    for(;;)
    {
        if(Win32DoNextWorkQueueEntry(Queue))
        {
            //NOTE: Some way suspend the thread that finished its job
            DWORD WaitReturn =  WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);
        }
    }
}

internal void 
Win32MakeQueue(platform_work_queue * Queue, thread_table * ThreadTable, uint32 ThreadCount)
{
    uint32 InitialCount = 0;
    
    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;
    
    Queue->SemaphoreHandle = CreateSemaphoreEx(0,InitialCount,ThreadCount,
                                               0,0,SEMAPHORE_ALL_ACCESS);
    for(uint32 ThreadIndex = 0;
        ThreadIndex < ThreadCount; 
        ++ThreadIndex)
    {
        DWORD ThreadId;
        HANDLE ThreadHandle = CreateThread(0,0, ThreadProc, Queue, 0, &ThreadId);
        
        Assert(ThreadTable->Count < MAX_NUM_THREADS);
        ThreadTable->IDs[ThreadTable->Count++] = ThreadId;
        
        CloseHandle(ThreadHandle);
    }
}


PLATFORM_ALLOCATE_MEMORY(Win32PlatformAllocateMemory)
{
    //TODO: Defines for maximum values UInt32Max
    uint32 Size32 = SafeTruncateToUint32(Size);
    void * Result = VirtualAlloc(0, Size32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    
    return Result;
}

PLATFORM_DEALLOCATE_MEMORY(Win32PlatformDeallocateMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}



//------------------------------------------------------------------------------------------------------
//WINMAIN
int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    
    //platform_work_queue AssetLoadQueue = {};
    
    //NOTE: 
    thread_table ThreadTable = {};
    
    ThreadTable.IDs[ThreadTable.Count++] = GetThreadID();
    
    Win32MakeQueue(&HighPriorityQueue, &ThreadTable,  6);
    Win32MakeQueue(&LowPriorityQueue, &ThreadTable,  2);
    
#if 0
    Win32AddEntry(&Queue, DoWorkThread, "String0");
    Win32AddEntry(&Queue, DoWorkThread, "String1");
    Win32AddEntry(&Queue, DoWorkThread, "String2");
    Win32AddEntry(&Queue, DoWorkThread, "String3");
    Win32AddEntry(&Queue, DoWorkThread, "String4");
    Win32AddEntry(&Queue, DoWorkThread, "String5");
    Win32AddEntry(&Queue, DoWorkThread, "String6");
    Win32AddEntry(&Queue, DoWorkThread, "String7");
    Win32AddEntry(&Queue, DoWorkThread, "String8");
    Win32AddEntry(&Queue, DoWorkThread, "String9");
    
    Sleep(1000);
    
    Win32AddEntry(&Queue, DoWorkThread, "Next0");
    Win32AddEntry(&Queue, DoWorkThread, "Next1");
    Win32AddEntry(&Queue, DoWorkThread, "Next2");
    Win32AddEntry(&Queue, DoWorkThread, "Next3");
    Win32AddEntry(&Queue, DoWorkThread, "Next4");
    Win32AddEntry(&Queue, DoWorkThread, "Next5");
    Win32AddEntry(&Queue, DoWorkThread, "Next6");
    Win32AddEntry(&Queue, DoWorkThread, "Next7");
    Win32AddEntry(&Queue, DoWorkThread, "Next8");
    Win32AddEntry(&Queue, DoWorkThread, "Next9");
    
    Win32CompleteAllWork(&Queue);
#endif
    
    win32_state Win32State = {};
    
    Win32GetEXEFileName(&Win32State);
    
    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_SIZE];
    Win32BuildEXEPathFileName(&Win32State, "handmade.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
    
    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_SIZE];
    Win32BuildEXEPathFileName(&Win32State, "handmade_temp.dll", sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);
    
    char GameCodeLockFullPath[WIN32_STATE_FILE_NAME_SIZE];
    Win32BuildEXEPathFileName(&Win32State, "lock.tmp", sizeof(GameCodeLockFullPath), GameCodeLockFullPath);
    
    
    //NOTE: Set the windows scheduler granularity to 1ms so that our Sleep()
    //can be more granular
    UINT DesiredSchedulerMilliseconds = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMilliseconds) == TIMERR_NOERROR);
    
    LARGE_INTEGER TimeCountFrequency;
    if(QueryPerformanceFrequency(&TimeCountFrequency))
    {
        //TODO: Handle Non high resolution performance counter
    }
    TotalTimeCountFrequency = (int64)(TimeCountFrequency.QuadPart);
    
    r64 CyclesPerFrame = 0.033333333333333 * TotalTimeCountFrequency;  
    
    
    Win32LoadXInput();
    //GetModuleHandle(0);
#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif
    WNDCLASSA WindowClass = {};
    
    //Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);
    //Win32ResizeDIBSection(&GlobalBackBuffer, 1920, 1080);
    Win32ResizeDIBSection(&GlobalBackBuffer, 1200, 700);
    
    //TODO: Check if style properties are still important
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;//CS_OWNDC|
    WindowClass.lpfnWndProc = Win32MainWindowCallBack;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    WindowClass.hCursor = LoadCursor(0,IDC_ARROW);
    
    //TODO: How do we reliably query on this on windows?
    //TODO:Let's think about running non-frame audio
    
    if(RegisterClassA(&WindowClass))
    {
        HWND Window =
            CreateWindowExA(
            0,//WS_EX_TOPMOST|WS_EX_LAYERED,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        if(Window)
        {
            //NOTE: Since we specify CS_OWNDC we can just get one device context and use it forever because we are not sharing with anyone
            
            int MonitorRefreshHz = 60;
            
            HDC RefreshDC = GetDC(Window);
            
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            
            if(Win32RefreshRate)
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            
            real32 GameUpdateHz =  (MonitorRefreshHz / 2.0f);
            real32 TargetSecondsPerFrame = 1.0f / (real32) GameUpdateHz;
            ReleaseDC(Window,RefreshDC);
            
            //NOTE: Sound Test Variables
            win32_sound_output SoundOutput = {};
            
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SoundBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            //TODO: Actually compute this variance and see What the lowest reasonable value is.
            SoundOutput.SafetyBytes = (int) (((real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample/ GameUpdateHz) / 2.0f);
            
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SoundBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            
            GlobalSecondarySoundBuffer->Play(0,0,DSBPLAY_LOOPING);
            
            GlobalRunning = true;
#if 0
            //NOTE: This test the playcursor writecursor update frequency
            while(GlobalRunning)
            {
                DWORD PlayCursor = 0;
                DWORD WriteCursor = 0;
                GlobalSecondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) ;
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),"PlayerCursor: %u   WriteCursor: %u  \n", PlayCursor, 	WriteCursor);
                OutputDebugStringA(TextBuffer);
            }
#endif
            //TODO: Treat specially at first time
            //TODO: Pool with bitmap VirtualAlloc
            u32 SoundBufferMaxSize = 2 * 4 * sizeof(s16);
            int16 * Samples= (int16*)VirtualAlloc(0, SoundOutput.SoundBufferSize + SoundBufferMaxSize, 
                                                  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            
#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(256);
            GameMemory.TransientStorageSize =  Gigabytes(1);
            GameMemory.DebugStorageSize =  Gigabytes(1);
            
            GameMemory.HighPriorityQueue = &HighPriorityQueue;
            GameMemory.LowPriorityQueue = &LowPriorityQueue;
            
            GameMemory.PlatformAPI.AddEntry = Win32AddEntry;
            GameMemory.PlatformAPI.CompleteAllWork = Win32CompleteAllWork;
            
            GameMemory.PlatformAPI.BeginGiveMeAllFilesOfExtension = Win32BeginGiveMeAllFilesOfExtension;
            GameMemory.PlatformAPI.EndGiveMeAllFilesOfExtension = Win32EndGiveMeAllFilesOfExtension;
            GameMemory.PlatformAPI.OpenNextFile = Win32OpenNextFile;  
            GameMemory.PlatformAPI.ReadDataFromFile = Win32ReadDataFromFile;  
            GameMemory.PlatformAPI.FileError = Win32FileError;
            
            GameMemory.PlatformAPI.PlatformAllocateMemory = Win32PlatformAllocateMemory;
            GameMemory.PlatformAPI.PlatformDeallocateMemory = Win32PlatformDeallocateMemory;
            
#if HANDMADE_INTERNAL            
            GameMemory.PlatformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.PlatformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.PlatformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
            GameMemory.PlatformAPI.DEBUGExecuteSystemCommmand = DEBUGExecuteSystemCommmand;
            GameMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif
            
            //TODO: Handle various memory footprints (using SYSTEM MWTRICS)
            //TODO: Use MEM_LARGE_PAGES and call adjust token privileges when not on windows XP
            
            //TODO: Transient Storage needs to be broken up into game transient and cache transient	and only the former need to be saved for state playback
            Win32State.TotalGameMemorySize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize + GameMemory.DebugStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalGameMemorySize, MEM_RESERVE|MEM_COMMIT , PAGE_READWRITE);
            
            
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = ((uint8*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);
            GameMemory.DebugStorage = ((uint8*)GameMemory.TransientStorage + GameMemory.TransientStorageSize);
            
            for(int ReplayIndex = 1;
                ReplayIndex < ArrayCount(Win32State.ReplayBuffers);
                ++ReplayIndex)
            {
                win32_replay_buffer * ReplayBuffer =  &Win32State.ReplayBuffers[ReplayIndex];
                // TODO: Recording system still seems to take too long
                // on record start - find out what Windows is doing and if
                // we can speed up / defer some of that processing.
                
                Win32GetInputFileLocation(&Win32State, false, ReplayIndex , 
                                          sizeof(ReplayBuffer->FileName), ReplayBuffer->FileName);		
                
                ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->FileName,
                                                       GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0,0);
                
                
                LARGE_INTEGER MaxSize;
                MaxSize.QuadPart = Win32State.TotalGameMemorySize;
                ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0,
                                                            PAGE_READWRITE,
                                                            MaxSize.HighPart,MaxSize.LowPart,
                                                            0);
                
                ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap , 
                                                          FILE_MAP_ALL_ACCESS,
                                                          0,0, 
                                                          Win32State.TotalGameMemorySize);
                
                
                //TODO; Change this to a log message
                if(ReplayBuffer->MemoryBlock)
                {
                    
                }
                else
                {
                    //TODO:Diagnostic
                }
            }
            
            if(GameMemory.PermanentStorage && GameMemory.TransientStorage && Samples)
            {
                //game_input controllers variables
                game_input Input[2] = {};
                game_input * NewInput =  &Input[0];
                game_input * OldInput = &Input[1];
                
                
                LARGE_INTEGER LastTimeCount = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();
                
                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarker[30] = {0};
                
                bool32 SoundPlayIsValid = false;
                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds =0;
                
                //uint64 LastCycleCount = __rdtsc();
                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath, GameCodeLockFullPath);
                
                while(GlobalRunning)
                {
                    //
                    //
                    //
                    
                    BEGIN_BLOCK(ExecutableReady);
                    
                    NewInput->dtForFrame = TargetSecondsPerFrame;
                    GameMemory.ExecutableReloaded = false;
                    GameMemory.LoopMemoryReset = false;
                    
                    FILETIME DLLNewWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if((CompareFileTime(&DLLNewWriteTime, &Game.DLLLastWriteTime)) != 0)
                    {
                        Win32CompleteAllWork(&HighPriorityQueue);
                        Win32CompleteAllWork(&LowPriorityQueue);
                        
#if HANDMADE_INTERNAL                        
                        //NOTE: Return pointer address back to local debugtable for this frame
                        GlobalDebugTable = &GlobalDebugTable_;
#endif
                        
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath,
                                                 GameCodeLockFullPath);
                        GameMemory.ExecutableReloaded = true;
                    }
                    
                    END_BLOCK(ExecutableReady);
                    
                    //
                    //
                    //
                    
                    BEGIN_BLOCK(InputProcessing);
                    
                    game_controller_input * OldKeyboardController = GetController(OldInput,0);
                    game_controller_input * NewKeyboardController = GetController(NewInput,0);
                    
                    //TODO: Zeroing macro
                    game_controller_input ZeroController = {};
                    *NewKeyboardController = ZeroController;
                    NewKeyboardController->IsConnected = true;
                    
                    for(int ButtonIndex= 0; 
                        ButtonIndex < ArrayCount(NewKeyboardController->Buttons); 
                        ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].ButtonEndedDown = 
                            OldKeyboardController->Buttons[ButtonIndex].ButtonEndedDown;
                        NewKeyboardController->Buttons[ButtonIndex].HalfTransitionCount = 0;
                    }
                    
                    //First we process messages
                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
                    
                    //XInput GamePad Controller
                    //TODO:Need to not poll disconnected controllers to avoid xinput frame rate hit 
                    //on older librares
                    //TODO:Shall we poll this more frequently
                    //NOTE:
                    //0 / Keyboard
                    //1 - (XUSERMAXCOUNT - 1)  / Gamepads
                    
                    //TODO: Shall we process XInput while Globalpause?
                    if(!GlobalPause)
                    {
                        POINT Cursor;
                        
                        GetCursorPos(&Cursor);
                        ScreenToClient(Window, &Cursor);
                        
                        //TODO(Alex): Do we want to handle pixel stretching so mouse coordinates fit properly?
                        NewInput->MouseX = (r32)Cursor.x;
                        NewInput->MouseY = (r32)(GlobalBackBuffer.Height - 1) - (r32)Cursor.y; 
                        NewInput->MouseZ = 0; //TODO(Alex):Support mouse wheel?  					
                        
#if HANDMADE_INTERNAL
                        WORD MouseKeyCodes[] =
                        {
                            VK_LBUTTON,
                            VK_MBUTTON,
                            VK_RBUTTON,
                            VK_XBUTTON1,
                            VK_XBUTTON2,
                            
                        };
                        
                        for(u32 ButtonIndex = 0;
                            ButtonIndex < ArrayCount(NewInput->MouseButtons);
                            ++ButtonIndex)
                        {
                            NewInput->MouseButtons[ButtonIndex] = OldInput->MouseButtons[ButtonIndex];
                            NewInput->MouseButtons[ButtonIndex].HalfTransitionCount = 0;
                            
                            b32 IsDown = ((GetKeyState(MouseKeyCodes[ButtonIndex]) >> 15) & 1);
                            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[ButtonIndex], IsDown);
                        }
                        
                        WORD CommandKeyCodes[] =
                        {
                            VK_SHIFT,
                            VK_CONTROL,
                            VK_MENU,
                        };
                        
                        for(u32 ButtonIndex = 0;
                            ButtonIndex < ArrayCount(NewInput->CommandButtons);
                            ++ButtonIndex)
                        {
                            NewInput->CommandButtons[ButtonIndex] = OldInput->CommandButtons[ButtonIndex];
                            NewInput->CommandButtons[ButtonIndex].HalfTransitionCount = 0;
                            
                            b32 IsDown = ((GetKeyState(CommandKeyCodes[ButtonIndex]) >> 15) & 1);
                            Win32ProcessKeyboardMessage(&NewInput->CommandButtons[ButtonIndex], IsDown);
                        }
#endif
                        
                        Win32ProcessKeyboardMessage(&NewKeyboardController->MoveUp, (GetKeyState(0x57) >> 15)  & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->MoveDown, (GetKeyState(0x53) >> 15)  & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->MoveLeft, (GetKeyState(0x41) >> 15)  & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->MoveRight, (GetKeyState(0x44) >> 15)  & 1);
                        
                        Win32ProcessKeyboardMessage(&NewKeyboardController->ActionUp, (GetKeyState(VK_UP) >> 15) & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->ActionDown, (GetKeyState(VK_DOWN) >> 15) & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->ActionLeft, (GetKeyState(VK_LEFT) >> 15) & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->ActionRight, (GetKeyState(VK_RIGHT) >> 15) & 1);
                        
                        Win32ProcessKeyboardMessage(&NewKeyboardController->LeftShoulder, (GetKeyState(0x51) >> 15)  & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->RightShoulder, (GetKeyState(0x45) >> 15)  & 1);
                        
                        Win32ProcessKeyboardMessage(&NewKeyboardController->Back, (GetKeyState(VK_ESCAPE) >> 15)  & 1);
                        Win32ProcessKeyboardMessage(&NewKeyboardController->Start, (GetKeyState(VK_SPACE) >> 15)  & 1);
                        
                        
                        DWORD MaxControllerCount = XUSER_MAX_COUNT;
                        if(MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
                        {
                            MaxControllerCount = (ArrayCount(NewInput->Controllers)-1);
                        }
                        
                        for (DWORD ControllerIndex = 0; 
                             ControllerIndex< MaxControllerCount; 
                             ++ControllerIndex)
                        {
                            
                            DWORD GamePadControllerIndex = ControllerIndex + 1;
                            game_controller_input * OldController = GetController(OldInput, GamePadControllerIndex);
                            game_controller_input * NewController = GetController(NewInput, GamePadControllerIndex);
                            
                            XINPUT_STATE StateController;
                            DWORD Result = XInputGetState(ControllerIndex,&StateController);
                            
                            if(Result == ERROR_SUCCESS)
                            {
                                //Controller is connected
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;
                                
                                //NOTE:See of StateController.dwPacketNumber increments rapidly
                                XINPUT_GAMEPAD * Pad = &StateController.Gamepad;
                                
                                //TODO: This is a square deadzone , check XInput to verify that dezdone is circular.
                                NewController->StickAverageX =  Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                
                                if((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f))
                                {
                                    NewController->IsAnalog = true;
                                }
                                
                                //TODO: GamePad
                                if((Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) == XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if((Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) == XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewController->StickAverageY = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if((Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) == XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if((Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) == XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewController->StickAverageX = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                
                                real32 Threshold = 0.5;
                                
                                //Digital Mapping of sticks 1:0
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX < -Threshold)? 1:0,
                                    &NewController->MoveLeft,
                                    &OldController->MoveLeft, 1);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageX > Threshold)? 1:0,
                                    &NewController->MoveRight,
                                    &OldController->MoveRight, 1);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY < -Threshold)? 1:0,
                                    &NewController->MoveDown,
                                    &OldController->MoveDown, 1);
                                Win32ProcessXInputDigitalButton(
                                    (NewController->StickAverageY > Threshold)? 1:0,
                                    &NewController->MoveUp,
                                    &OldController->MoveUp, 1);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->ActionLeft,&OldController->ActionLeft,XINPUT_GAMEPAD_X);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->ActionUp,&OldController->ActionUp,XINPUT_GAMEPAD_Y);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->ActionDown,&OldController->ActionDown,XINPUT_GAMEPAD_A);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->ActionRight,&OldController->ActionRight,XINPUT_GAMEPAD_B);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->LeftShoulder,&OldController->LeftShoulder,XINPUT_GAMEPAD_LEFT_SHOULDER);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->RightShoulder,&OldController->RightShoulder,XINPUT_GAMEPAD_RIGHT_SHOULDER);
                                
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->Start,&OldController->Start,XINPUT_GAMEPAD_START);
                                Win32ProcessXInputDigitalButton(Pad->wButtons,&NewController->Back,&OldController->Back,XINPUT_GAMEPAD_BACK);
                                
                            }
                            else
                            {
                                //Controller is disconnected
                                NewController->IsConnected = false;
                            }
                            
                        }//END: For
                        
                        /*NOTE: Vibration Test
                        XINPUT_VIBRATION VibrationController;
                        VibrationController.wLeftMotorSpeed = 65,535 /2;
                        VibrationController.wRightMotorSpeed = 65,535 /2;
                        XInputSetState(0,&VibrationController);
                        */
                    }
                    
                    END_BLOCK(InputProcessing);
                    
                    //
                    //
                    //
                    
                    BEGIN_BLOCK(GameUpdate);
                    //render
                    game_offscreen_buffer RenderBuffer = {};
                    RenderBuffer.Memory = GlobalBackBuffer.Memory;
                    RenderBuffer.Width = GlobalBackBuffer.Width;
                    RenderBuffer.Height = GlobalBackBuffer.Height;
                    RenderBuffer.Pitch = GlobalBackBuffer.Pitch;
                    
                    if(!GlobalPause)
                    {
                        if(Win32State.InputRecordingIndex)
                        {
                            Win32RecordingInput(&Win32State, NewInput);
                        }
                        if(Win32State.InputPlayingIndex)
                        {
                            game_input TempInput = *NewInput;
                            
                            Win32PlayBackInput(&Win32State, NewInput, &GameMemory);
                            
                            for(u32 ButtonIndex = 0;
                                ButtonIndex < PlatformMouseButton_Count;
                                ++ButtonIndex)
                            {
                                NewInput->MouseButtons[ButtonIndex] = TempInput.MouseButtons[ButtonIndex];
                            }
                            
                            NewInput->MouseX = TempInput.MouseX;
                            NewInput->MouseY = TempInput.MouseY;
                            NewInput->MouseZ = TempInput.MouseZ;
                        }
                        
                        if(Game.UpdateAndRender)
                        {
                            Game.UpdateAndRender(&GameMemory, NewInput, &RenderBuffer);
                        }
                        
                    }
                    
                    END_BLOCK(GameUpdate);
                    
                    //
                    //
                    //
                    
                    BEGIN_BLOCK(SoundUpdate);
                    
                    if(!GlobalPause)
                    {
                        //NOTE:Directsound output test
                        DWORD PlayCursor = 0;
                        DWORD WriteCursor = 0;
                        
                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds = (real32)Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);
                        
                        if(GlobalSecondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                            /*
                            NOTE: Here is how sound output computation works.
                            We define a safety value that is the number of samples we think 
                            our game update loop may vary by.
                            
                            When we wake up to write audio, we will look and see what the play 
                            cursor position is and we will forecast ahead where we think 
                            the playcursor will be on the next frame boundary.
                            
                            We will then look to see if the writecursor is befora that by our safe amount. 
                            It it is, the target fill position is that frame boundary plus one frame. 
                            This gives us a perfect audio sync in the case of a card that
                            has low enough latency.
                            
                            If the write cursor is AFTER the next frame play cursor estimate plus a 
                            safety margin, we assume we can never sync the audio perfectly, 
                            so we'll write one frame of audio plus some number of guard samples
                            (1 ms, or something determined to be safe).
                            
                            */
                            if(!SoundPlayIsValid)
                            {
                                SoundOutput.RunningSampleIndex = WriteCursor/SoundOutput.BytesPerSample;
                                SoundPlayIsValid = true;
                            }
                            
                            DWORD  ByteToLock = (DWORD)((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SoundBufferSize);
                            
                            
                            DWORD ExpectedSoundBytesPerFrame =
                                (int)((real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);
                            
                            real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
                            
                            DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);
                            
                            DWORD ExpectedFrameBoundaryByte = (DWORD)(PlayCursor + ExpectedBytesUntilFlip);
                            
                            DWORD SafeWriteCursor = WriteCursor;
                            if(SafeWriteCursor < PlayCursor)
                            {
                                //Unwrap SafeWriteCursor to same line as PlayCursor
                                SafeWriteCursor += SoundOutput.SoundBufferSize;
                            }
                            
                            Assert(SafeWriteCursor>=PlayCursor);
                            
                            SafeWriteCursor += SoundOutput.SafetyBytes;
                            
                            bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);
                            
                            DWORD TargetCursor = 0;
                            if(AudioCardIsLowLatency)
                            {
                                TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                            }
                            else
                            {
                                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
                            }
                            
                            TargetCursor =  (TargetCursor % SoundOutput.SoundBufferSize);
                            
                            DWORD BytesToWrite = 0;
                            if(ByteToLock > TargetCursor)
                            {
                                BytesToWrite = (SoundOutput.SoundBufferSize - ByteToLock);
                                BytesToWrite += TargetCursor;
                            }
                            else{
                                BytesToWrite= TargetCursor - ByteToLock;
                            }
                            
                            game_sound_output SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.Samples = Samples;
                            
                            SoundBuffer.SampleCount = Align4(BytesToWrite/SoundOutput.BytesPerSample);
                            BytesToWrite = SoundBuffer.SampleCount * SoundOutput.BytesPerSample;
                            
                            if(Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&GameMemory, &SoundBuffer);	
                            }
                            
                            
#if HANDMADE_INTERNAL
                            win32_debug_time_marker * TestTimeMarker = &DebugTimeMarker[DebugTimeMarkerIndex];
                            
                            TestTimeMarker->OutputPlayCursor = PlayCursor;
                            TestTimeMarker->OutputWriteCursor = WriteCursor;
                            TestTimeMarker->OutputByteToLock = ByteToLock ;
                            TestTimeMarker->OutputBytesToWrite = BytesToWrite;
                            TestTimeMarker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
                            
                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if(UnwrappedWriteCursor < PlayCursor)
                            {
                                UnwrappedWriteCursor += SoundOutput.SoundBufferSize;
                            }
                            AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                            /*if(WriteCursor > PlayCursor)
                            {
                            AudioLatencyBytes = WriteCursor - PlayCursor;
                            }
                            else
                            {
                            AudioLatencyBytes = SoundOutput.SoundBufferSize - PlayCursor;
                            AudioLatencyBytes += WriteCursor;
                            }*/
                            
                            AudioLatencySeconds = (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);
#if 0
                            char TextBuffer[256];
                            _snprintf_s(TextBuffer, sizeof(TextBuffer),"BytesToLock: %u ,  
                                        TargetCursor: %u , BytesToWrite: %u , PlayCursor: %u , 
                                        WriteCursor: %u , Delta: %u  (%fs) \n", 
                                        ByteToLock, TargetCursor, BytesToWrite, 
                                        PlayCursor, WriteCursor, 
                                        AudioLatencyBytes, AudioLatencySeconds);
                            OutputDebugStringA(TextBuffer);
#endif							
#endif
                            Win32FillSoundBuffer(&SoundOutput, &SoundBuffer, ByteToLock, BytesToWrite);
                            
                        }
                        else
                        {
                            SoundPlayIsValid = false;
                        }
                    }
                    
                    END_BLOCK(SoundUpdate);
                    
                    //
                    //
                    //
#if HANDMADE_INTERNAL
                    BEGIN_BLOCK(DebugCollation);
                    
                    if(Game.DEBUGFrameEnd)
                    {
                        GlobalDebugTable = Game.DEBUGFrameEnd(&GameMemory, NewInput, &RenderBuffer);
                    }
                    //NOTE: Prevents overflow of local DEBUG table
                    GlobalDebugTable_.EventArrayIndex_EventIndex = 0;
                    
                    END_BLOCK(DebugCollation);
#endif
                    //
                    //
                    //
                    
#if 0
                    //TODO(Alex): Leave this off until we have vblank support
                    BEGIN_BLOCK(FramerateWait);
                    
                    if(!GlobalPause)
                    {
                        real32 SecondsElapsedForFrame = Win32GetSecondsElapsed(LastTimeCount, Win32GetWallClock());
                        
                        //TODO: Not tested yet probably buggy
                        if(SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            //Assert(TestSecondsElapsedForFrame < TargetSecondsPerFrame);
                            if(SleepIsGranular)
                            {
                                DWORD MillisecondsToWait = (DWORD)(1000.0f*(TargetSecondsPerFrame-SecondsElapsedForFrame));
                                if(MillisecondsToWait > 0)
                                {
                                    Sleep(MillisecondsToWait);
                                }
                            }
                            
                            SecondsElapsedForFrame  = Win32GetSecondsElapsed(LastTimeCount, Win32GetWallClock());
                            
                            if(SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                //TODO:Log missed here
                            }
                            
                            while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastTimeCount, Win32GetWallClock());
                            }
                        }
                        else
                        {
                            //TODO: FrameRate Missed!
                            //TODO: Logging
                        }
                    }
                    
                    END_BLOCK(FramerateWait);
#endif
                    
                    //
                    //
                    //
                    
                    BEGIN_BLOCK(EndOfFrame);
                    
                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                    
                    HDC DeviceContext = GetDC(Window);
                    Win32UpdateWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                    
                    ReleaseDC(Window,DeviceContext);
                    FlipWallClock = Win32GetWallClock();
                    
                    game_input * TempInput = NewInput;
                    NewInput = OldInput;
                    OldInput = TempInput;
                    
                    END_BLOCK(EndOfFrame);
                    
                    LARGE_INTEGER EndTimeCount = Win32GetWallClock();
                    FRAME_MARKER(Win32GetSecondsElapsed(LastTimeCount, EndTimeCount));
                    LastTimeCount = EndTimeCount;
                }
            }
            else
            {
                //TODO: Logging
            }
        }
        else
        {
            //TODO: Logging
        }
    }
    else
    {
        //TODO: Logging
    }
    
    //MessageBox(0,"This is handmade hero.","Handmade Hero", MB_OK|MB_ICONINFORMATION);
    return 0;
}

