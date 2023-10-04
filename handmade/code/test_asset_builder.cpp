#include "test_asset_builder.h"


#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;     /* File type, always 4D42h ("BM") */
    uint32 FileSize;     /* Size of the file in bytes */
    uint16 Reserved1;    /* Always 0 */
    uint16 Reserved2;    /* Always 0 */
    uint32 BitmapOffset; /* Starting position of image data in bytes */
    
    uint32 Size;            /* Size of this header in bytes */
    int32 Width;           /* Image width in pixels */
    int32 Height;          /* Image height in pixels */
    uint16 Planes;          /* Number of color planes */
    uint16 BitsPerPixel;    /* Number of bits per pixel */
    
    uint32 Compression;     /* Compression methods used */
    uint32 SizeOfBitmap;    /* Size of bitmap in bytes */
    int32  HorzResolution;  /* Horizontal resolution in pixels per meter */
    int32  VertResolution;  /* Vertical resolution in pixels per meter */
    uint32 ColorsUsed;      /* Number of colors in the image */
    uint32 ColorsImportant; /* Minimum number of important colors */
    
    uint32 RedMask;       /* Mask identifying bits of red component */
    uint32 GreenMask;     /* Mask identifying bits of green component */
    uint32 BlueMask;      /* Mask identifying bits of blue component */
    uint32 AlphaMask;     /* Mask identifying bits of alpha component */
};


//NOTE: Header file is stored in little endian order
struct WAVE_header 
{
    uint32 RIFFID;
    uint32 Size;
    uint32 WAVEID;
};

#define RIFF_CODE(a,b,c,d) ((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24)

enum WAVE_chunk_id
{
    WAVE_ChunkID_fmt = RIFF_CODE('f','m','t',' '),
    WAVE_ChunkID_data = RIFF_CODE('d','a','t','a'),
    WAVE_ChunkID_RIFF = RIFF_CODE('R','I','F','F'),
    WAVE_ChunkID_WAVE = RIFF_CODE('W','A','V','E')
};

struct WAVE_chunk 
{
    uint32 ID;
    uint32 Size;
};

struct WAVE_fmt
{
    uint16 wFormatTag;
    uint16 nChannels;
    uint32 nSamplesPerSec;
    uint32 nAvgBytesPerSec;
    uint16 nBlockAlign;
    uint16 wBitsPerSample;
    uint16 cbSize;
    uint16 wValidBitsPerSample;
    uint32 dwChannelMask;
    uint8 SubFormat[16];
};

#pragma pack(pop)


struct loaded_bitmap
{
    int32 Width;
    int32 Height;
    int32 Pitch;
    void * Memory;
    void * Free;
};

struct entire_file
{
    u32 ContentSize;
    void * Content;
    
};

entire_file
ReadEntireFile(char * FileName)
{
    entire_file Result = {};
    
    FILE * In = fopen(FileName, "rb");
    if(In)
    {
        fseek(In, 0, SEEK_END);
        Result.ContentSize = ftell(In);
        fseek(In, 0, SEEK_SET);
        
        Result.Content = malloc (Result.ContentSize);
        fread (Result.Content, Result.ContentSize, 1, In);
        fclose(In);
    }
    else
    {
        printf("ERROR Cannot open file %s.\n", FileName);
    }
    
    return Result;
}

internal loaded_bitmap   
LoadBMP(char * FileName)
{
    loaded_bitmap LoadedBitmap = {};
    
    entire_file EntireFile = ReadEntireFile(FileName);
    if(EntireFile.ContentSize != 0)
    {	
        LoadedBitmap.Free = EntireFile.Content;
        
        bitmap_header * BitMapHeader = (bitmap_header *)EntireFile.Content; 	
        uint32 * Pixels = (uint32*)((uint8 *)EntireFile.Content + BitMapHeader->BitmapOffset);
        LoadedBitmap.Memory = Pixels;
        LoadedBitmap.Width = BitMapHeader->Width;
        LoadedBitmap.Height = BitMapHeader->Height;
        
        Assert(LoadedBitmap.Height >= 0);
        Assert(BitMapHeader->Compression == 3);
        //NOTE: If you are using this generically, please remember that bmpp files cango 
        //on either direction and the height will be negative for top-down 
        //also there can be compression, etc, this isn't the complete bmp loading code.
        
        //NOTE: Byte order is determined by the headeritself, we have to look to the red, green, and blue mask.
        
        uint32 RedMask = BitMapHeader->RedMask;
        uint32 GreenMask = BitMapHeader->GreenMask;
        uint32 BlueMask = BitMapHeader->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);        
        
        
        bit_scan_result RedScan = FindLeastSignificandSetBit(RedMask);;
        bit_scan_result GreenScan = FindLeastSignificandSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificandSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificandSetBit(AlphaMask);
        
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);
        
        int32 RedShiftDown = (int32)RedScan.Index;
        int32 GreenShiftDown =  (int32)GreenScan.Index;
        int32 BlueShiftDown = (int32)BlueScan.Index;
        int32 AlphaShiftDown = (int32)AlphaScan.Index;
        
        uint32 * SourceDest = (uint32 *)LoadedBitmap.Memory;
        for(int32 Y = 0 ; Y < BitMapHeader->Height; ++Y)
        {
            for(int32 X = 0 ; X < BitMapHeader->Width; ++X)
            {
                uint32 C = *SourceDest;
                
                vector_4 Texel255 = {
                    (real32)((C & RedMask) >> RedShiftDown),
                    (real32)((C & GreenMask) >> GreenShiftDown),
                    (real32)((C & BlueMask) >> BlueShiftDown),
                    (real32)((C & AlphaMask) >> AlphaShiftDown)
                };
                
#define GAMA_CORRECT 1 
#if GAMA_CORRECT
                vector_4 Texel = SRGB255ToLinear1(Texel255);
#else
                vector_4 Texel = Texel255 * (1.0f / 255.0f);
#endif
                //NOTE: Premultiplied Alpha
                Texel.rgb *= Texel.a;
                
#if GAMA_CORRECT
                vector_4 PMTexel= Linear1ToSRGB255(Texel);
#else
                vector_4 PMTexel = 255.0f * Texel;
#endif
                
                *SourceDest++ = (((uint32)(PMTexel.a + 0.5f) << 24)
                                 | ((uint32)(PMTexel.r + 0.5f) << 16)
                                 | ((uint32)(PMTexel.g + 0.5f) << 8)
                                 | ((uint32)(PMTexel.b + 0.5f) << 0));
            }
        }
    }
    
    LoadedBitmap.Pitch = BITMAP_BYTES_PER_PIXEL * LoadedBitmap.Width;
    
#if 0
    LoadedBitmap.Memory = (uint8 *)LoadedBitmap.Memory + (LoadedBitmap.Pitch * (LoadedBitmap.Height - 1)); 
    LoadedBitmap.Pitch = -LoadedBitmap.Pitch;
#endif
    
    return LoadedBitmap;
}


global_variable HDC GlobalFontDeviceContext = 0;
global_variable VOID * GlobalFontMemory = 0;

//TODO: FreeDC()
internal void
InitializeFontDC(void)
{
    if(!GlobalFontDeviceContext)
    {
        GlobalFontDeviceContext = CreateCompatibleDC (GetDC(0));
        
        //NOTE: Bitmap is bottom-up
        BITMAPINFO Info = {};
        Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        Info.bmiHeader.biWidth = MAX_BITMAP_WIDTH;
        Info.bmiHeader.biHeight = MAX_BITMAP_HEIGHT;
        Info.bmiHeader.biPlanes = 1;
        Info.bmiHeader.biBitCount = 32;
        Info.bmiHeader.biCompression = BI_RGB;
        Info.bmiHeader.biSizeImage = 0;
        Info.bmiHeader.biXPelsPerMeter = 0;
        Info.bmiHeader.biYPelsPerMeter = 0;
        Info.bmiHeader.biClrUsed = 0;
        Info.bmiHeader.biClrImportant = 0;
        
        HBITMAP Bitmap = CreateDIBSection(GlobalFontDeviceContext, &Info, DIB_RGB_COLORS, &GlobalFontMemory, 0,0); 
        
        SelectObject (GlobalFontDeviceContext, Bitmap);
        //SetTextAlign (GlobalFontDeviceContext, GetTextAlign(GlobalFontDeviceContext) & (~TA_CENTER));
        SetBkColor(GlobalFontDeviceContext, RGB(0,0,0));
    }
}


internal loaded_font *
LoadFont(char * FileName, char * FontName, u32 Height)
{
    //NOTE: We can call this every time we want to add a new font 
    //NOTE: This adds the Font to the resources 
    loaded_font * Font = (loaded_font*)malloc(sizeof(loaded_font));
    
    int FontCount = AddFontResourceExA(FileName, FR_PRIVATE, 0); 
    Font->Win32Handle = CreateFontA(Height, 0, 0, 0, 
                                    FW_NORMAL, //NOTE: Weight
                                    FALSE, //NOTE: Italic
                                    FALSE, //NOTE: Underline
                                    FALSE, //StrikeOut
                                    DEFAULT_CHARSET, //Charset
                                    OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS,
                                    ANTIALIASED_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE,
                                    FontName);
    
    //NOTE: Selects font
    SelectObject (GlobalFontDeviceContext, Font->Win32Handle);
    GetTextMetricsW(GlobalFontDeviceContext, &Font->TextMetric);
    
    memory_index GlyphIndexFromCodePointSize = (sizeof(u32) * ONE_PAST_MAX_CODE_POINT);
    Font->GlyphIndexFromCodePoint = (u32*)malloc(GlyphIndexFromCodePointSize);
    memset(Font->GlyphIndexFromCodePoint, 0, GlyphIndexFromCodePointSize);
    
    Font->MaxGlyphCount = 5000;
    Font->FirstGLyph = U32Max;
    Font->LastGlyph = 0;
    
    memory_index GlyphSize = sizeof(hha_font_glyph) * Font->MaxGlyphCount;
    Font->Glyphs = (hha_font_glyph*)malloc(GlyphSize);
    memset(Font->Glyphs, 0, GlyphSize);
    
    memory_index KerningTableSize = (sizeof(r32) * Font->MaxGlyphCount * Font->MaxGlyphCount); 
    Font->KerningTable = (r32*)malloc(KerningTableSize);
    memset(Font->KerningTable, 0x00, KerningTableSize);
    
    //NOTE: Initialize Null Glyph
    Font->GlyphCount = 1;
    Font->Glyphs[0].UnicodeCodePoint = 0;
    Font->Glyphs[0].BitmapID.Value = 0;
    
    Font->OnePastLastCodePoint = 0;
    return Font;
}

internal void 
FinishKerningForFont(loaded_font * LoadedFont)
{
    SelectObject (GlobalFontDeviceContext, LoadedFont->Win32Handle);
    
    DWORD TotalKerningPairs = GetKerningPairsW(GlobalFontDeviceContext, 0, 0); 
    KERNINGPAIR * KerningPairs = (KERNINGPAIR *)malloc(sizeof(KERNINGPAIR) * TotalKerningPairs);
    TotalKerningPairs = GetKerningPairsW(GlobalFontDeviceContext, TotalKerningPairs, KerningPairs); 
    
    for(u32 KerningPairIndex = 0;
        KerningPairIndex < TotalKerningPairs;
        ++KerningPairIndex)
    {
        KERNINGPAIR * KerningPair = KerningPairs + KerningPairIndex;
        
        if(KerningPair->wFirst < ONE_PAST_MAX_CODE_POINT && 
           KerningPair->wSecond < ONE_PAST_MAX_CODE_POINT)
        {
            u32 FirstGlyphIndex = LoadedFont->GlyphIndexFromCodePoint[KerningPair->wFirst];
            u32 SecondGLyphIndex = LoadedFont->GlyphIndexFromCodePoint[KerningPair->wSecond];
            
            Assert(FirstGlyphIndex < LoadedFont->MaxGlyphCount);
            Assert(SecondGLyphIndex < LoadedFont->MaxGlyphCount);
            
            //NOTE: First and second glyph indices can be 0 since is a sparse table
            if(FirstGlyphIndex > 0 && 
               SecondGLyphIndex > 0)
            {
                LoadedFont->KerningTable[FirstGlyphIndex * LoadedFont->MaxGlyphCount + SecondGLyphIndex] += 
                    (r32)KerningPair->iKernAmount;
            }
        }
    }
    
    free(KerningPairs);
}

internal void 
FreeFont(loaded_font * LoadedFont)
{
    if(LoadedFont)
    {
        DeleteObject(LoadedFont->Win32Handle);
        DeleteDC(GlobalFontDeviceContext);
        
        free(LoadedFont->GlyphIndexFromCodePoint);
        free(LoadedFont->Glyphs);
        free(LoadedFont->KerningTable);
        free(LoadedFont);
    }
}


#define Make4ChannelFromMono(Source) (((Source) << 0) | ((Source) << 8)  | ((Source) << 16) | ((Source) << 24))

internal loaded_bitmap
LoadGlyphBitmap(loaded_font * LoadedFont, u32 CodePoint, hha_asset * HHA)
{
    loaded_bitmap Result = {};
    
    //NOTE: Here is about glyph indices you have added to the table so you 
    u32 GlyphIndex = LoadedFont->GlyphIndexFromCodePoint[CodePoint];
    Assert(GlyphIndex > 0 && GlyphIndex < LoadedFont->MaxGlyphCount);
    
#ifdef USING_WINDOWS_FONTS
    
    memset(GlobalFontMemory, 0x00, MAX_BITMAP_WIDTH * MAX_BITMAP_HEIGHT * BITMAP_BYTES_PER_PIXEL);
    
    SelectObject (GlobalFontDeviceContext, LoadedFont->Win32Handle);
    
    wchar_t CheeseCodepoint =  (wchar_t)(CodePoint);
    //NOTE: Size units are logical units
    SIZE Size = {};
    GetTextExtentPoint32W(GlobalFontDeviceContext, &CheeseCodepoint, 1, &Size);
    
    u32 PreStepX = 128;
    int Width = Size.cx + 2 * PreStepX;
    //NOTE: Clamping to BitmapSize
    if(Width > MAX_BITMAP_WIDTH)
    {
        Width = MAX_BITMAP_WIDTH;
    }
    int Height = Size.cy;
    if(Height > MAX_BITMAP_HEIGHT)
    {
        Height = MAX_BITMAP_HEIGHT;
    }
    
    //NOTE: CLears to zero the bitmap on GlobalFontDeviceContext 
    //PatBlt(GlobalFontDeviceContext, 0, 0, Width, Height, BLACKNESS);
    //SetBkMode(GlobalFontDeviceContext, TRANSPARENT);
    SetTextColor(GlobalFontDeviceContext, RGB(255,255,255));
    
    TextOutW(GlobalFontDeviceContext, PreStepX, 0, &CheeseCodepoint, 1); 
    //ExtTextOutW(GlobalFontDeviceContext, 0, 0, ETO_CLIPPED, 0, &CheeseCodepoint, 1, 0);  
    
    s32 MinWidth = 10000;
    s32 MinHeight = 10000;
    s32 MaxWidth = -10000;
    s32 MaxHeight = -10000;
    
    u32 * Row = (u32*)GlobalFontMemory + ((MAX_BITMAP_HEIGHT - 1) * MAX_BITMAP_WIDTH);
    for(s32 Y = 0; 
        Y < Height;
        ++Y)
    {
        
        u32 * Source = Row;
        for(s32 X = 0; 
            X < Width;
            ++X)
        {
            
#if 0
            //NOTE: This is really slow calling the OS, every time!
            COLORREF Pixel = GetPixel(GlobalFontDeviceContext, X, Y);  
            //u32 Value = (Pixel & 0xFF);
            Assert(Pixel == *Source);
#endif
            
            if(*Source != 0)
            {
                if(Y > MaxHeight)
                {
                    MaxHeight = Y;
                }
                
                if(X > MaxWidth)
                {
                    MaxWidth = X;
                }
                
                if(Y < MinHeight)
                {
                    MinHeight = Y;
                }
                
                if(X < MinWidth)
                {
                    MinWidth = X;
                }
            }
            
            ++Source;
        }
        
        Row -= MAX_BITMAP_WIDTH;
        
    }
    
    r32 KerningOffset = 0;
    
    if((MinWidth <= MaxWidth) && 
       (MinHeight <= MaxHeight))
    {
        //NOTE: Here we do the malloc 
        Result.Width = (MaxWidth - MinWidth) + 3;
        Result.Height = (MaxHeight - MinHeight) + 3;
        Result.Pitch = BITMAP_BYTES_PER_PIXEL * Result.Width; 
        Result.Memory = malloc(Result.Pitch * Result.Height);
        Result.Free = Result.Memory;
        
        memset(Result.Memory, 0x00, Result.Pitch * Result.Height);
        
        u8 * DestRow = (u8*)Result.Memory + Result.Pitch;//(Result.Pitch * (Result.Height - 2)));
        u32 * SourceRow = (u32*)GlobalFontMemory + ((MAX_BITMAP_HEIGHT - MaxHeight - 1) * MAX_BITMAP_WIDTH);
        
        for(s32 Y = MinHeight; 
            Y <= MaxHeight;
            ++Y)
        {
            u32 * Dest = (u32*)DestRow + 1;
            u32 * Source = (SourceRow + MinWidth);
            for(s32 X = MinWidth; 
                X <= MaxWidth;
                ++X)
            {
#if 0
                //NOTE: This is really slow calling the OS, every time!
                COLORREF Pixel = GetPixel(GlobalFontDeviceContext, X, Y);  
                Assert(Pixel == *Source);
#endif
                
#if 0
                u32 Value = Make4ChannelFromMono(*Source & 0xFF);
                *Dest++ = Value;
                ++Source;
#endif
                
                r32 Gray = (r32)(*Source & 0xFF);
                
                vector_4 Texel255 = {255.0f,255.0f,255.0f,Gray};
                
                //NOTE: Premultiplied Alpha
                vector_4 Texel = SRGB255ToLinear1(Texel255);
                Texel.rgb *= Texel.a;
                vector_4 PMTexel= Linear1ToSRGB255(Texel);
                
                *Dest++ = (((uint32)(PMTexel.a + 0.5f) << 24)
                           | ((uint32)(PMTexel.r + 0.5f) << 16)
                           | ((uint32)(PMTexel.g + 0.5f) << 8)
                           | ((uint32)(PMTexel.b + 0.5f) << 0));
                
                ++Source;
            }
            DestRow += Result.Pitch;
            SourceRow += MAX_BITMAP_WIDTH;
        }
        
        KerningOffset = (r32)MinWidth - (r32)PreStepX;
        
        HHA->Bitmap.AlignPorcentage[0] = (1.0f) / (r32)Result.Width; 
        HHA->Bitmap.AlignPorcentage[1] = (LoadedFont->TextMetric.tmDescent - (Height - MaxHeight) + 1.0f)  / Result.Height;
        
    }
    
    //NOTE: we expect here to have the  glyph indices table  already here 
#if 0
    ABC ThisABC;
    GetCharABCWidthsW(GlobalFontDeviceContext, CodePoint, CodePoint, &ThisABC);
    r32 CharacterWidth = r32(ThisABC.abcA + ThisABC.abcB + ThisABC.abcC); 
#else
    INT CharSize;
    GetCharWidth32W(GlobalFontDeviceContext, CodePoint, CodePoint, &CharSize); 
    r32 CharacterWidth = (r32)CharSize;
#endif
    
    for(u32 OtherGlyphIndex = 0;
        OtherGlyphIndex < LoadedFont->MaxGlyphCount;
        ++OtherGlyphIndex)
    {
        LoadedFont->KerningTable[GlyphIndex * LoadedFont->MaxGlyphCount + OtherGlyphIndex] += ((r32)CharacterWidth - KerningOffset);
        
        if(OtherGlyphIndex != 0)
        {
            LoadedFont->KerningTable[OtherGlyphIndex * LoadedFont->MaxGlyphCount + GlyphIndex] += (r32)KerningOffset;
        }
    }
    
#else
    //NOTE: Shawn's Font Rasterizer
    entire_file EntireFile = ReadEntireFile(FileName);
    if(EntireFile.ContentSize != 0)
    {	
        
        stbtt_fontinfo Font;
        stbtt_InitFont(&Font, (u8*)EntireFile.Content, stbtt_GetFontOffsetForIndex((u8*)EntireFile.Content,0));
        
        int Width;
        int Height;
        int XOff;
        int YOff;
        
        u8* MonoBitmap = stbtt_GetCodepointBitmap(&Font, 0, stbtt_ScaleForPixelHeight(&Font, 128.0f), 
                                                  CodePoint, &Width, &Height, &XOff, &YOff);
        
        Result.Width = Width;
        Result.Height = Height;
        Result.Pitch = BITMAP_BYTES_PER_PIXEL * Result.Width; 
        Result.Memory = malloc (Result.Pitch * Height);
        Result.Free = Result.Memory;
        
        u8 * DestRow = ((u8*)Result.Memory + (Result.Pitch * (Result.Height - 1)));
        u8 * Source = MonoBitmap;
        
        for(s32 Y = (Height - 1); 
            Y >= 0;
            --Y)
        {
            u32 * Dest = (u32*)DestRow;
            for(s32 X = 0; 
                X < Width;
                ++X)
            {
                u32 Value = Make4ChannelFromMono(*Source);
                *Dest++ = Value;
                ++Source;
            }
            
            DestRow -= Result.Pitch;
            
        }
        
        stbtt_FreeBitmap(MonoBitmap, 0);
        free(EntireFile.Content);
    }
#endif
    
    return(Result);
}


struct riff_iterator
{
    uint8 * At;
    uint8 * Stop;
};

inline riff_iterator
ParseChunkAt(void * At, void * Stop)
{
    riff_iterator Result = {};
    
    Result.At = (uint8*)(At);
    Result.Stop = (uint8 *)Stop;
    
    return Result;
}

inline bool32
IsValid(riff_iterator Iter)
{
    bool32 Result = (Iter.At < Iter.Stop); 
    return Result;
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
    Iter.At += ((WAVE_chunk *)Iter.At)->Size + sizeof(WAVE_chunk);
    return Iter;
}

inline uint32 
GetID(riff_iterator Iter)
{
    WAVE_chunk * Chunk  = (WAVE_chunk *)Iter.At;
    return Chunk->ID;
}

inline void *
GetChunkData(riff_iterator Iter)
{
    void * Data = Iter.At + sizeof(WAVE_chunk);
    return Data;
}

inline void 
SwapUint16s(int16 * FirstP, int16 * SecondP)
{
    Assert(FirstP && SecondP);
    
    int16 Buffer = *FirstP;
    *FirstP = *SecondP;
    *SecondP = Buffer;
    
}


struct loaded_sound
{
    uint32 SampleCount; 
    uint32 ChannelCount;
    int16 * Samples[2];
    void * Free;
};


internal loaded_sound 
LoadWAV(char * FileName, u32 SectionFirstSampleIndex , u32 SectionSampleCount)
{
    loaded_sound LoadedSound = {};
    
    entire_file EntireFile = ReadEntireFile(FileName);
    if(EntireFile.ContentSize != 0)
    {	
        LoadedSound.Free = EntireFile.Content;
        
        WAVE_header * Header = (WAVE_header *)EntireFile.Content; 	
        Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
        Assert(Header->WAVEID == WAVE_ChunkID_WAVE);
        
        WAVE_fmt * Format;
        
        int16 * SampleData = 0;
        uint32 ChannelCount = 0;
        uint32 SampleDataSize = 0;
        
        for(riff_iterator Iter = ParseChunkAt(Header + 1, (uint8*)(Header + 1) + Header->Size - 4);
            IsValid(Iter);
            Iter = NextChunk(Iter))
        {
            switch(GetID(Iter))
            {
                case WAVE_ChunkID_fmt:
                {
                    Format = (WAVE_fmt *)GetChunkData(Iter);
                    Assert(Format->wFormatTag == 1);
                    Assert(Format->nSamplesPerSec == 48000);
                    Assert(Format->wBitsPerSample == 16);
                    Assert(Format->nBlockAlign == (2 *  Format->nChannels));
                    ChannelCount = Format->nChannels;
                    
                }break;
                case WAVE_ChunkID_data:
                {
                    SampleData = (int16 *)GetChunkData(Iter);
                    WAVE_chunk * Chunk  = (WAVE_chunk *)Iter.At;
                    SampleDataSize = Chunk->Size;
                    
                    Chunk->Size = (Chunk->Size + 1) & ~1;
                }break;
            }
        }
        
        Assert(ChannelCount && SampleData);
        
        LoadedSound.ChannelCount = ChannelCount;
        //NOTE: SampleDataSize is in bytes 
        u32 SampleCount = SampleDataSize / (ChannelCount * sizeof(int16));
        
        
        if(LoadedSound.ChannelCount == 1)
        {
            LoadedSound.Samples[0] = SampleData;
            LoadedSound.Samples[1] = 0;
        }
        else if(LoadedSound.ChannelCount == 2)
        {
            LoadedSound.Samples[0] = SampleData;
            LoadedSound.Samples[1] = SampleData + SampleCount;
#if 0
            for(uint32 DataIndex = 1;
                DataIndex < (LoadedSound.SampleCount);
                ++DataIndex)
            {
                SampleData[2 * DataIndex + 0] = (int16)DataIndex;
                SampleData[2 * DataIndex + 1] = (int16)DataIndex;
            }
#endif
            
            for(uint32 DataIndex = 1;
                DataIndex < SampleCount;
                ++DataIndex)
            {
                int16 Buffer = SampleData[DataIndex];
                SampleData[DataIndex] = SampleData[2 * DataIndex];
                SampleData[2 * DataIndex] = Buffer;
            }
            
            int16 LastSample = (uint16)(SampleCount * 2);
            int16 EndSample = (uint16)(LastSample - CeilReal32ToInt32(SampleCount / 2.0f));
            
            for(int16 DataIndex = (LastSample - 2);
                DataIndex > EndSample;
                --DataIndex)
            {
                int16 Difference = (LastSample - 1) - DataIndex;
                
                int16 Buffer = SampleData[DataIndex];
                SampleData[DataIndex] = SampleData[DataIndex - Difference];
                SampleData[DataIndex - Difference] = Buffer;
            }
        }
        else
        {
            Assert(!"Invalid Channel Count in Wav File");
        }
        
        //TODO: Adjust this for multiple channels
        LoadedSound.ChannelCount = 1;
        bool32 AtEndOfSound = true;
        
        //TODO: Load right channels
        if(SectionSampleCount)
        {
            Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
            AtEndOfSound = ((SectionFirstSampleIndex + SectionSampleCount) == SampleCount);
            
            SampleCount = SectionSampleCount;
            
            for(uint32 ChannelIndex = 0; 
                ChannelIndex < LoadedSound.ChannelCount; 
                ++ChannelIndex)
            {
                LoadedSound.Samples[ChannelIndex] += SectionFirstSampleIndex;
            }
        }
        
#if 1
        if(AtEndOfSound)
        {
            for(uint32 ChannelIndex = 0; 
                ChannelIndex < LoadedSound.ChannelCount; 
                ++ChannelIndex)
            {
                for(u32 SampleIndex = SampleCount;
                    SampleIndex < (SampleCount + 4);
                    ++SampleIndex)
                {
                    LoadedSound.Samples[ChannelIndex][SampleIndex] = 0;
                }
            }
        }
#endif
        
        LoadedSound.SampleCount = SampleCount;
    }
    
    return LoadedSound; 
}


internal void 
BeginAssetType(game_assets * Assets, asset_type_id TypeID)
{
    Assert(Assets->DEBUGAssetType == 0);
    Assets->DEBUGAssetType = Assets->AssetTypes + TypeID;
    
    Assets->DEBUGAssetType->TypeID = TypeID;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

struct added_asset
{
    u32 Index;
    hha_asset * HHA;
    asset_source * Source;
};

internal added_asset
AddAsset(game_assets * Assets)
{
    Assert(Assets->DEBUGAssetType);
    added_asset Result = {};
    Result.Index = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    
    Result.Source = Assets->Sources + Result.Index;
    Result.HHA = Assets->Assets + Result.Index;
    Result.HHA->FirstTagIndex = Assets->TagCount;
    Result.HHA->OnePastLastTagIndex = Result.HHA->FirstTagIndex;
    
    Assets->AssetIndex = Result.Index;
    return Result;
}


internal bitmap_id 
AddBitmapAsset(game_assets * Assets, char * FileName, 
               r32 AlignPorcentageX = 0.5f, r32 AlignPorcentageY = 0.5f)
{
    added_asset AssetAdded = AddAsset(Assets);
    AssetAdded.HHA->Bitmap.AlignPorcentage[0] = AlignPorcentageX;
    AssetAdded.HHA->Bitmap.AlignPorcentage[1] = AlignPorcentageY;
    AssetAdded.Source->AssetType = AssetType_Bitmap;
    AssetAdded.Source->Bitmap.FileName = FileName;
    
    bitmap_id Result = {AssetAdded.Index};
    return Result;
}

internal font_id
AddFontAsset(game_assets * Assets, loaded_font * LoadedFont)
{
    added_asset AssetAdded = AddAsset(Assets);
    
    AssetAdded.HHA->Font.GlyphCount = LoadedFont->GlyphCount;
    AssetAdded.HHA->Font.ExternalLeading = (r32)LoadedFont->TextMetric.tmExternalLeading;
    AssetAdded.HHA->Font.AscenderHeight = (r32)LoadedFont->TextMetric.tmAscent;
    AssetAdded.HHA->Font.DescenderHeight = (r32)LoadedFont->TextMetric.tmDescent;
    AssetAdded.HHA->Font.OnePastLastCodePoint = LoadedFont->OnePastLastCodePoint;
    
    AssetAdded.Source->AssetType = AssetType_Font;
    AssetAdded.Source->Font.LoadedFont = LoadedFont;
    
    font_id Result = {AssetAdded.Index};
    return Result;
}

internal bitmap_id
AddCharacterAsset(game_assets * Assets, loaded_font * LoadedFont, u32 CodePoint)
{
    added_asset AssetAdded = AddAsset(Assets);
    
    AssetAdded.HHA->Bitmap.AlignPorcentage[0] = 0.0f; //NOTE: Set later by extraction
    AssetAdded.HHA->Bitmap.AlignPorcentage[1] = 0.0f; //NOTE: Set later by extraction
    AssetAdded.Source->AssetType = AssetType_FontGlyph;
    AssetAdded.Source->FontGlyph.LoadedFont = LoadedFont;
    AssetAdded.Source->FontGlyph.CodePoint = CodePoint;
    
    bitmap_id Result = {AssetAdded.Index};
    
    //NOTE: this is the sparse table of glyphs
    Assert(LoadedFont->GlyphCount > 0 && LoadedFont->GlyphCount < LoadedFont->MaxGlyphCount);
    u32 GlyphIndex = LoadedFont->GlyphCount++; 
    hha_font_glyph * Glyph = LoadedFont->Glyphs + GlyphIndex;
    Glyph->BitmapID = Result;
    Glyph->UnicodeCodePoint = CodePoint;
    
    LoadedFont->GlyphIndexFromCodePoint[CodePoint] = GlyphIndex;
    
    if(CodePoint >= LoadedFont->OnePastLastCodePoint)
    {
        LoadedFont->OnePastLastCodePoint = (CodePoint + 1);
    }
    
    return Result;
}


internal sound_id 
AddSoundAsset(game_assets * Assets, char * FileName, u32 FirstSampleIndex = 0, u32 SampleCount = 0, 
              hha_sound_chain Chain = HHASound_None)
{
    added_asset AssetAdded = AddAsset(Assets);
    
    //NOTE: If SampleCount = 0 We mean to load the whole sound
    AssetAdded.HHA->Sound.SampleCount = SampleCount;
    AssetAdded.HHA->Sound.Chain = Chain;
    AssetAdded.Source->AssetType = AssetType_Sound;
    AssetAdded.Source->Sound.FileName = FileName;
    AssetAdded.Source->Sound.FirstSampleIndex = FirstSampleIndex;
    
    sound_id Result = {AssetAdded.Index};
    return Result;
}

internal void 
AddTag(game_assets * Assets, asset_tag_id TagID, real32 Value)
{
    Assert(Assets->AssetIndex);
    
    hha_asset * HHA = Assets->Assets + Assets->AssetIndex;
    ++HHA->OnePastLastTagIndex;
    
    hha_tag * Tag = Assets->Tags + Assets->TagCount++;
    Tag->ID = TagID; 
    Tag->Value = Value;
}

internal void 
EndAssetType(game_assets * Assets)
{
    Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assert(Assets->DEBUGAssetType);
    Assets->DEBUGAssetType = 0;
    Assets->AssetIndex = 0;
}


inline bool32
IsValid(bitmap_id ID)
{
    bool32 Result = (ID.Value != 0);
    return Result;
}

inline bool32
IsValid(sound_id ID)
{
    bool32 Result = (ID.Value != 0);
    return Result;
}

inline void
InitializeAssets(game_assets * Assets)
{
    Assets->DEBUGAssetType = 0; 
    Assets->AssetIndex = 0; 
    Assets->TagCount = 1;
    Assets->AssetTypeCount = 0;
    Assets->AssetCount = 1;
}

internal void  
WriteHHA(game_assets * Assets, char * FileName)
{
    FILE * Out = fopen(FileName, "wb");
    if(Out)
    {
        hha_header Header = {};
        Header.MagicValue = HHA_MAGIC_VALUE;
        Header.Version = HHA_VERSION;
        Header.TagCount = Assets->TagCount;
        Header.AssetTypeCount = Asset_Count; //TODO: Do we really want to do this?
        Header.AssetCount = Assets->AssetCount;
        
        u32 TagsArraySize = Header.TagCount * sizeof(hha_tag); 
        u32 AssetTypesArraySize = Header.AssetTypeCount * sizeof(hha_asset_type); 
        u32 AssetsArraySize = Header.AssetCount * sizeof(hha_asset); 
        
        Header.TagOffset = sizeof(hha_header);
        Header.AssetTypeOffset = Header.TagOffset + TagsArraySize;;
        Header.AssetOffset = Header.AssetTypeOffset + AssetTypesArraySize;
        
        fwrite(&Header, sizeof(hha_header), 1, Out);
        fwrite(Assets->Tags, TagsArraySize, 1, Out);
        fwrite(Assets->AssetTypes, AssetTypesArraySize, 1, Out);
        
        fseek(Out, AssetsArraySize, SEEK_CUR);
        
        for(u32 AssetIndex = 1;
            AssetIndex < Header.AssetCount;
            ++AssetIndex)
        {
            //NOTE: First write actual assets onto disk
            asset_source * Source = Assets->Sources + AssetIndex;
            hha_asset * Dest = Assets->Assets + AssetIndex;
            
            Dest->DataOffset = ftell(Out);
            
            if(Source->AssetType == AssetType_Sound)
            {
                loaded_sound Sound = LoadWAV(Source->Sound.FileName, 
                                             Source->Sound.FirstSampleIndex, 
                                             Dest->Sound.SampleCount);
                
                Dest->Sound.SampleCount = Sound.SampleCount;
                Dest->Sound.ChannelCount = Sound.ChannelCount;
                
                for(u32 ChannelIndex = 0;
                    ChannelIndex < Sound.ChannelCount;
                    ++ChannelIndex)
                {
                    fwrite(Sound.Samples[ChannelIndex], (Dest->Sound.SampleCount * sizeof(s16)), 1, Out);
                }
                
                free(Sound.Free);
            }
            else if(Source->AssetType == AssetType_Font)
            {
                loaded_font * LoadedFont = Source->Font.LoadedFont;
                
                FinishKerningForFont(LoadedFont);
                
                u32 GlyphsSize = (LoadedFont->GlyphCount * sizeof(hha_font_glyph)); 
                fwrite(LoadedFont->Glyphs, GlyphsSize, 1, Out);
                
                //NOTE: Since the kerning table is initialized as MaxGlyphCount * MaxGlyphCount we need to write
                //it by rows 
                u32 RowSize = LoadedFont->GlyphCount * sizeof(r32);
                
                for(u32 KerningRowIndex = 0;
                    KerningRowIndex < LoadedFont->GlyphCount;
                    ++KerningRowIndex)
                {
                    r32 * RowPtr = LoadedFont->KerningTable + (KerningRowIndex * LoadedFont->MaxGlyphCount);
                    fwrite(RowPtr, RowSize, 1, Out);
                }
            }
            else
            {
                loaded_bitmap Bitmap = {};
                
                if(Source->AssetType == AssetType_FontGlyph)
                {
                    Bitmap = LoadGlyphBitmap(Source->FontGlyph.LoadedFont, Source->FontGlyph.CodePoint, Dest);
                }
                else
                {
                    Assert(Source->AssetType == AssetType_Bitmap);
                    Bitmap = LoadBMP(Source->Bitmap.FileName);
                }
                
                if(Bitmap.Memory)
                {
                    Dest->Bitmap.Dim[0] = Bitmap.Width; 
                    Dest->Bitmap.Dim[1] = Bitmap.Height; 
                    
                    Assert(Bitmap.Pitch == (Bitmap.Width * BITMAP_BYTES_PER_PIXEL));
                    fwrite(Bitmap.Memory, Bitmap.Pitch * Bitmap.Height, 1, Out);
                    free(Bitmap.Free);
                }
                else
                {
                    //NOTE: We have Null Bitmap for ' ' character
                    Assert(Source->FontGlyph.CodePoint == 32);
                }
            }
        }
        
        fseek (Out, (u32)Header.AssetOffset, SEEK_SET);
        fwrite(Assets->Assets, AssetsArraySize, 1, Out);
        
        fclose(Out);
    }
    else
    {
        //NOTE: Error opening file
    }
}

inline void
WriteHero(char * FileName)
{
    game_assets Assets_ = {};
    game_assets * Assets = &Assets_;
    InitializeAssets(Assets);
    
    real32 RightAngle = 0.0f;
    real32 BackAngle = Pi32 * 0.5f;
    real32 LeftAngle = Pi32;
    real32 FrontAngle = Pi32 * 1.5f;
    
    //NOTE: All these bitmaps have the next top-down alignment:
    BeginAssetType(Assets, Asset_Cape);
    AddBitmapAsset(Assets,"test/test_hero_right_cape.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, RightAngle);
    AddBitmapAsset(Assets,"test/test_hero_back_cape.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, BackAngle);
    AddBitmapAsset(Assets,"test/test_hero_left_cape.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, LeftAngle);
    AddBitmapAsset(Assets,"test/test_hero_front_cape.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, FrontAngle);
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Head);
    AddBitmapAsset(Assets,"test/test_hero_right_head.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, RightAngle);
    AddBitmapAsset(Assets,"test/test_hero_back_head.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, BackAngle);
    AddBitmapAsset(Assets,"test/test_hero_left_head.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, LeftAngle);
    AddBitmapAsset(Assets,"test/test_hero_front_head.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, FrontAngle);
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Torso);
    AddBitmapAsset(Assets,"test/test_hero_right_torso.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, RightAngle);
    AddBitmapAsset(Assets,"test/test_hero_back_torso.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, BackAngle);
    AddBitmapAsset(Assets,"test/test_hero_left_torso.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, LeftAngle);
    AddBitmapAsset(Assets,"test/test_hero_front_torso.bmp", 0.5f, 0.152073726f);
    AddTag(Assets, Tag_FacingDirection, FrontAngle);
    EndAssetType(Assets);
    
    WriteHHA(Assets, FileName);
}

internal void
AddGlyphsForFont(game_assets * Assets, loaded_font * Font)
{
    for(u32 CharacterIndex = ' ';
        CharacterIndex <= '~';
        ++CharacterIndex)
    {
        AddCharacterAsset(Assets, Font, CharacterIndex);
    }
    
    //NOTE: Kanji owl
    AddCharacterAsset(Assets, Font, 0x5C0F);
    AddCharacterAsset(Assets, Font, 0x8033);
    AddCharacterAsset(Assets, Font, 0x6728);
    AddCharacterAsset(Assets, Font, 0x514E);
}


inline void 
WriteFontAssets(char * FileName)
{
    game_assets Assets_ = {};
    game_assets * Assets = &Assets_;
    InitializeAssets(Assets);
    
    loaded_font * DefaultFont = LoadFont("C:/Windows/Fonts/arial.ttf", "Arial", 128);
    loaded_font * DebugFont = LoadFont("C:/Windows/Fonts/liberation-mono.ttf", "Liberation Mono", 15);
    
    BeginAssetType(Assets, Asset_FontGlyph);
    AddGlyphsForFont(Assets, DefaultFont);
    AddGlyphsForFont(Assets, DebugFont);
    EndAssetType(Assets);
    
    //TODO: This is janky, this has to follow this order first glyph writes then Font writes
    //is there other way to precatch kerning offsets and then write it to file?
    BeginAssetType(Assets, Asset_Font);
    AddFontAsset(Assets, DefaultFont);
    AddTag(Assets, Tag_FontType, (r32)FontType_Default);
    
    AddFontAsset(Assets, DebugFont);
    AddTag(Assets, Tag_FontType, (r32)FontType_Debug);
    EndAssetType(Assets);
    
    WriteHHA(Assets, FileName);
    
    FreeFont(DefaultFont);
    FreeFont(DebugFont);
}


inline void
WriteNonHero(char * FileName)
{
    game_assets Assets_ = {};
    game_assets * Assets = &Assets_;
    InitializeAssets(Assets);
    
    BeginAssetType(Assets, Asset_Stair);
    AddBitmapAsset(Assets, "BmpStairs.bmp");
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/test_hero_shadow.bmp", 0.5f, 0.152073726f);
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test2/tree00.bmp", 0.493827164f, 0.295652181f);
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test2/rock03.bmp", 0.5f, 0.65625f);
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Grass);
    AddBitmapAsset(Assets, "test2/grass00.bmp");
    AddBitmapAsset(Assets, "test2/grass01.bmp");
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Tuft);
    AddBitmapAsset(Assets, "test2/tuft00.bmp");
    AddBitmapAsset(Assets, "test2/tuft01.bmp");
    AddBitmapAsset(Assets, "test2/tuft02.bmp");
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Stone);
    AddBitmapAsset(Assets, "test2/ground00.bmp");
    AddBitmapAsset(Assets, "test2/ground01.bmp");
    AddBitmapAsset(Assets, "test2/ground02.bmp");
    AddBitmapAsset(Assets, "test2/ground03.bmp");
    EndAssetType(Assets);
    
    //TODO: Shall we add ranged values for asset tags 
    //u32 Ranges[][2] = {{'A','Z'} , {'a','z'}, {'0','9'}};
    WriteHHA(Assets, FileName);
}

inline void 
WriteSound(char * FileName)
{
    //
    //NOTE: Adding Sound 
    //
    game_assets Assets_ = {};
    game_assets * Assets = &Assets_;
    InitializeAssets(Assets);
    
    BeginAssetType(Assets, Asset_Bloop);
    AddSoundAsset(Assets,"test3/bloop_01.wav");
    AddSoundAsset(Assets,"test3/bloop_02.wav");
    AddSoundAsset(Assets,"test3/bloop_03.wav");
    AddSoundAsset(Assets,"test3/bloop_04.wav");
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Crack);
    AddSoundAsset(Assets, "test3/crack_00.wav");
    EndAssetType(Assets);
    
    u32 TotalMusicSampleCount = 7468095;
    u32 MusicChunkSize = 2 * 48000;
    
    BeginAssetType(Assets, Asset_Music);
    
    for(u32 FirstSampleIndex = 0;
        FirstSampleIndex < TotalMusicSampleCount;
        FirstSampleIndex += MusicChunkSize)
    {
        u32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
        if(SampleCount > MusicChunkSize)
        {
            SampleCount = MusicChunkSize;
        }
        
        sound_id CurrentID = AddSoundAsset(Assets, "test3/music_test.wav", FirstSampleIndex , SampleCount,
                                           ((FirstSampleIndex + MusicChunkSize) < TotalMusicSampleCount) ? HHASound_Advance : HHASound_None);
    }
    
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Puhp);
    AddSoundAsset(Assets, "test3/puhp_00.wav");
    AddSoundAsset(Assets, "test3/puhp_01.wav");
    EndAssetType(Assets);
    
#if 0
    //TODO: Fix this, know why does this sounds cant' be written on to disk!
    BeginAssetType(Assets, Asset_Glide);
    AddSoundAsset(Assets, "test3/glide_00.wav");
    EndAssetType(Assets);
    
    BeginAssetType(Assets, Asset_Drop);
    AddSoundAsset(Assets, "test3/drop_00.wav");
    EndAssetType(Assets);
#endif
    
    WriteHHA(Assets, FileName);
}


int 
main(int ArgCount, char ** Args)
{
    //NOTE: GLobalFontDc   
    InitializeFontDC();
    WriteNonHero("TestNonHero.hha");
    WriteFontAssets("TestFontAssets.hha");
    WriteHero("TestHero.hha");
    WriteSound("TestSound.hha");
    return 0;
}
