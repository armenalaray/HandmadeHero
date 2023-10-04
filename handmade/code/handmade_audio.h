#ifndef AUDIO_H
#define AUDIO_H


struct playing_sound
{
    real32 Real32SamplesPlayed;
    
    vector_2 CurrentVolume;
    vector_2 TargetVolume;
    vector_2 dCurrentVolume;
    
    real32 dSample;
    
    sound_id ID;
    playing_sound * Next;
};


struct audio_state
{
    memory_arena * PermArena;
    playing_sound * FirstPlayingSound;
    playing_sound * FirstFreePlayingSound;
    
    vector_2 TotalVolume;
};



#endif

