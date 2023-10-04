#include "handmade_audio.h"

/*
NOTE: Check to lock framerate, or check audio-locks-to-write in win32_handmade.cpp 
and  there's some bug in there

*/

#if 0
internal void 
OutputTestSineWave(game_sound_output *SoundBuffer, game_state * GameState)
{
    int16 SoundAmplitude = 500;
    int WavePeriod = SoundBuffer->SamplesPerSecond/400;
    
    int16 * SampleWritten = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {	
#if 1
        real32 SineValue = (real32)sinf(GameState->tSine);
        int16 SampleValue =  (int16)(SineValue  * SoundAmplitude);
#else 
        int16 SampleValue = 0;
#endif	
        *SampleWritten++ = SampleValue;
        *SampleWritten++ = SampleValue;
        
#if 1		
        GameState->tSine += (real32)(Tau32  / (real32)WavePeriod);
        
        if(GameState->tSine >= Tau32)
        {
            GameState->tSine -= (real32)(Tau32);
        }
#endif		
        
    }
}
#endif

internal playing_sound *  
PlaySound(audio_state * AudioState, sound_id SoundID)
{
    TIMED_FUNCTION();
    
    playing_sound * PlayingSound = 0;
    
    if(IsValid(SoundID))
    {
        if(!AudioState->FirstFreePlayingSound)
        {
            AudioState->FirstFreePlayingSound = PushStruct(AudioState->PermArena, playing_sound);
            AudioState->FirstFreePlayingSound->Next = 0;
        }
        
        PlayingSound = AudioState->FirstFreePlayingSound; 
        AudioState->FirstFreePlayingSound = PlayingSound->Next;
        
        PlayingSound->Real32SamplesPlayed = 0;
        //TODO: Should these be 0.5 / 0.5f default centered?
        PlayingSound->CurrentVolume.E[0] = PlayingSound->TargetVolume.E[0] = 1.0f;
        PlayingSound->CurrentVolume.E[1] = PlayingSound->TargetVolume.E[1] = 1.0f;
        
        PlayingSound->dCurrentVolume.E[0] = PlayingSound->dCurrentVolume.E[1] = 0.0f;
        PlayingSound->dSample = 1.0f;
        
        PlayingSound->ID = SoundID;
        
        PlayingSound->Next = AudioState->FirstPlayingSound;;
        AudioState->FirstPlayingSound = PlayingSound;
    }
    
    return PlayingSound;
}
internal void 
ChangeVolume(playing_sound * Sound, vector_2 VolumeForChannels, vector_2 TransitionInSeconds)
{
    TIMED_FUNCTION();
    if(Sound)
    {
        Sound->TargetVolume = VolumeForChannels;
        Assert(TransitionInSeconds.E[0] >= 0 && TransitionInSeconds.E[1] >= 0);
        //NOTE: If TransitionTime is 0 the volume change is instantly
        
        if(TransitionInSeconds.E[0] == 0)
        {
            Sound->CurrentVolume.E[0] = Sound->TargetVolume.E[0];
        }
        else
        {
            real32 OneOverTransition = 1.0f / TransitionInSeconds.E[0];
            Sound->dCurrentVolume.E[0] = (Sound->TargetVolume.E[0] - Sound->CurrentVolume.E[0]) * OneOverTransition;
        }
        
        if(TransitionInSeconds.E[1] == 1)
        {
            Sound->CurrentVolume.E[1] = Sound->TargetVolume.E[1];
        }
        else
        {
            real32 OneOverTransition = 1.0f / TransitionInSeconds.E[1];
            Sound->dCurrentVolume.E[1] = (Sound->TargetVolume.E[1] - Sound->CurrentVolume.E[1]) * OneOverTransition;
        }
    }
}

internal void
ChangePitch(playing_sound * Sound, real32 dSample)
{
    if(Sound)
    {
        Sound->dSample = dSample;
    }
}

inline bool32
IsAnimating(vector_2 dVolume)
{
    bool32 Result = ((dVolume.E[0] != 0) && (dVolume.E[1] != 0));
    return Result;
}

internal void
OutputPlayingSounds(audio_state * AudioState,game_sound_output * SoundBuffer, 
                    game_assets * Assets, memory_arena * TempArena)
{
    TIMED_FUNCTION();
    
    temp_memory TempMemory = BeginTempMemory(TempArena);
    
    u32 GenerationIndex = BeginGenerationIndex(Assets);
    
    //NOTE:  Zero out Channels
    //NOTE:SoundBuffer->SampleCount Always aligned to a 4 bit boundary
    Assert((SoundBuffer->SampleCount & 3) == 0);
    
    u32 ChunkCount = (SoundBuffer->SampleCount / 4);
    
    __m128 * RealChannel0 = PushArray(TempArena, ChunkCount, __m128, 16);
    __m128 * RealChannel1 = PushArray(TempArena, ChunkCount, __m128, 16);
    
    __m128 Half = _mm_set1_ps(0.5f);
    __m128 Zero = _mm_set1_ps(0.0f); 
    __m128 One = _mm_set1_ps(1.0f);
    //__m128 MaxS16 = _mm_set1_ps(0x7fff);
    //__m128 MinS16 = _mm_set1_ps(~0x7fff);
    {
        __m128 * Dest0 = RealChannel0;
        __m128 * Dest1 = RealChannel1;
        
        for(u32 SampleIndex = 0; 
            SampleIndex < ChunkCount; 
            ++SampleIndex)
        {
            _mm_store_ps ((float*)Dest0++,Zero);
            _mm_store_ps ((float*)Dest1++,Zero);
        }
    }
    
#define AudioStateOutputChannelCount 2
    real32 RealSecondsPerSample = 1.0f  / (real32)SoundBuffer->SamplesPerSecond;
    
    
    //NOTE: Sound mixer
    for(playing_sound ** PlayingSoundPtr = &AudioState->FirstPlayingSound; 
        *PlayingSoundPtr;
        )
    {
        playing_sound * PlayingSound = *PlayingSoundPtr;
        
        __m128 * Dest0 = RealChannel0;
        __m128 * Dest1 = RealChannel1;
        
        bool32 SoundFinished = false;
        u32 TotalChunksToMix = ChunkCount;
        
        while(!SoundFinished && TotalChunksToMix)
        {
            loaded_sound * LoadedSound = GetSound(Assets, PlayingSound->ID, GenerationIndex);
            if(LoadedSound)
            {
                //TODO: Fix pairing of sound chunks,  when dSample is a Real value, 
                //There's a glitch in sound
                sound_id NextID = GetNextSoundInChain(Assets, PlayingSound->ID);
                PrefetchSound(Assets,NextID);
                
                vector_2 dVolumePerSample = RealSecondsPerSample * PlayingSound->dCurrentVolume; 
                vector_2 dVolumePerChunk = 4.0f * dVolumePerSample;
                
                
                real32 dSample = PlayingSound->dSample * 1.0f;
                real32 dSampleChunk = 4.0f * dSample;
                
                __m128 TotalVolume0 = _mm_set1_ps(AudioState->TotalVolume.E[0]);
                __m128 CurrentVolume0 = _mm_setr_ps(PlayingSound->CurrentVolume.E[0] + dVolumePerSample.E[0], 
                                                    PlayingSound->CurrentVolume.E[0] + 2.0f * dVolumePerSample.E[0], 
                                                    PlayingSound->CurrentVolume.E[0] + 3.0f * dVolumePerSample.E[0], 
                                                    PlayingSound->CurrentVolume.E[0] + 4.0f * dVolumePerSample.E[0]);
                __m128 dVolumePerChunk0 = _mm_set1_ps(dVolumePerChunk.E[0]);
                __m128 dVolumePerSample0 = _mm_set1_ps(dVolumePerSample.E[0]);
                
                __m128 TotalVolume1 = _mm_set1_ps(AudioState->TotalVolume.E[1]);
                __m128 CurrentVolume1 = _mm_setr_ps(PlayingSound->CurrentVolume.E[1] + dVolumePerSample.E[1], 
                                                    PlayingSound->CurrentVolume.E[1] + 2.0f * dVolumePerSample.E[1], 
                                                    PlayingSound->CurrentVolume.E[1] + 3.0f * dVolumePerSample.E[1], 
                                                    PlayingSound->CurrentVolume.E[1] + 4.0f * dVolumePerSample.E[1]);
                __m128 dVolumePerChunk1 = _mm_set1_ps(dVolumePerChunk.E[1]);
                __m128 dVolumePerSample1 = _mm_set1_ps(dVolumePerSample.E[1]);
                
                Assert(PlayingSound->Real32SamplesPlayed >= 0);
                
                u32 ChunksToMix = TotalChunksToMix;
                r32 RealSoundMissingChunks = (LoadedSound->SampleCount - PlayingSound->Real32SamplesPlayed) / dSampleChunk;
                u32 LoadedSoundMissingChunks = CeilReal32ToInt32(RealSoundMissingChunks);
                
                if(ChunksToMix > LoadedSoundMissingChunks)
                {
                    ChunksToMix = LoadedSoundMissingChunks;
                }
                
                u32 VolumeEndsAt[AudioStateOutputChannelCount] = {};
                //s32 ChunksToAnimate = 0x7fff;
                
                for(u32 ChannelIndex = 0;
                    ChannelIndex < AudioStateOutputChannelCount;
                    ++ChannelIndex)
                {
                    if(dVolumePerChunk.E[ChannelIndex] != 0)
                    {
                        //TODO: Fix the both volumes end at the same time 
                        real32 OneOverdVolumePerChunk = 1.0f / dVolumePerChunk.E[ChannelIndex];
                        real32 DeltaVolume = PlayingSound->TargetVolume.E[ChannelIndex] - PlayingSound->CurrentVolume.E[ChannelIndex];
                        s32 ChunksToAnimate = RoundReal32ToInt32(DeltaVolume * OneOverdVolumePerChunk);
                        
                        //TODO: Check if chunks to animate is ever < 0 
                        if(ChunksToAnimate < 0)
                        {
                            ChunksToAnimate = 0;
                        }
                        
                        if(ChunksToMix > (u32)ChunksToAnimate)
                        {
                            ChunksToMix = ChunksToAnimate;
                            VolumeEndsAt[ChannelIndex] = ChunksToAnimate;
                        }
                    }
                }
                
                r32 BeginSamplePosition = PlayingSound->Real32SamplesPlayed;
                r32 EndSamplePosition = BeginSamplePosition + (r32)CeilReal32ToInt32(ChunksToMix * dSampleChunk);
                
                r32 C = (EndSamplePosition - BeginSamplePosition) / (r32)ChunksToMix;
                for(u32 ChunkIndex = 0;
                    ChunkIndex < ChunksToMix;
                    ++ChunkIndex)
                {
                    real32 SamplePosition = BeginSamplePosition + (C * (r32)ChunkIndex);
                    //TODO: Move sound here and make explicit calculation
                    
                    //NOTE: Bilinear filtering
#if 1
                    __m128 SampleP = _mm_setr_ps(SamplePosition + 0.0f * dSample,
                                                 SamplePosition + 1.0f * dSample,
                                                 SamplePosition + 2.0f * dSample,
                                                 SamplePosition + 3.0f * dSample); 
                    
                    __m128i SampleIndex = _mm_cvttps_epi32(SampleP);
                    __m128 tLerp = _mm_sub_ps(SampleP , _mm_cvtepi32_ps(SampleIndex));
                    
                    __m128 SampleValue0 = _mm_setr_ps(LoadedSound->Samples[0][((s32*)&SampleIndex)[0]],
                                                      LoadedSound->Samples[0][((s32*)&SampleIndex)[1]],
                                                      LoadedSound->Samples[0][((s32*)&SampleIndex)[2]],
                                                      LoadedSound->Samples[0][((s32*)&SampleIndex)[3]]);
                    
                    __m128 SampleValue1 = _mm_setr_ps(LoadedSound->Samples[0][((s32*)&SampleIndex)[0] + 1],
                                                      LoadedSound->Samples[0][((s32*)&SampleIndex)[1] + 1],
                                                      LoadedSound->Samples[0][((s32*)&SampleIndex)[2] + 1],
                                                      LoadedSound->Samples[0][((s32*)&SampleIndex)[3] + 1]);
                    
                    
                    __m128 LerpedSample = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(One, tLerp), SampleValue0),_mm_mul_ps(tLerp, SampleValue1));
                    
#else
                    
                    __m128 SampleValue = _mm_setr_ps(LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 0.0f * dSample)],
                                                     LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 1.0f * dSample)],
                                                     LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 2.0f * dSample)],
                                                     LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 3.0f * dSample)]);
                    
#endif
                    
                    //TODO: This could have floating point impresition
                    __m128 D0 = _mm_load_ps((float * )Dest0);
                    __m128 D1 = _mm_load_ps((float * )Dest1);
                    
                    D0 = _mm_add_ps(D0, _mm_mul_ps(_mm_mul_ps(TotalVolume0, CurrentVolume0), LerpedSample)); 
                    D1 = _mm_add_ps(D1, _mm_mul_ps(_mm_mul_ps(TotalVolume1, CurrentVolume1), LerpedSample));  
                    
                    _mm_store_ps((float * )Dest0,D0);
                    _mm_store_ps((float * )Dest1,D1);
                    
                    Dest0++;
                    Dest1++;
                    
                    CurrentVolume0 = _mm_add_ps(CurrentVolume0, dVolumePerChunk0);
                    CurrentVolume1 = _mm_add_ps(CurrentVolume1, dVolumePerChunk1);
                    
                    PlayingSound->CurrentVolume += dVolumePerChunk;
                }
                
                for(u32 ChannelIndex = 0;
                    ChannelIndex < AudioStateOutputChannelCount;
                    ++ChannelIndex)
                {
                    if(VolumeEndsAt[ChannelIndex] == ChunksToMix)
                    {
                        PlayingSound->CurrentVolume.E[ChannelIndex] = PlayingSound->TargetVolume.E[ChannelIndex];
                        PlayingSound->dCurrentVolume.E[ChannelIndex] = 0;
                    }
                }
                
                Assert(TotalChunksToMix >= ChunksToMix);
                TotalChunksToMix -= ChunksToMix;
                
                PlayingSound->Real32SamplesPlayed = EndSamplePosition;
                //(u32)PlayingSound->Real32SamplesPlayed >= LoadedSound->SampleCount
                if(ChunksToMix == LoadedSoundMissingChunks)
                {
                    if(IsValid(NextID))
                    {
                        PlayingSound->ID = NextID;
                        Assert(PlayingSound->Real32SamplesPlayed >= LoadedSound->SampleCount);
                        PlayingSound->Real32SamplesPlayed -= (r32)LoadedSound->SampleCount;
                        if(PlayingSound->Real32SamplesPlayed < 0.0f)
                        {
                            PlayingSound->Real32SamplesPlayed = 0.0f;
                        }
                    }
                    else
                    {
                        SoundFinished = true;
                    }
                }
                
            }
            else
            {
                LoadSound(Assets, PlayingSound->ID);
                break;
            }
        }
        
        if(SoundFinished)
        {
            *PlayingSoundPtr = PlayingSound->Next; 
            PlayingSound->Next = AudioState->FirstFreePlayingSound;
            AudioState->FirstFreePlayingSound = PlayingSound;
        }
        else
        {
            PlayingSoundPtr = &PlayingSound->Next; 
        }
        
    }
    
    //NOTE:  Pack Source into 16 bit form   
    {
        __m128 * Source0 = RealChannel0;
        __m128 * Source1 = RealChannel1;
        
        __m128i * SampleWritten = (__m128i *)SoundBuffer->Samples;
        for(u32 SampleIndex = 0; 
            SampleIndex < ChunkCount; 
            ++SampleIndex)
        {
            __m128 S0 = _mm_load_ps ((float *) Source0++);
            __m128 S1 = _mm_load_ps ((float *) Source1++);
            
            //NOTE:_mm_cvtps_epi32 rounds to nearest value  
            __m128i S032 = _mm_cvtps_epi32 (S0);
            __m128i S132 = _mm_cvtps_epi32 (S1);
            
            __m128i SI0 = _mm_unpacklo_epi32(S032, S132);
            __m128i SI1 = _mm_unpackhi_epi32(S032, S132);
            
            __m128i SourcePacked =  _mm_packs_epi32 (SI0, SI1);
            
            _mm_store_si128(SampleWritten++, SourcePacked);
        }
    }
    
    EndGenerationIndex(Assets, GenerationIndex);
    EndTempMemory(TempMemory);
}


internal void 
InitializeAudioState(audio_state * AudioState, memory_arena * PermArena)
{
    AudioState->PermArena = PermArena;
    AudioState->FirstPlayingSound = 0;
    AudioState->FirstFreePlayingSound = 0;
    
    AudioState->TotalVolume.E[0] = 1.0f;
    AudioState->TotalVolume.E[1] = 1.0f;
}








