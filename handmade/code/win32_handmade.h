#ifndef WIN32_HANDAMADE
#define WIN32_HANDAMADE

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void * Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    //NOTE: Sound Test Variables
    uint32 RunningSampleIndex = 0;
    int SamplesPerSecond = 48000;
    int BytesPerSample = sizeof(int16)*2;
    int SoundBufferSize = SamplesPerSecond*BytesPerSample;
    
    DWORD SafetyBytes;
    //NOTE:Math gets simpler  if we add a bytes per second field?
};

struct win32_debug_time_marker 
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputByteToLock;
    DWORD OutputBytesToWrite;
    DWORD ExpectedFlipPlayCursor;
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};


struct win32_game_code
{
    HMODULE GameCodeDLL;
    
    //NOTE: Either of the callbacks can be NULL
    //IMPORTANT: You must check before calling
    game_update_and_render * UpdateAndRender;
    game_get_sound_samples * GetSoundSamples;
    
#if HANDMADE_INTERNAL    
    game_debug_frame_end * DEBUGFrameEnd;
#endif
    
    FILETIME DLLLastWriteTime;
    bool32 IsValid;
};




#define WIN32_STATE_FILE_NAME_SIZE MAX_PATH

struct win32_replay_buffer
{
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char  FileName[WIN32_STATE_FILE_NAME_SIZE];
    void * MemoryBlock;
    
};

struct win32_state
{
    uint64 TotalGameMemorySize;
    void * GameMemoryBlock;
    
    win32_replay_buffer ReplayBuffers[4];
    
    HANDLE RecordingHandle;
    int InputRecordingIndex = 0;
    
    HANDLE PlayBackHandle;
    int InputPlayingIndex = 0;
    
    char  EXEFilename[WIN32_STATE_FILE_NAME_SIZE];
    char * LastSlashEXEFileName;
};

#endif




