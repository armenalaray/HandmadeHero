#if !defined HANDMADE_PLATFORM_H

//#include "handmade_config.h"

/*
NOTE:

HANDMADE_INTERNAL:
 0 - Build for Public release 
 1 - Build for developer only
 
HANDMADE_SLOW_PERFORMANCE:
 0 - Not slow code allowed
 1 - Slow code allowed
 
*/

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
    
#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif
    
#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif
    
#if !(COMPILER_LLVM || COMPILER_MSVC)
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
    //TODO: More compilers
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif	
#endif
    
    
#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM 
#include <x86intrin.h>
#else
#error SSE / NEON optimizations are not available for this compiler yet 
#endif
    
    typedef uint8_t uint8;
    typedef uint16_t uint16;
    typedef uint32_t uint32;
    typedef uint64_t uint64;
    
    typedef int8_t  int8;
    typedef int16_t int16;
    typedef int32_t int32;
    typedef int64_t int64;
    typedef uintptr_t uintptr;
    typedef intptr_t intptr;
    typedef int32 bool32;
    typedef float real32;
    typedef double real64;
    
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;
    
    typedef int8_t  s8;
    typedef int16_t s16;
    typedef int32_t s32;
    typedef int64_t s64;
    
    typedef uintptr_t uptr;
    typedef intptr_t sptr;
    
    typedef bool32 b32;
    
    typedef float r32;
    typedef double r64;
    
    typedef size_t memory_index;
    
    
    //Static One translation unit
#if !defined(internal)
#define internal static 
#endif
    
#define local_persist static
#define global_variable static
    
#define Pi32 3.1415926535f
#define Tau32 6.283185307179f  
    
#define Real32Max FLT_MAX
#define U32Max UINT_MAX
#define S32Max INT_MAX
    
    
#if  HANDMADE_SLOW_PERFORMANCE
    
#define Assert(Expression)\
    if (!(Expression)){ * ( int * ) 0 = 0;}
    
#else
#define Assert(Expression)
#endif
    
#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)
    
#define ArrayCount(Array) (sizeof (Array) / sizeof((Array)[0]))
    
#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break
    
#define AlignPower2(Value, Alignment) ((((Value) + ((Alignment) - 1))) & ~((Alignment) - 1))
#define Align4(Value) AlignPower2(Value, 4)
#define Align8(Value) AlignPower2(Value, 8)
#define Align16(Value) (((Value) + 15) & -16)
    
#define Introspection(Param) 
    //TODO:Swap, min, max , -- MACROS?
    
#pragma pack(push, 1)
    struct bitmap_id
    {
        uint32 Value;
    };
    
    struct sound_id
    {
        uint32 Value;
    };
    
    struct font_id
    {
        u32 Value;
    };
#pragma pack(pop)
    
    //Introspection(category:vector_2) 
    union vector_2
    {
        struct 
        {
            real32 x;
            real32 y;
        };	
        
        struct 
        {
            real32 u;
            real32 v;
        };
        
        real32 E[2];
    };
    
    //Introspection(category:vector_2) 
    union vector_3
    {
        struct 
        {
            real32 x;
            real32 y;
            real32 z;
        };
        struct 
        {
            real32 r;
            real32 g;
            real32 b;
        };
        struct 
        {
            real32 u;
            real32 v;
            real32 w;
        };
        struct 
        {
            vector_2 xy;
            real32 Ignored0_;
        };
        struct 
        {
            real32 Ignored1_;
            vector_2 yz;
        };
        struct 
        {
            vector_2 uv;
            real32 Ignored2_;
        };
        struct 
        {
            real32 Ignored3_;
            vector_2 vw;
        };
        real32 E[3];
    };
    
    union vector_4
    {
        struct 
        {
            union
            {
                vector_3 xyz;
                struct
                {
                    real32 x, y, z;
                };
                
            };
            real32 w;
        };
        struct 
        {
            union
            {
                vector_3 rgb;
                struct
                {
                    real32 r, g, b;
                };
            };
            real32 a;
        };
        struct 
        {
            vector_2 xy;
            real32 Ignored0_;
            real32 Ignored1_;
            
        };
        struct 
        {
            real32 Ignored2_;
            vector_2 yz;
            real32 Ignored3_;
        };
        struct 
        {
            real32 Ignored4_;
            real32 Ignored5_;
            vector_2 zw;
        };
        real32 E[4];
    };
    
    struct rectangle_2
    {
        vector_2 Min;
        vector_2 Max;
    };
    
    Introspection(category:rectangle_3) struct rectangle_3
    {
        vector_3 Min;
        vector_3 Max;
    };
    
    
    struct rectangle_2i
    {
        int32 Minx, Maxx;
        int32 Miny, Maxy;
    };
    
    
    
    inline u32 
        SafeTruncateToUint32(uint64 Value)
    {
        Assert(Value <= 0xFFFFFFFF);
        u32 Result  = (u32)Value;
        return Result;
    }
    
#if 0
    inline u16 
        SafeTruncateToUint16(int32 Value)
    {
        Assert(Value <= 65535);
        Assert(Value >= 0);
        
        u16 Result  = (u16)Value;
        return Result;
    }
    
    inline s16 
        SafeTruncateToInt16(int32 Value)
    {
        Assert(Value <= 32767);
        Assert(Value >= -32768);
        
        s16 Result = (s16)Value;
        return Result;
    }
#endif
    
    //TODO(Alex) Try internal here
    inline void ConcatStringsA(size_t SourceASize, char * SourceA,
                               size_t SourceBSize, char * SourceB,
                               size_t DestCount, char * Dest)
    {
        //TODO: Test bounds checking
        Assert(SourceASize + SourceBSize < (DestCount - 1));
        
        for (int Index = 0; Index < SourceASize; ++Index)
        {
            *Dest++ = *SourceA++;
        }
        for (int Index = 0; Index < SourceBSize; ++Index)
        {
            *Dest++ = *SourceB++;
        }
        //inserts null terminator
        *Dest++ = 0;
    }
    
    inline u32 StringLength(char * String)
    {
        int Count = 0;
        while (*String++)
        {
            ++Count;
        }
        return (Count);
    }
    
    
    inline void 
        Copy(u64 Size, void * Source, void * Dest)
    {
        u8 * S8 = (u8 *)Source;
        u8 * D8 = (u8 *)Dest;
        
        while(Size--)
        {
            *D8++ = *S8++;
        }
    }
    
    
    //NOTE(Alex): DLIST Macro
#define DLIST_ADD_TAIL(Sentinel, Element)\
    (Element)->Next = (Sentinel);\
    (Element)->Prev = (Sentinel)->Prev;\
    (Element)->Prev->Next = (Element);\
    (Element)->Next->Prev = (Element);
    
#define DLIST_INIT(Sentinel)\
    (Sentinel)->Next = (Sentinel);\
    (Sentinel)->Prev = (Sentinel);
    
    
    //NOTE: Services that the platform layer provides to the game
#if HANDMADE_INTERNAL
    /*
    IMPORTANT: These are NOT for doing anything in the shipping game
    */ 
    
    typedef struct debug_file_content
    {
        void* Content;
        uint32 ContentSize;
    }debug_file_content;
    
    typedef struct debug_executing_process
    {
        u64 OSHandle;
        
    }debug_executing_process;
    
    typedef struct debug_process_state
    {
        b32 ProcessStarted;
        b32 IsRunning;
        u32 ExitCode;
        
    }debug_process_state;
    
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void * Memory)
    typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);
    
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_file_content name(char *Filename)
    typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);
    
#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize32, void * Memory)
    typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
    
#define DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(name)  debug_executing_process name(char * Path, char * Command, char * CommandLine)
    typedef DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(debug_platform_execute_system_command);
    
#define DEBUG_PLATFORM_GET_PROCESS_STATE(name)  debug_process_state  name(debug_executing_process ExecutingProcess)
    typedef DEBUG_PLATFORM_GET_PROCESS_STATE(debug_platform_get_process_state);
    
#endif
    //NOTE: Services that the game provides to the platform layer
    //FOUR THINGS - Timing. controller/keyboard input, bitmap to output,  sound buffer to output
    
    //TODO: in the future, rendering specifically will become a three tiered abstraction!!!
#define BITMAP_BYTES_PER_PIXEL 4
    
    typedef struct game_offscreen_buffer
    {
        void * Memory;
        int Width;
        int Height;
        int Pitch;
    }game_offscreen_buffer;
    
    //NOTE: SampleCount has to be a multiple of 4 
    typedef struct game_sound_output
    {
        int SamplesPerSecond;
        int SampleCount;
        int16 * Samples;
    }game_sound_output;
    
    typedef struct game_button_state
    {
        bool32 ButtonEndedDown;
        int HalfTransitionCount;
    }game_button_state;
    
    inline b32 WasPressed(game_button_state * State)
    {
        b32 Result = ((State->HalfTransitionCount > 1) || 
                      ((State->HalfTransitionCount == 1) && State->ButtonEndedDown));
        return Result;
    }
    
    inline b32 WasReleased(game_button_state * State)
    {
        b32 Result = ((State->HalfTransitionCount > 1) || 
                      ((State->HalfTransitionCount == 1) && !State->ButtonEndedDown));
        return Result;
    }
    
    typedef struct game_controller_input
    {	
        bool32 IsConnected;
        bool32 IsAnalog;
        real32 StickAverageX;
        real32 StickAverageY;
        
        union
        {
            game_button_state Buttons[13];
            struct 
            {
                game_button_state MoveUp;
                game_button_state MoveDown;
                game_button_state MoveLeft;
                game_button_state MoveRight;
                
                game_button_state ActionUp;
                game_button_state ActionDown;
                game_button_state ActionLeft;
                game_button_state ActionRight;
                
                game_button_state LeftShoulder;
                game_button_state RightShoulder;
                
                game_button_state Start;
                game_button_state Back;
                game_button_state Terminator;
            };
        };
    }game_controller_input;
    
    
#if HANDMADE_INTERNAL    
    enum platform_mouse_buttons
    {
        PlatformMouseButton_Left,
        PlatformMouseButton_Middle,
        PlatformMouseButton_Right,
        PlatformMouseButton_Extended0,
        PlatformMouseButton_Extended1,
        PlatformMouseButton_Count,
    };
    
    
    enum platform_command_buttons
    {
        PlatformCommandButton_Shift,
        PlatformCommandButton_Ctrl,
        PlatformCommandButton_Alt,
        PlatformCommandButton_Count,
    };
#endif
    
    typedef struct game_input
    {
#if HANDMADE_INTERNAL        
        game_button_state MouseButtons[PlatformMouseButton_Count];
        game_button_state CommandButtons[PlatformCommandButton_Count];
        r32 MouseX, MouseY, MouseZ;
#endif
        r32 dtForFrame;
        game_controller_input Controllers[5];
        
    }game_input;
    
    
    inline game_controller_input * GetController(game_input *Input, uint32 ControllerIndex)
    {
        Assert(ControllerIndex < ArrayCount(Input->Controllers));
        game_controller_input * Result = &Input->Controllers[ControllerIndex];
        return Result;
    }
    
    typedef struct platform_file_handle
    {
        b32 NoErrors;
        void * Platform;
    }platform_file_handle;
    
    typedef struct platform_file_group
    {
        u32 FileCount;
        void * Platform;
    }platform_file_group;
    
    typedef enum platform_file_type
    {
        PlatformFileType_AssetFile,
        PlatformFileType_SaveFile
    }platform_file_type;
    
#define BEGIN_PLATFORM_GIVE_ME_ALL_FILES_OF_EXTENSION(name) platform_file_group name(platform_file_type FileType)
    typedef BEGIN_PLATFORM_GIVE_ME_ALL_FILES_OF_EXTENSION(begin_platform_give_me_all_files_of_extension);
    
#define END_PLATFORM_GIVE_ME_ALL_FILES_OF_EXTENSION(name) void name(platform_file_group *  FileGroup)
    typedef END_PLATFORM_GIVE_ME_ALL_FILES_OF_EXTENSION(end_platform_give_me_all_files_of_extension);
    
#define PLATFORM_OPEN_NEXT_FILE(name) platform_file_handle name(platform_file_group * FileGroup)
    typedef PLATFORM_OPEN_NEXT_FILE(platform_open_next_file);  
    
#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle * Source, u64 Offset, u64 Size, void * Dest)
    typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);  
    
#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle * Handle, char * Message)
    typedef PLATFORM_FILE_ERROR(platform_file_error);
    
#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors)
    
    
    struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue * Queue, void * Data)
    typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);
    typedef void platform_add_entry(platform_work_queue * Queue, platform_work_queue_callback * CallBack, void * Data);
    typedef void platform_complete_all_work(platform_work_queue * Queue);
    
    
#define PLATFORM_ALLOCATE_MEMORY(name) void * name(memory_index Size)
    typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);
    
#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void * Memory)
    typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);
    
    
    typedef struct platform_api
    {
        platform_add_entry * AddEntry;
        platform_complete_all_work * CompleteAllWork;
        
        begin_platform_give_me_all_files_of_extension * BeginGiveMeAllFilesOfExtension;
        end_platform_give_me_all_files_of_extension * EndGiveMeAllFilesOfExtension;
        platform_open_next_file * OpenNextFile;  
        platform_read_data_from_file * ReadDataFromFile;  
        platform_file_error * FileError;
        
        platform_allocate_memory * PlatformAllocateMemory;
        platform_deallocate_memory * PlatformDeallocateMemory;
        
#if HANDMADE_INTERNAL        
        debug_platform_free_file_memory * DEBUGFreeFileMemory;
        debug_platform_read_entire_file * DEBUGReadEntireFile;
        debug_platform_write_entire_file * DEBUGWriteEntireFile;
        debug_platform_execute_system_command * DEBUGExecuteSystemCommmand;
        debug_platform_get_process_state * DEBUGGetProcessState;
#endif
        
    }platform_api;
    
    typedef struct game_memory
    {
        b32 ExecutableReloaded;
        b32 LoopMemoryReset;
        r32 LastMenuPosX;
        r32 LastMenuPosY;
        
        uint64 PermanentStorageSize;
        void * PermanentStorage; //NOTE: Required to  be initialized to zeros
        uint64 TransientStorageSize;
        void * TransientStorage;
        uint64 DebugStorageSize;
        void * DebugStorage;
        
        platform_work_queue * HighPriorityQueue;
        platform_work_queue * LowPriorityQueue;
        
        platform_api PlatformAPI;
        
    }game_memory;
    
    //DEBUG: Logging exposed to the platform layer
    
#if COMPILER_MSVC
    
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFututeWrites _WriteBarrier()
    
    inline u32 
        GetThreadID()
    {
        u8* GSPtr =  (u8*)__readgsqword((unsigned long)0x30);  
        u32 Result = *(u32*)(GSPtr + 0x48);
        
        return Result;
    }
    
    
    inline u32 
        AtomicCompareExchangeUint32(u32 volatile * Dest, u32 New, u32 Expected) 
    {
        u32 Result = _InterlockedCompareExchange((long *)Dest, New, Expected);
        return Result;
    }
    
    inline u64
        AtomicExchangeU64(u64 volatile * Dest, u64 New) 
    {
        u64 Result =  _InterlockedExchange64((__int64 *) Dest, New);
        return Result;
    }
    
    inline u64
        AtomicAddU64(u64 volatile * Dest, u64 Addend)
    {
        //NOTE: _InterlockedExchangeAdd64 returns the value of Dest prior to adding
        u64 Result =  _InterlockedExchangeAdd64((__int64*)Dest, Addend);
        return Result;
    }
    
    inline u32
        AtomicAddU32(u32 volatile * Dest, u32 Addend)
    {
        //NOTE: _InterlockedExchangeAdd64 returns the value of Dest prior to adding
        u32 Result =  _InterlockedExchangeAdd((long*)Dest, Addend);
        return Result;
    }
    
#elif COMPILER_LLVM
    
    //TODO: Does LLVM has read-specific barriers yet? 
#define CompletePreviousReadsBeforeFutureReads asm volatile("" ::: "memory")
#define CompletePreviousWritesBeforeFututeWrites asm volatile("" ::: "memory")
    
    inline u32 
        GetThreadID()
    {
        u32 ThreadID:
#if defined(__APPLE__) && defined(__x86_64__)
        asm("mov %%gs:0x00,%0" : "=r"(ThreadID));
#elif defined(__i386__)
        asm("mov %%gs:0x08,%0" : "=r"(ThreadID));
#elif defined(__x86_64__)
        asm("mov %%fs:0x10,%0" : "=r" (ThreadID));
#else
#error Unsupported architecture
#endif
        
        return ThreadID;
    }
    
    inline uint32 
        AtomicCompareExchangeUint32(uint32 volatile * Dest, uint32 New, uint32 Expected) 
    {
        uint32 Result = __sync_val_compare_and_swap(Value, Expected, New);
        return Result;
    }
    
    
    inline u32
        AtomicAddU32(u32 volatile * Dest, u32 Addend)
    {
        u32 Result =  (_InterlockedExchangeAdd((long *)Dest, Addend) + Addend);
        return Result;
    }
    
    inline u64
        AtomicExchangeU64(u64 volatile * Dest, u64 New) 
    {
        u64 Result =  __sync_lock_test_and_set(Dest, New);
        return Result;
    }
    
    inline u64
        AtomicAddU64(u64 volatile * Dest, u64 Addend)
    {
        //NOTE: _InterlockedExchangeAdd64 returns the value of Dest prior to adding
        u64 Result =  __sync_fetch_and_add(Dest, Addend);
        return Result;
    }
    
#else
    //TODO: More compilers?
#endif
    
#define GAME_UPDATE_AND_RENDER(name) void name (game_memory * Memory, game_input * Input, game_offscreen_buffer * Buffer)
    typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
    
    //NOTE: At this moment this function has to be very fast, not more than one millisecond
    //TODO: Reduce the pressure on this function's performance by measuring os asking about it
#define GAME_GET_SOUND_SAMPLES(name) void name (game_memory * Memory, game_sound_output * SoundBuffer)
    typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
    
    
#ifdef __cplusplus
}
#endif


#include "handmade_debug_header.h"

#define HNADMADE_PLATFORM_H
#endif


