#define internal  
#include "handmade.h"


#define IACA 0
#if IACA
#include <iacaMarks.h>
#else
#define IACA_VC64_START
#define IACA_VC64_END 
#endif

#define IGNORED_TIMED_FUNCTION TIMED_FUNCTION
#define IGNORED_TIMED_BLOCK TIMED_BLOCK 

void 
DrawRectangleQuickly(loaded_bitmap * Buffer, vector_2 Origin , vector_2 XAxis, vector_2 YAxis, 
                     vector_4 Color, loaded_bitmap * Texture, real32 PixelsToMeters, 
                     rectangle_2i ClipRect, bool32 Even)
{
    IGNORED_TIMED_FUNCTION();
    
    //NOTE: premultiply color up front
    Color.rgb *= Color.a;
    
    real32 XAxisLength = Length(XAxis);
    real32 YAxisLength = Length(YAxis);
    
    //NOTE: ZScale could be a parameter if we want the user 
    //to modify the scale in z dimension that the normals appear to have
    real32 ZScale = 0.5f * (XAxisLength + YAxisLength);
    
    real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
    real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);
    
    rectangle_2i FillRect = InvertedInfinityRectangle2i();
    
    vector_2 P[4] = {Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis};
    
    for(uint32 PIndex = 0; 
        PIndex < ArrayCount(P);
        ++PIndex)
    {
        vector_2 TestP = P[PIndex];
        rectangle_2i MaxBound = 
        {
            FloorReal32ToInt32(TestP.x), 
            CeilReal32ToInt32(TestP.x) + 1,
            FloorReal32ToInt32(TestP.y),
            CeilReal32ToInt32(TestP.y) + 1
        }; 
        FillRect =  Union(FillRect, MaxBound);
    }
    
    FillRect = Intersect(FillRect, ClipRect);
    
    //NOTE: One Hyperthread will run through Even lines while the other on Odd ones.
    if((Even && (FillRect.Miny % 2)) || !(Even || (FillRect.Miny % 2)))
    {
        FillRect.Miny += 1;
    }
    
    __m128i StartClipMask = _mm_set1_epi8(-1);
    __m128i EndClipMask = _mm_set1_epi8(-1);
    
    __m128i StartClipMasks[] = 
    {
        _mm_slli_si128(StartClipMask, 0 * BITMAP_BYTES_PER_PIXEL),
        _mm_slli_si128(StartClipMask, 1 * BITMAP_BYTES_PER_PIXEL),
        _mm_slli_si128(StartClipMask, 2 * BITMAP_BYTES_PER_PIXEL),
        _mm_slli_si128(StartClipMask, 3 * BITMAP_BYTES_PER_PIXEL),
        
    };
    
    __m128i EndClipMasks[] = 
    {
        _mm_srli_si128(EndClipMask, 0 * BITMAP_BYTES_PER_PIXEL),
        _mm_srli_si128(EndClipMask, 3 * BITMAP_BYTES_PER_PIXEL),
        _mm_srli_si128(EndClipMask, 2 * BITMAP_BYTES_PER_PIXEL),
        _mm_srli_si128(EndClipMask, 1 * BITMAP_BYTES_PER_PIXEL),
        
    };
    if(FillRect.Minx & 3)
    {
        StartClipMask = StartClipMasks[FillRect.Minx & 3];
        FillRect.Minx = FillRect.Minx & -4;
    }
    
    if(FillRect.Maxx & 3)
    {
        EndClipMask = EndClipMasks[FillRect.Maxx & 3];
        FillRect.Maxx = ((FillRect.Maxx & -4) + 4);
    }
    
    int32 TexturePitch = Texture->Pitch;
    __m128 Inv255_4x = _mm_set1_ps(1 / 255.0f);
    __m128 One = _mm_set1_ps(1.0f);
    __m128 Half = _mm_set1_ps(0.5f);
    __m128 One255 = _mm_set1_ps(255.0f);
    __m128 Zero = _mm_set1_ps(0.0f);
    __m128 Four_4x = _mm_set1_ps(4.0f);
    
    //NOTE: Color values are in 0 - 1 space
    __m128 Colorr_4x = _mm_set1_ps(Color.r);
    __m128 Colorg_4x = _mm_set1_ps(Color.g);
    __m128 Colorb_4x = _mm_set1_ps(Color.b);
    __m128 Colora_4x = _mm_set1_ps(Color.a);
    
    __m128 NXAxisx = _mm_set1_ps(InvXAxisLengthSq * XAxis.x);
    __m128 NXAxisy = _mm_set1_ps(InvXAxisLengthSq * XAxis.y);
    __m128 NYAxisx = _mm_set1_ps(InvYAxisLengthSq * YAxis.x);
    __m128 NYAxisy = _mm_set1_ps(InvYAxisLengthSq * YAxis.y);
    
    __m128 Originx = _mm_set1_ps(Origin.x);
    __m128 Originy = _mm_set1_ps(Origin.y);
    
    __m128 TextureWidthM2 = _mm_set1_ps((real32)(Texture->Width - 2));
    __m128 TextureHeightM2 = _mm_set1_ps((real32)(Texture->Height - 2));
    __m128i TexturePitch_4x  = _mm_set1_epi32(Texture->Pitch);
    
    __m128i MaskByte = _mm_set1_epi32(0xFF);
    __m128i MaskWORD = _mm_set1_epi32(0x0000FFFF);
    __m128i Mask16Bit = _mm_set1_epi32(0x00FF00FF);
    
    uint8 * Row = ((uint8 *)Buffer->Memory + 
                   (FillRect.Minx* BITMAP_BYTES_PER_PIXEL) + (FillRect.Miny * Buffer->Pitch));
    
    
    int32 RowAdvance = 2 * Buffer->Pitch;
    void * TextureMemory = Texture->Memory;
    
    int32 Maxx = FillRect.Maxx;
    int32 Minx = FillRect.Minx;
    int32 Miny = FillRect.Miny;
    int32 Maxy = FillRect.Maxy;
    
    
    __m128 PixelPxI = _mm_set_ps((real32)(Minx + 3) - Origin.x, 
                                 (real32)(Minx + 2) - Origin.x,
                                 (real32)(Minx + 1) - Origin.x,
                                 (real32)(Minx + 0) - Origin.x);
    
    //TODO: Fix the other TIMED_BLOCK calls also!
    IGNORED_TIMED_BLOCK(PixelFillingDrawRQ, GetClampedRectArea(FillRect) / 2);
    
    for(int32 Y = Miny; 
        Y < Maxy; 
        Y += 2)
    {
        
        __m128i ClipMask = StartClipMask;
        
        __m128 PixelPy = _mm_set1_ps((real32)Y);
        PixelPy = _mm_sub_ps(PixelPy , Originy); 
        
        __m128 PixelPx = PixelPxI; 
        
        __m128 PyNX =  _mm_mul_ps(PixelPy , NXAxisy);
        __m128 PyNY =  _mm_mul_ps(PixelPy , NYAxisy);
        
        uint32 * Pixel = (uint32*)Row;
        for(int32 X = Minx; 
            X < Maxx; 
            X += 4)
        {
            
#define mmSquare(A) _mm_mul_ps(A, A) 
#define M(A, I) ((real32 *)&A)[I]
#define MI(A, I) ((int32 *)&A)[I]
            
            IACA_VC64_START;
            
            __m128 U = _mm_add_ps(_mm_mul_ps(PixelPx , NXAxisx) , PyNX); 
            __m128 V = _mm_add_ps(_mm_mul_ps(PixelPx , NYAxisx) , PyNY);
            
            //NOTE: Here we already know which pixels have to be written so we can clamp UV 
            //so we never fetch out of memory 
            __m128i WriteMask = _mm_castps_si128(_mm_and_ps(_mm_and_ps(_mm_cmpge_ps(U, Zero) , 
                                                                       _mm_cmple_ps(U, One)) , 
                                                            _mm_and_ps(_mm_cmpge_ps(V, Zero) , 
                                                                       _mm_cmple_ps(V, One))));
            
            WriteMask = _mm_and_si128(WriteMask, ClipMask); 
            
            //TODO:Check later if this helps
            //if(_mm_movemask_epi8 (WriteMask))
            {
                __m128i OriginalDest = _mm_load_si128((__m128i *)Pixel);
                U = _mm_min_ps(_mm_max_ps(U, Zero),One);
                V = _mm_min_ps(_mm_max_ps(V, Zero),One);
                
                __m128 XReal = _mm_add_ps(_mm_mul_ps(U, TextureWidthM2), Half);
                __m128 YReal = _mm_add_ps(_mm_mul_ps(V, TextureHeightM2), Half);
                
                __m128i FetchX_4x =  _mm_cvttps_epi32(XReal);
                __m128i FetchY_4x =  _mm_cvttps_epi32(YReal);
                
                //NOTE: tX and tY are fractional values 
                __m128 tX = _mm_sub_ps(XReal , _mm_cvtepi32_ps(FetchX_4x));
                __m128 tY = _mm_sub_ps(YReal , _mm_cvtepi32_ps(FetchY_4x));
                
                FetchX_4x = _mm_slli_epi32(FetchX_4x, 2);
                FetchY_4x = _mm_or_si128(_mm_mullo_epi16(FetchY_4x, TexturePitch_4x), 
                                         _mm_slli_epi32(_mm_mulhi_epi16(FetchY_4x, TexturePitch_4x), 16));
                __m128i Fetch_4x = _mm_add_epi32(FetchY_4x, FetchX_4x);
                
                //Pick neighboring texels to sample from
                uint32 Fetch0 = MI(Fetch_4x,0);
                uint32 Fetch1 = MI(Fetch_4x,1);
                uint32 Fetch2 = MI(Fetch_4x,2);
                uint32 Fetch3 = MI(Fetch_4x,3);
                
                uint8 * TexelPtr0 = (uint8 *)TextureMemory + Fetch0;
                uint8 * TexelPtr1 = (uint8 *)TextureMemory + Fetch1;
                uint8 * TexelPtr2 = (uint8 *)TextureMemory + Fetch2;
                uint8 * TexelPtr3 = (uint8 *)TextureMemory + Fetch3;
                
                //NOTE: Here samples are unrolled SampleA has 4 pixel
                __m128i SampleA = _mm_setr_epi32(*(uint32 *)(TexelPtr0),
                                                 *(uint32 *)(TexelPtr1),
                                                 *(uint32 *)(TexelPtr2),
                                                 *(uint32 *)(TexelPtr3));
                
                __m128i SampleB = _mm_setr_epi32(*(uint32 *)(TexelPtr0 + BITMAP_BYTES_PER_PIXEL),
                                                 *(uint32 *)(TexelPtr1 + BITMAP_BYTES_PER_PIXEL),
                                                 *(uint32 *)(TexelPtr2 + BITMAP_BYTES_PER_PIXEL),
                                                 *(uint32 *)(TexelPtr3 + BITMAP_BYTES_PER_PIXEL));
                __m128i SampleC = _mm_setr_epi32(*(uint32 *)(TexelPtr0 + TexturePitch),
                                                 *(uint32 *)(TexelPtr1 + TexturePitch),
                                                 *(uint32 *)(TexelPtr2 + TexturePitch),
                                                 *(uint32 *)(TexelPtr3 + TexturePitch));
                __m128i SampleD = _mm_setr_epi32(*(uint32 *)(TexelPtr0 + TexturePitch + BITMAP_BYTES_PER_PIXEL),
                                                 *(uint32 *)(TexelPtr1 + TexturePitch + BITMAP_BYTES_PER_PIXEL),
                                                 *(uint32 *)(TexelPtr2 + TexturePitch + BITMAP_BYTES_PER_PIXEL),
                                                 *(uint32 *)(TexelPtr3 + TexturePitch + BITMAP_BYTES_PER_PIXEL));
                
                
                //NOTE: Unpcack bilinear samples
                __m128i TexelArb = _mm_and_si128(SampleA, Mask16Bit);
                __m128i TexelAag = _mm_and_si128(_mm_srli_epi32(SampleA, 8), Mask16Bit);
                __m128 TexelAa =  _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleA, 24), MaskByte));  
                TexelArb = _mm_mullo_epi16 (TexelArb, TexelArb);
                TexelAag = _mm_mullo_epi16 (TexelAag, TexelAag);
                
                __m128i TexelBrb = _mm_and_si128(SampleB, Mask16Bit);
                __m128i TexelBag = _mm_and_si128(_mm_srli_epi32(SampleB, 8), Mask16Bit);
                __m128 TexelBa =  _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleB, 24), MaskByte));  
                TexelBrb = _mm_mullo_epi16 (TexelBrb, TexelBrb);
                TexelBag = _mm_mullo_epi16 (TexelBag, TexelBag);
                
                __m128i TexelCrb = _mm_and_si128(SampleC, Mask16Bit);
                __m128i TexelCag = _mm_and_si128(_mm_srli_epi32(SampleC, 8), Mask16Bit);
                __m128 TexelCa =  _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleC, 24), MaskByte));  
                TexelCrb = _mm_mullo_epi16 (TexelCrb, TexelCrb);
                TexelCag = _mm_mullo_epi16 (TexelCag, TexelCag);
                
                __m128i TexelDrb = _mm_and_si128(SampleD, Mask16Bit);
                __m128i TexelDag = _mm_and_si128(_mm_srli_epi32(SampleD, 8), Mask16Bit);
                __m128 TexelDa =  _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleD, 24), MaskByte));  
                TexelDrb = _mm_mullo_epi16 (TexelDrb, TexelDrb);
                TexelDag = _mm_mullo_epi16 (TexelDag, TexelDag);
                
                __m128 Destb = _mm_cvtepi32_ps(_mm_and_si128(OriginalDest, MaskByte));
                __m128 Destg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 8), MaskByte));
                __m128 Destr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 16), MaskByte));
                __m128 Desta = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 24), MaskByte));
                
                //NOTE: SRGB 0 - 255 to linear 0 - 1 space. Gama Correct 
                //NOTE:Alpha values are kept i 0 - 255 space 
                //R, G, B values are changed to 0 - Square(255.0f); 
                __m128 TexelAb = _mm_cvtepi32_ps(_mm_and_si128(TexelArb, MaskWORD));
                __m128 TexelAr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelArb, 16));
                __m128 TexelAg = _mm_cvtepi32_ps(_mm_and_si128(TexelAag,MaskWORD));
                
                __m128 TexelBb = _mm_cvtepi32_ps(_mm_and_si128(TexelBrb, MaskWORD));
                __m128 TexelBr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelBrb, 16));
                __m128 TexelBg = _mm_cvtepi32_ps(_mm_and_si128(TexelBag, MaskWORD));
                
                __m128 TexelCb = _mm_cvtepi32_ps(_mm_and_si128(TexelCrb, MaskWORD));
                __m128 TexelCr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelCrb, 16));
                __m128 TexelCg = _mm_cvtepi32_ps(_mm_and_si128(TexelCag, MaskWORD));
                
                __m128 TexelDb = _mm_cvtepi32_ps(_mm_and_si128(TexelDrb, MaskWORD));
                __m128 TexelDr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelDrb, 16));
                __m128 TexelDg = _mm_cvtepi32_ps(_mm_and_si128(TexelDag, MaskWORD));
                
                //NOTE: Bilinear texture blend
                __m128 itY = _mm_sub_ps(One, tY);
                __m128 itX = _mm_sub_ps(One, tX);
                
                __m128 C1 =  _mm_mul_ps(itY , itX);
                __m128 C2 =  _mm_mul_ps(itY , tX);
                __m128 C3 =  _mm_mul_ps(tY , itX);
                __m128 C4 =  _mm_mul_ps(tY , tX);
                
                __m128 Texelr = _mm_add_ps(_mm_add_ps(_mm_mul_ps(C1 , TexelAr) ,_mm_mul_ps(C2 , TexelBr)) ,_mm_add_ps(_mm_mul_ps(C3 , TexelCr) , _mm_mul_ps(C4 , TexelDr)));
                __m128 Texelg = _mm_add_ps(_mm_add_ps(_mm_mul_ps(C1 , TexelAg) ,_mm_mul_ps(C2 , TexelBg)) ,_mm_add_ps(_mm_mul_ps(C3 , TexelCg) , _mm_mul_ps(C4 , TexelDg)));
                __m128 Texelb = _mm_add_ps(_mm_add_ps(_mm_mul_ps(C1 , TexelAb) ,_mm_mul_ps(C2 , TexelBb)) ,_mm_add_ps(_mm_mul_ps(C3 , TexelCb) , _mm_mul_ps(C4 , TexelDb)));
                __m128 Texela = _mm_add_ps(_mm_add_ps(_mm_mul_ps(C1 , TexelAa) ,_mm_mul_ps(C2 , TexelBa)) ,_mm_add_ps(_mm_mul_ps(C3 , TexelCa) , _mm_mul_ps(C4 , TexelDa)));
                
                //NOTE: Modulate by incoming color
                Texelr = _mm_mul_ps(Texelr , Colorr_4x); 
                Texelg = _mm_mul_ps(Texelg , Colorg_4x); 
                Texelb = _mm_mul_ps(Texelb , Colorb_4x); 
                Texela = _mm_mul_ps(Texela , Colora_4x); 
                
                //NOTE: Clamp Texel values between zero and maxColorValue
                __m128 MaxColorValue = _mm_set1_ps(255.0f * 255.0f);
                Texelr =_mm_min_ps(_mm_max_ps(Texelr, Zero),MaxColorValue);
                Texelg =_mm_min_ps(_mm_max_ps(Texelg, Zero),MaxColorValue);
                Texelb =_mm_min_ps(_mm_max_ps(Texelb, Zero),MaxColorValue);
                
                //NOTE: SRGB 0 - 255 to linear 0 - 1 space for destination pixel 
                Destr = mmSquare(Destr);
                Destg = mmSquare(Destg);
                Destb = mmSquare(Destb);
                
                //NOTE: Blend from Source to Dest 
                __m128 InvTexelA = _mm_sub_ps(One , _mm_mul_ps(Texela, Inv255_4x));
                __m128 Blendedr = _mm_add_ps(_mm_mul_ps(InvTexelA , Destr) , Texelr);
                __m128 Blendedg = _mm_add_ps(_mm_mul_ps(InvTexelA , Destg) , Texelg);
                __m128 Blendedb = _mm_add_ps(_mm_mul_ps(InvTexelA , Destb) , Texelb);
                __m128 Blendeda = _mm_add_ps(_mm_mul_ps(InvTexelA , Desta) , Texela);
                
                //NOTE: linear 0 - 1 to SRGB space 0 - 255 
#if 1
                Blendedr = _mm_sqrt_ps(Blendedr);
                Blendedg = _mm_sqrt_ps(Blendedg);
                Blendedb = _mm_sqrt_ps(Blendedb);
#else
                Blendedr = _mm_mul_ps(_mm_rsqrt_ps(Blendedr),Blendedr);
                Blendedg = _mm_mul_ps(_mm_rsqrt_ps(Blendedg),Blendedg);
                Blendedb = _mm_mul_ps(_mm_rsqrt_ps(Blendedb),Blendedb);
#endif
                //TODO: Should we change to rounding to the nearest integer?
                __m128i IntR = _mm_cvtps_epi32(Blendedr);
                __m128i IntG = _mm_cvtps_epi32(Blendedg);
                __m128i IntB = _mm_cvtps_epi32(Blendedb);
                __m128i IntA = _mm_cvtps_epi32(Blendeda);
                
                //NOTE: Pack RGBA Pixel values into 8bit boundaries
                //NOTE: Shift left logical immediate
                
                IntR = _mm_slli_epi32(IntR,16);
                IntG = _mm_slli_epi32(IntG, 8);
                IntB = IntB;
                IntA = _mm_slli_epi32(IntA,24);
                
                __m128i NewValue = _mm_or_si128(_mm_or_si128 (IntR, IntG),_mm_or_si128(IntB, IntA));
                
                //NOTE: Mask out the pixels that does'nt have to be filled from new value 
                //and or them with the non filled pixels from  originalDest 
                __m128i MaskedOut = _mm_or_si128(_mm_and_si128(WriteMask, NewValue),_mm_andnot_si128 (WriteMask, OriginalDest));
                //TODO: Make address alignment to 128 - bits
                _mm_store_si128((__m128i *)Pixel, MaskedOut);
            }
            
            PixelPx  = _mm_add_ps(PixelPx, Four_4x);
            Pixel += 4;
            
            if((X + 8) < Maxx)
            {
                ClipMask = _mm_set1_epi8(-1);
            }
            else
            {
                ClipMask = EndClipMask;
            }
            
            IACA_VC64_END;
        }
        Row += RowAdvance;
    }
}


