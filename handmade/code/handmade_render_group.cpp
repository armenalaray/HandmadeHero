
#define IGNORED_TIMED_FUNCTION TIMED_FUNCTION

inline vector_4
UnscaleAndBiasNormal(vector_4 Normal)
{
    vector_4 Result = {};
    
    real32 Inv255 = 1 / 255.0f;
    
    Result.x = 2.0f * (Inv255 * Normal.x) - 1;
    Result.y = 2.0f * (Inv255 * Normal.y) - 1;
    Result.z = 2.0f * (Inv255 * Normal.z) - 1;
    Result.w = Inv255 * Normal.w;
    
    return Result;
}

internal void 
DrawRectangle(loaded_bitmap * Buffer, vector_2 vMin , vector_2 vMax, vector_4 Color, 
              rectangle_2i ClipRect, bool32 Even)
{
    //TODO: Premultiplied alpha
    TIMED_FUNCTION();
    
    real32 A = Color.a;
    real32 R = Color.r;
    real32 G = Color.g;
    real32 B = Color.b;
    
    //NOTE:The rightest most pixel is not written 
    rectangle_2i FillRect = {};
    FillRect.Minx = RoundReal32ToInt32(vMin.x);
    FillRect.Miny = RoundReal32ToInt32(vMin.y);
    FillRect.Maxx = RoundReal32ToInt32(vMax.x);
    FillRect.Maxy = RoundReal32ToInt32(vMax.y);
    
    FillRect = Intersect(FillRect, ClipRect);
    
    //NOTE: One Hyperthread will run through Even lines while the other on Odd ones.
    if((Even && (FillRect.Miny % 2)) || !(Even || (FillRect.Miny % 2)))
    {
        FillRect.Miny += 1;
    }
    
    //NOTE: R-G-B are porcentages from the color to be represented - goes from 0 to 1
    //Change to bit pattern 0x 00RRGGBB
    uint32 Color32 = ((RoundReal32ToUInt32(A * 255.0f) << 24) | 
                      (RoundReal32ToUInt32(R * 255.0f) << 16) |
                      (RoundReal32ToUInt32(G * 255.0f) << 8) |
                      (RoundReal32ToUInt32(B * 255.0f) << 0));
    
    uint8 * Row = ((uint8 *)Buffer->Memory + 
                   (FillRect.Minx * BITMAP_BYTES_PER_PIXEL) + (FillRect.Miny * Buffer->Pitch));
    int32 RowAdvance = (2 * Buffer->Pitch);
    
    for(int Y = FillRect.Miny; 
        Y < FillRect.Maxy; 
        Y += 2)
    {		
        uint32 * Pixel = (uint32*)Row;
        for(int X = FillRect.Minx; 
            X < FillRect.Maxx; 
            ++X)
        {
            *Pixel++ = Color32;		
        }
        Row += RowAdvance;
    }
}

inline bool32
EdgeTest(vector_2 PRel, vector_2 Perp)
{
    bool32 Result = (InnerProduct(PRel, Perp) >= 0) ? true : false;
    return Result;
}

inline vector_4
UnpackUint32ToVector4(uint32 Packed)
{
    vector_4 Result = {
        (real32)((Packed >> 16) & 0xFF),
        (real32)((Packed >> 8) & 0xFF), 
        (real32)((Packed >> 0) & 0xFF),
        (real32)((Packed >> 24) & 0xFF)
    };
    return Result;
}

struct bilinear_sample
{
    uint32 A, B, C, D;
};

inline bilinear_sample
BilinearSample(loaded_bitmap * Texture, int32 X, int32 Y)
{
    IGNORED_TIMED_FUNCTION();
    
    bilinear_sample Sample = {};
    uint8 * TexelPtr = (uint8 *)Texture->Memory + Y * Texture->Pitch + 
        X * BITMAP_BYTES_PER_PIXEL;
    Sample.A = *(uint32 *)(TexelPtr);
    Sample.B = *(uint32 *)(TexelPtr + BITMAP_BYTES_PER_PIXEL);
    Sample.C = *(uint32 *)(TexelPtr + Texture->Pitch);
    Sample.D = *(uint32 *)(TexelPtr + Texture->Pitch + BITMAP_BYTES_PER_PIXEL);
    
    return Sample;
}


inline vector_4
SRGBBilinearBlend(bilinear_sample TextureSample, real32 tX, real32 tY)
{
    IGNORED_TIMED_FUNCTION();
    
    vector_4 TexelA255 = UnpackUint32ToVector4(TextureSample.A);
    vector_4 TexelB255 = UnpackUint32ToVector4(TextureSample.B);
    vector_4 TexelC255 = UnpackUint32ToVector4(TextureSample.C);
    vector_4 TexelD255 = UnpackUint32ToVector4(TextureSample.D);
    
    //NOTE: Go from SRGB to linear brightness space
    //Gama correct
    vector_4 TexelA = SRGB255ToLinear1(TexelA255);
    vector_4 TexelB = SRGB255ToLinear1(TexelB255);
    vector_4 TexelC = SRGB255ToLinear1(TexelC255);
    vector_4 TexelD = SRGB255ToLinear1(TexelD255);
    
    //NOTE: Bilinear filtering
    vector_4 Result = Lerp(Lerp(TexelA, tX , TexelB) , tY, Lerp(TexelC, tX, TexelD));
    return Result;
}

inline vector_3
SampleEnvironmentMap(vector_2 ScreenSpaceUV, vector_3 SamplingDirection, real32 Roughness,
                     environment_map * Map, real32 DistanceFromMapInZ)
{
    IGNORED_TIMED_FUNCTION();
    /*NOTE: ScreenSpaceUV tells us where does the ray is being cast from in normalized screen coordinates  
    .       SamplingDirection tells us the direction of the bounced ray.
    .       Is not expected to be normalized but y has to be positive.
    .       We choose the LOD based on the roughness going from the highest detailed LOD to the lowest.
    .       Map Is the map we are sampling  from.
    */
    vector_3 Result = {};
    
    //NOTE: Pich LOD Map based on roughness
    uint32 LODIndex = (uint32)(Roughness * (ArrayCount(Map->LOD) - 1) + 0.5f);
    Assert(LODIndex < ArrayCount(Map->LOD));
    
    loaded_bitmap * LOD = Map->LOD + LODIndex;
    
    //NOTE: Compute distance to Map and Scale DistanceFromMapInZ to UVs
    real32 MetersToUVs = 0.1f;//TODO: Parameterize this and V should be a different value 
    
    //NOTE: Check signs for DistanceFromMapInZ and SamplingDirection.y
    real32 EnvsDistanceRatio = (MetersToUVs * DistanceFromMapInZ) / SamplingDirection.y; 
    vector_2 Offset = EnvsDistanceRatio * Vector2(SamplingDirection.x, SamplingDirection.z);
    
    //NOTE: Find intersection point within map
    vector_2 UV = Offset + ScreenSpaceUV;
    
    //NOTE: Clamp UVs to appropiate mapping
    UV.x = Clamp01(UV.x);
    UV.y = Clamp01(UV.y);
    
    //NOTE: BilinearSample
    real32 XReal = (UV.x * (real32)(LOD->Width - 2));
    real32 YReal = (UV.y * (real32)(LOD->Height - 2));
    
    int32 X = (int32)XReal; 
    int32 Y = (int32)YReal; 
    
    real32 tX = XReal - (real32)X;
    real32 tY = YReal - (real32)Y;
    
    Assert(X >= 0 && X < LOD->Width);
    Assert(Y >= 0 && Y < LOD->Height);
    
    //DEBUG_SWITCH(SampleEnvironmentMap)
    {
        //NOTE(Alex): Tur this one to know where you're sampling from environment map
        uint8 * TexelPtr = (uint8 *)LOD->Memory + Y * LOD->Pitch + X * BITMAP_BYTES_PER_PIXEL;
        uint32 * Pixel = (uint32 *)TexelPtr;
        *Pixel = 0xFFFFFFFF;
    }
    
    bilinear_sample Sample = BilinearSample(LOD, X, Y);
    Result = SRGBBilinearBlend(Sample, tX, tY).xyz;
    
    return Result; 
}


//#pragma optimize("", on)
internal void
DrawRectangleSlowly(loaded_bitmap * Buffer, vector_2 Origin , vector_2 XAxis, vector_2 YAxis, 
                    vector_4 Color, loaded_bitmap * Texture, loaded_bitmap * Normalmap, 
                    environment_map * Top, environment_map * Middle, environment_map *  Bottom, 
                    real32 PixelsToMeters)
{
    IGNORED_TIMED_FUNCTION();
    
    //NOTE: premultiply color up front
    Color.rgb *= Color.a;
    
    real32 XAxisLength = Length(XAxis);
    real32 YAxisLength = Length(YAxis);
    
    vector_2 NXAxis = YAxisLength / XAxisLength * XAxis; 
    vector_2 NYAxis = XAxisLength / YAxisLength * YAxis; 
    
    //NOTE: ZScale could be a parameter if we want the user 
    //to modify the scale in z dimension that the normals appear to have
    real32 ZScale = 0.5f * (XAxisLength + YAxisLength);
    
    real32 InvXAxisLengthSq = 1 / LengthSq(XAxis);
    real32 InvYAxisLengthSq = 1 / LengthSq(YAxis);
    
    uint32 Color32 = ((RoundReal32ToUInt32(Color.a * 255.0f) << 24) | 
                      (RoundReal32ToUInt32(Color.r * 255.0f) << 16) |
                      (RoundReal32ToUInt32(Color.g * 255.0f) << 8) |
                      (RoundReal32ToUInt32(Color.b * 255.0f) << 0));
    
    int32 WidthMax = (Buffer->Width - 1); 
    int32 HeightMax =(Buffer->Height - 1); 
    
    real32 InvWidthMax = 1.0f / (real32)WidthMax;
    real32 InvHeightMax = 1.0f / (real32)HeightMax;
    
    //NOTE: OriginZ Has to be offseted from base position
    real32 OriginZ = 0.0f;//-Origin.y * PixelsToMeters;
    //real32 ZOffset = Origin.y * PixelsToMeters - OriginZ;
    real32 OriginY = (Origin + 0.5f * XAxis + 0.5f * YAxis).y;
    real32 FixedCastY = InvHeightMax * OriginY;
    
    int Miny = HeightMax;
    int Maxy = 0;
    int Minx = WidthMax;
    int Maxx = 0;
    
    vector_2 P[4] = {Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis};
    
    for(uint32 PIndex = 0; 
        PIndex < ArrayCount(P);
        ++PIndex)
    {
        vector_2 TestP = P[PIndex];
        int32 FloorX = FloorReal32ToInt32(TestP.x);
        int32 CeilX = CeilReal32ToInt32(TestP.x);
        int32 FloorY = FloorReal32ToInt32(TestP.y);
        int32 CeilY = CeilReal32ToInt32(TestP.y);
        
        if(Miny > FloorY){ Miny = FloorY;} 
        if(Maxy < CeilY){ Maxy = CeilY;} 
        if(Minx > FloorX){ Minx = FloorX;} 
        if(Maxx < CeilX){ Maxx = CeilX;} 
    }
    
    if(Miny < 0){Miny = 0;}
    if(Maxy > HeightMax){Maxy = HeightMax;}
    if(Minx < 0){Minx = 0;}
    if(Maxx >  WidthMax){Maxx = WidthMax;}
    
    uint8 * Row = ((uint8 *)Buffer->Memory + 
                   (Minx* BITMAP_BYTES_PER_PIXEL) + (Miny * Buffer->Pitch));
    
    TIMED_BLOCK(PixelFillingDrawRS, (Maxy - Miny + 1) * (Maxx - Minx + 1));
    for(int32 Y = Miny; 
        Y <= Maxy; 
        ++Y)
    {		
        uint32 * Pixel = (uint32*)Row;
        for(int32 X = Minx; 
            X <= Maxx; 
            ++X)
        {
            
#if 1
            vector_2 PixelP = {(real32)X, (real32)Y};
            
            vector_2 d = PixelP - Origin;
            //TODO: PerpInner
            //TODO: Simpler Origin
            real32 Edge0 = InnerProduct(d, -Perp(XAxis));
            real32 Edge1 = InnerProduct(d - XAxis, -Perp(YAxis));
            real32 Edge2 = InnerProduct(d - XAxis -  YAxis, Perp(XAxis));
            real32 Edge3 = InnerProduct(d - YAxis, Perp(YAxis));
            
            if((Edge0 < 0) && 
               (Edge1 < 0) && 
               (Edge2 < 0) && 
               (Edge3 < 0))
            {
                //NOTE: We need to add an offset in y based on z world coordinate so 
                //that our cast is made properly
                
#if 1
                vector_2 ScreenSpaceUV = {(real32)X * InvWidthMax, FixedCastY};
                real32 ZDiff = PixelsToMeters * ((real32)Y - OriginY);
#else
                vector_2 ScreenSpaceUV = {(real32)X * InvWidthMax, (real32)Y * InvHeightMax};
                real32 ZDiff = 0.0f;
#endif
                real32 U = InvXAxisLengthSq * InnerProduct(d, XAxis); 
                real32 V = InvYAxisLengthSq * InnerProduct(d, YAxis);
                
                //TODO: SSE Clampling
                //U = Clamp01(U);
                //V = Clamp01(V);
                
                real32 XReal = (U * (real32)(Texture->Width - 2));
                real32 YReal = (V * (real32)(Texture->Height - 2));
                
                int32 XTex = (int32)XReal; 
                int32 YTex = (int32)YReal; 
                
                real32 Fx = XReal - (real32)XTex;
                real32 Fy = YReal - (real32)YTex;
                
                Assert(XTex >= 0 && XTex < Texture->Width);
                Assert(YTex >= 0 && YTex < Texture->Height);
                
                bilinear_sample TextureSample = BilinearSample(Texture, XTex, YTex);
                
                vector_4 Texel = SRGBBilinearBlend(TextureSample, Fx, Fy);
                
#if 0
                if(Normalmap)
                {
                    bilinear_sample NormalSample = BilinearSample(Normalmap, XTex, YTex);
                    
                    //TODO: Pack glossiness onto the alpha channel for the normal map
                    vector_4 NormalA = UnpackUint32ToVector4(NormalSample.A);
                    vector_4 NormalB = UnpackUint32ToVector4(NormalSample.B);
                    vector_4 NormalC = UnpackUint32ToVector4(NormalSample.C);
                    vector_4 NormalD = UnpackUint32ToVector4(NormalSample.D);
                    
                    //NOTE:Bilinear filtering
                    vector_4 Normal = Lerp(Lerp(NormalA, Fx , NormalB) , Fy, Lerp(NormalC, Fx, NormalD));
                    
                    Normal = UnscaleAndBiasNormal(Normal);
                    
                    //TODO: Rotate normals based on X/Y axis
                    Normal.xy = Normal.x * NXAxis + Normal.y * NYAxis;
                    Normal.z = ZScale;
                    
                    Normal.xyz = Normalize(Normal.xyz); 
                    
                    //NOTE:e = EyeDirection = [0,0,1]
                    //NOTE: this is a simplification of -e + 2.0f *  e^T * N * N;
                    vector_3 BounceDirection = 2.0f * Normal.xyz * Normal.z;
                    BounceDirection.z -= 1;
                    
                    BounceDirection.z = -BounceDirection.z;
                    
                    environment_map * Facingmap = 0; 
                    real32 tFacingMap = 0.0f;
                    real32 tEnvMap = BounceDirection.y;
                    
                    //NOTE: This will move the distance  in Z we are sampling from!
                    real32 Pz = ZDiff + OriginZ;
                    
                    //NOTE: Compute tFacingMap to set how much to lerp from the middle map
                    if(tEnvMap < -0.5f)
                    {
                        Facingmap = Bottom;
                        tFacingMap = -2.0f * (tEnvMap + 0.5f);
                        //tFacingMap = -tEnvMap;
                    }
                    else if(tEnvMap >= 0.5f)
                    {
                        Facingmap = Top;
                        tFacingMap = 2.0f * (tEnvMap - 0.5f); 
                        //tFacingMap = tEnvMap;
                    }
                    
                    tFacingMap *= tFacingMap;
                    tFacingMap *= tFacingMap;
                    
                    vector_3 MiddleLightColor = {0,0,0};//TODO: How do we sample from the middle map 
                    if(Facingmap)
                    {
                        real32 DistanceFromMapInZ = Facingmap->Pz - Pz;
                        vector_3 FacingLightColor = SampleEnvironmentMap(ScreenSpaceUV, BounceDirection, 
                                                                         Normal.w, Facingmap, DistanceFromMapInZ); 
                        MiddleLightColor = Lerp(MiddleLightColor, tFacingMap, FacingLightColor);
                    }
                    
                    //TODO: Do some lighting computation model here
                    Texel.rgb = Texel.rgb +  Texel.a * MiddleLightColor; 
                    
#if 0
                    Texel.rgb = Vector3(0.5f,0.5f,0.5f) + 0.5f * BounceDirection;
                    Texel.rgb *= Texel.a;
#endif
                }
#endif
                
                Texel = Hadamard(Texel, Color); 
                Texel.r = Clamp01(Texel.r);
                Texel.g = Clamp01(Texel.g);
                Texel.b = Clamp01(Texel.b);
                
                vector_4 Dest255 = {
                    (real32)((*Pixel >> 16) & 0xFF),
                    (real32)((*Pixel >> 8) & 0xFF),
                    (real32)((*Pixel >> 0) & 0xFF),
                    (real32)((*Pixel >> 24) & 0xFF)
                };
                
                vector_4 Dest = SRGB255ToLinear1(Dest255);
                
                vector_4 Blended = (1.0f - Texel.a) * Dest + Texel;
                
                vector_4 Blended255 = Linear1ToSRGB255(Blended);
                
                *Pixel = (((uint32)(Blended255.a) << 24)
                          | ((uint32)(Blended255.r + 0.5f) << 16)
                          | ((uint32)(Blended255.g + 0.5f) << 8)
                          | ((uint32)(Blended255.b + 0.5f) << 0));
                
            }
#else
            *Pixel = Color32;
#endif
            ++Pixel;
            
        }
        Row += Buffer->Pitch;
    }
}
//#pragma optimize("", off)

//#pragma optimize("", on)

//#pragma optimize("", off)



internal void 
ChangeSaturation(loaded_bitmap * Buffer, real32 Level)
{
    IGNORED_TIMED_FUNCTION();
    
    //TODO:Source row needs to be changed based on  clipping
    uint8 * DestRow = (uint8 * )Buffer->Memory;
    
    for(int32 Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32* Dest = (uint32*)DestRow;
        for(int32 X = 0; X < Buffer->Width; ++X)
        {
            vector_4 D = {
                (real32)((*Dest >> 16) & 0xFF),
                (real32)((*Dest >> 8) & 0xFF),
                (real32)((*Dest >> 0) & 0xFF),
                (real32)((*Dest >> 24) & 0xFF)
            };
            
            D = SRGB255ToLinear1(D);
            
            real32 Avg = (1.0f / 3.0f) * (D.r +  D.g + D.b);
            vector_3 Deltas = {D.r - Avg, D.g - Avg, D.b - Avg};
            vector_4 Result = Vector4(Vector3(Avg, Avg, Avg) + Deltas * Level, D.a);
            
            Result = Linear1ToSRGB255(Result);
            
            *Dest = (((uint32)(Result.a + 0.5f) << 24)
                     | ((uint32)(Result.r + 0.5f) << 16)
                     | ((uint32)(Result.g + 0.5f) << 8)
                     | ((uint32)(Result.b + 0.5f) << 0));
            
            ++Dest;
        }
        
        DestRow += Buffer->Pitch;
    }
}

//#pragma optimize("", on)
internal void 
DrawBitmap(loaded_bitmap * Buffer, loaded_bitmap * Bitmap, real32 RealX, 
           real32 RealY, real32 CAlpha = 1.0f)
{
    IGNORED_TIMED_FUNCTION();
    //NOTE:The rightest most pixel is not written 	
    int32 MinX = RoundReal32ToInt32(RealX);
    int32 MinY = RoundReal32ToInt32(RealY);
    int32 MaxX = MinX + Bitmap->Width;
    int32 MaxY = MinY + Bitmap->Height;
    
    int32 SourceOffsetX = 0;
    
    if(MinX < 0)
    {
        SourceOffsetX = -MinX;
        MinX  = 0; 
    }
    
    int32 SourceOffsetY = 0;
    
    if(MinY < 0)
    {
        SourceOffsetY = -MinY;
        MinY = 0;
    }
    
    if(MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }	
    
    if(MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }	
    
    //TODO:Source row needs to be changed based on  clipping
    uint8 * SourceRow =(uint8*)Bitmap->Memory  +  SourceOffsetY * Bitmap->Pitch + BITMAP_BYTES_PER_PIXEL * SourceOffsetX;
    uint8 * DestRow = ((uint8 * )Buffer->Memory + (MinY*Buffer->Pitch) + (MinX * BITMAP_BYTES_PER_PIXEL));
    
    for(int32 Y = MinY; Y < MaxY; ++Y)
    {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = (uint32 *)SourceRow;
        for(int32 X = MinX; X < MaxX; ++X)
        {
            //Lerp - linear blending mode
            vector_4 Texel255 = {
                (real32)((*Source >> 16) & 0xFF),
                (real32)((*Source >> 8) & 0xFF),
                (real32)((*Source >> 0) & 0xFF),
                (real32)((*Source >> 24) & 0xFF)
            };
            
            vector_4 Texel = SRGB255ToLinear1(Texel255);
            
            Texel *= CAlpha;
            
            vector_4 D = {
                (real32)((*Dest >> 16) & 0xFF),
                (real32)((*Dest >> 8) & 0xFF),
                (real32)((*Dest >> 0) & 0xFF),
                (real32)((*Dest >> 24) & 0xFF)
            };
            
            D = SRGB255ToLinear1(D);
            
            vector_4 Result = (1.0f - Texel.a) * D + Texel;
            
            Result = Linear1ToSRGB255(Result);
            
            *Dest = (((uint32)(Result.a + 0.5f) << 24)
                     | ((uint32)(Result.r + 0.5f) << 16)
                     | ((uint32)(Result.g + 0.5f) << 8)
                     | ((uint32)(Result.b + 0.5f) << 0));
            
            ++Dest;
            ++Source;
        }
        
        DestRow += Buffer->Pitch;
        SourceRow += Bitmap->Pitch;
    }
}

//#pragma optimize("", off)

internal void 
DrawMatte(loaded_bitmap * Bitmap, loaded_bitmap * Buffer, real32 RealX, 
          real32 RealY, real32 CAlpha = 1.0f)
{
    IGNORED_TIMED_FUNCTION();
    
    //NOTE:The rightest most pixel is not written 	
    int32 MinX = RoundReal32ToInt32(RealX);
    int32 MinY = RoundReal32ToInt32(RealY);
    int32 MaxX = MinX + Bitmap->Width;
    int32 MaxY = MinY + Bitmap->Height;
    
    int32 SourceOffsetX = 0;
    
    if(MinX < 0)
    {
        SourceOffsetX = -MinX;
        MinX  = 0; 
    }
    
    int32 SourceOffsetY = 0;
    
    if(MinY < 0)
    {
        SourceOffsetY = -MinY;
        MinY = 0;
    }
    
    if(MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }	
    
    if(MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }	
    
    //TODO:Source row needs to be changed based on  clipping
    uint8 * SourceRow =(uint8*)Bitmap->Memory  +  SourceOffsetY * Bitmap->Pitch + BITMAP_BYTES_PER_PIXEL * SourceOffsetX;
    uint8 * DestRow = ((uint8 * )Buffer->Memory + (MinY*Buffer->Pitch) + (MinX * BITMAP_BYTES_PER_PIXEL));
    
    for(int32 Y = MinY; Y < MaxY; ++Y)
    {
        uint32* Dest = (uint32*)DestRow;
        uint32* Source = (uint32 *)SourceRow;
        for(int32 X = MinX; X < MaxX; ++X)
        {
            //Lerp - linear blending mode
            real32 ASource = (real32)((*Source >> 24) & 0xFF);
            real32 ASourceR = ASource  / 255.0f * CAlpha ;
            real32 RSource = CAlpha * (real32)((*Source >> 16) & 0xFF);
            real32 GSource = CAlpha * (real32)((*Source >> 8) & 0xFF);
            real32 BSource = CAlpha *  (real32)((*Source >> 0) & 0xFF);
            
            real32 ADest = (real32)((*Dest >> 24) & 0xFF);
            real32 RDest = (real32)((*Dest >> 16) & 0xFF);
            real32 GDest = (real32)((*Dest >> 8) & 0xFF);
            real32 BDest = (real32)((*Dest >> 0) & 0xFF);
            real32 ADestR = ADest / 255.0f;
            
            real32 OneMinusASR = (1.0f - ASourceR);
            //TODO:Check this out for math errors
            //real32 ALerp = 255.0f * (ADestR + ASourceR - (ADestR * ASourceR));
            real32 ALerp = OneMinusASR * ADest;
            real32 RLerp = OneMinusASR * RDest;  
            real32 GLerp = OneMinusASR * GDest;  
            real32 BLerp = OneMinusASR * BDest;  
            
            *Dest = (((uint32)(ALerp + 0.5f) << 24)
                     | ((uint32)(RLerp + 0.5f) << 16)
                     | ((uint32)(GLerp + 0.5f) << 8)
                     | ((uint32)(BLerp + 0.5f) << 0));
            
            ++Dest;
            ++Source;
        }
        
        DestRow += Buffer->Pitch;
        SourceRow += Bitmap->Pitch;
    }
}


internal void
RenderGroupToOutput(render_group * RenderGroup, loaded_bitmap * OutputTarget, 
                    rectangle_2i ClipRect, bool32 Even)
{
    //TODO: Make Z ordering of RenderGroups
    //NOTE: This has to go from 0 to DebugCycleCounter_Count;
    IGNORED_TIMED_FUNCTION();
    
    real32 NullPixelsToMeters = 1.0f;
    
    for(uint32 BaseAddress = 0; 
        BaseAddress < RenderGroup->PushBufferSize; 
        )
    {
        render_group_entry_header * Header = (render_group_entry_header *)
            (RenderGroup->PushBufferBase + BaseAddress);
        
        BaseAddress += sizeof(*Header);
        render_group_entry_header * Data = (Header + 1);
        
        switch(Header->Type)
        {
            case RenderGroupEntryType_render_entry_clear:
            {
                render_entry_clear * Entry = (render_entry_clear *)(Data);
                
                DrawRectangle(OutputTarget, Vector2(0.0f,0.0f) , 
                              Vector2((real32)OutputTarget->Width, (real32)OutputTarget->Height), 
                              Entry->Color, ClipRect, Even);
                
                BaseAddress += sizeof(*Entry);
                
            }break;
            
            case RenderGroupEntryType_render_entry_bitmap:
            {
                render_entry_bitmap * Entry = (render_entry_bitmap *)(Data);
                Assert(Entry->Bitmap);
                
                vector_2 XAxis = {1, 0};
                vector_2 YAxis = Perp(XAxis);
                
                DrawRectangleQuickly(OutputTarget, Entry->Position, 
                                     Entry->Size.x * XAxis,
                                     Entry->Size.y * YAxis, 
                                     Entry->Color, Entry->Bitmap, NullPixelsToMeters, ClipRect, Even);
                
                BaseAddress += sizeof(*Entry);
                
            }break;
            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle * Entry = (render_entry_rectangle *)Data;
                DrawRectangle(OutputTarget, Entry->Position, Entry->Position + Entry->Dimension, 
                              Entry->Color, ClipRect, Even);				
                
                BaseAddress += sizeof(*Entry);
                
            }break;
            case RenderGroupEntryType_render_entry_coordinate_system:
            {
                render_entry_coordinate_system * Entry = (render_entry_coordinate_system *)Data;
                
                vector_2 Dim = {2,2};
                
                DrawRectangleSlowly(OutputTarget, Entry->Origin, Entry->XAxis , Entry->YAxis, 
                                    Entry->Color, Entry->Texture, Entry->Normalmap,
                                    Entry->Top , Entry->Middle, Entry->Bottom, 
                                    NullPixelsToMeters);
                
                vector_4 Color = {1.0f,1.0f,0.0f,1.0f};
                
                vector_2 Position =  Entry->Origin;
                DrawRectangle(OutputTarget, Position - Dim, Position + Dim, Color, ClipRect, Even);				
                
                Position =  Entry->Origin + Entry->XAxis;
                DrawRectangle(OutputTarget, Position - Dim, Position + Dim, Color, ClipRect, Even);				
                
                Position =  Entry->Origin + Entry->YAxis;
                DrawRectangle(OutputTarget, Position - Dim, Position + Dim, Color, ClipRect, Even);				
                
                Position =  Entry->Origin + Entry->YAxis + Entry->XAxis;
                DrawRectangle(OutputTarget, Position - Dim, Position + Dim, Color, ClipRect, Even);			
                BaseAddress += sizeof(*Entry);
                
            }break;
            //NOTE: This is used when we want to always define a valid case 
            InvalidDefaultCase;
        }
    }
}

struct tiled_render_work
{
    rectangle_2i ClipRect;
    
    render_group * RenderGroup;
    loaded_bitmap * OutputTarget;
    
};

internal PLATFORM_WORK_QUEUE_CALLBACK(DoTiledRenderWork)
{
    TIMED_FUNCTION();
    
    tiled_render_work * Work  = (tiled_render_work *)Data;
    Assert(Work->RenderGroup->InsideRenderer);
    
    RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, true);
    RenderGroupToOutput(Work->RenderGroup, Work->OutputTarget, Work->ClipRect, false);
}

internal void
RenderGroupToOutput(render_group * RenderGroup, loaded_bitmap * OutputTarget)
{
    //NOTE: This is called by the ground chunk composition system 
    TIMED_FUNCTION();
    Assert(RenderGroup->InsideRenderer);
    /*
    TODO: 
    -    Make sure tiles are all - cache aligned 
    -    Can we get hyperthreads synced so they do interleaved lines?
    -    How big should the tiles be for performance? 
    -    Actually BallPark the memory bandwith for our DrawRectangleQuickly
    -    Re-test some of our instruction choices
    
    */
    Assert(((uintptr)OutputTarget->Memory & 15) == 0);
    Assert(((OutputTarget->Height * OutputTarget->Pitch) & 15) == 0);
    
    rectangle_2i ClipRect = {};
    ClipRect.Minx = 0;
    ClipRect.Maxx = OutputTarget->Width;
    ClipRect.Miny = 0;
    ClipRect.Maxy = OutputTarget->Height;
    
    tiled_render_work Work;
    Work.ClipRect = ClipRect;
    Work.RenderGroup = RenderGroup;
    Work.OutputTarget = OutputTarget;
    
    DoTiledRenderWork(0, &Work);
}

internal void
TiledRenderGroupToOutput(platform_work_queue * RenderQueue, 
                         render_group * RenderGroup, loaded_bitmap * OutputTarget)
{
    TIMED_FUNCTION();
    Assert(RenderGroup->InsideRenderer);
    /*
    TODO: 
    -    Make sure tiles are all - cache aligned 
    -    Can we get hyperthreads synced so they do interleaved lines?
    -    How big should the tiles be for performance? 
    -    Actually BallPark the memory bandwith for our DrawRectangleQuickly
    -    Re-test some of our instruction choices
    
    */
    
    uint32 const TileCountY = 6;
    uint32 const TileCountX = 6;
    
    tiled_render_work RenderWork[TileCountX * TileCountY];
    
    Assert(((uintptr)OutputTarget->Memory & 15) == 0);
    Assert(((OutputTarget->Height * OutputTarget->Pitch) & 15) == 0);
    
    //TODO: Make sure that alllocator allocates enough space to round these up
    //TODO:Round to 4?
    int32 TileWidth = OutputTarget->Width / TileCountX;
    int32 TileHeight = OutputTarget->Height / TileCountY;
    
    TileWidth = ((TileWidth + 3) / 4) * 4;
    
    uint32 WorkIndex = 0;
    for(uint32 TileY = 0;
        TileY < TileCountY; 
        ++TileY)
    {
        for(uint32 TileX = 0;
            TileX < TileCountX; 
            ++TileX)
        {
            tiled_render_work * Work =  RenderWork + WorkIndex++;
            rectangle_2i ClipRect = {};
            ClipRect.Minx = TileX * TileWidth;
            ClipRect.Maxx = (TileX + 1) * TileWidth;
            ClipRect.Miny = TileY * TileHeight;
            ClipRect.Maxy = (TileY + 1) * TileHeight;
            
            if(TileX == (TileCountX - 1))
            {
                ClipRect.Maxx = OutputTarget->Width;
            }
            if(TileY == (TileCountY - 1))
            {
                ClipRect.Maxy = OutputTarget->Height;
            }
            
            Work->ClipRect = ClipRect;
            Work->RenderGroup = RenderGroup;
            Work->OutputTarget = OutputTarget;
            Platform.AddEntry(RenderQueue, DoTiledRenderWork, Work);
        }
    }
    //NOTE: Finish all thread work rendering before continuing
    Platform.CompleteAllWork(RenderQueue);
}




internal render_group * 
AllocateRenderGroup(game_assets * Assets, memory_arena * Arena, uint32 MaxPushBufferSize, 
                    b32 IsBackgroundThread)
{
    IGNORED_TIMED_FUNCTION();
    BeginOperationLock(Assets);
    render_group * Result = PushStruct(Arena, render_group);
    Result->PushBufferBase = (uint8 *)PushSize(Arena, MaxPushBufferSize - sizeof(render_group));
    EndOperationLock(Assets);
    
    Result->MaxPushBufferSize = MaxPushBufferSize;
    Result->PushBufferSize = 0;
    Result->GlobalAlpha = 1.0f;
    Result->Assets = Assets;
    
    Result->IsBackgroundThread = IsBackgroundThread;
    Result->InsideRenderer = false;
    
    Result->Transform.OffsetP = Vector3(0,0,0);
    Result->Transform.Scale = 1.0f;
    Result->MissingResourceCount = 0;
    return Result;
}

internal void 
BeginRenderGroup(render_group * RenderGroup)
{
    Assert(RenderGroup->InsideRenderer == false);
    RenderGroup->GenerationIndex = BeginGenerationIndex(RenderGroup->Assets);
    RenderGroup->InsideRenderer = true;
}


internal void
EndRenderGroup(render_group * RenderGroup)
{
    if(RenderGroup)
    {
        Assert(RenderGroup->InsideRenderer == true);
        RenderGroup->InsideRenderer = false;
        
        EndGenerationIndex(RenderGroup->Assets, RenderGroup->GenerationIndex);
        RenderGroup->GenerationIndex = 0;
        RenderGroup->PushBufferSize = 0;
    }
}

inline void 
Perspective(render_group * RenderGroup, int32 PixelWidth, int32 PixelHeight, real32 MetersToPixels, 
            real32 CameraToMonitorZ, real32 CameraToTargetZ)
{
    
    Assert(RenderGroup->InsideRenderer);
    
    real32 PixelsToMeters  = SafeRatio1(1.0f , MetersToPixels);
    RenderGroup->MonitorHalfDimInMeters = PixelsToMeters * 0.5f * Vector2i(PixelWidth , PixelHeight);
    
    //NOTE: Default Transform initialization
    RenderGroup->Transform.MetersToPixels = MetersToPixels;
    RenderGroup->Transform.ScreenCenter = 0.5f * Vector2i(PixelWidth , PixelHeight);
    
    RenderGroup->Transform.CameraToMonitorZ = CameraToMonitorZ;
    RenderGroup->Transform.CameraToTargetZ = CameraToTargetZ;
    
    RenderGroup->Transform.OffsetP = Vector3(0,0,0);
    RenderGroup->Transform.Scale = 1.0f;
    
    RenderGroup->Transform.Perspective = true; 
}

inline void 
Ortographic(render_group * RenderGroup, s32 PixelWidth, s32 PixelHeight, r32 MetersToPixels)
{
    Assert(RenderGroup->InsideRenderer);
    
    real32 PixelsToMeters  = SafeRatio1(1.0f , MetersToPixels);
    RenderGroup->MonitorHalfDimInMeters = PixelsToMeters * 0.5f * Vector2i(PixelWidth , PixelHeight);
    
    //NOTE: Default Transform initialization
    RenderGroup->Transform.MetersToPixels = MetersToPixels;
    RenderGroup->Transform.ScreenCenter = 0.5f * Vector2i(PixelWidth , PixelHeight);
    
    RenderGroup->Transform.CameraToMonitorZ = 1;
    RenderGroup->Transform.CameraToTargetZ = 1;
    
    RenderGroup->Transform.OffsetP = Vector3(0,0,0);
    RenderGroup->Transform.Scale = 1.0f;
    
    RenderGroup->Transform.Perspective = false; 
}



inline render_entity_basis_result 
GetRenderEntityBasis(render_transform * Transform, vector_3 OriginalP)
{
    IGNORED_TIMED_FUNCTION();
    //TODO: Figure out how xy displacement should work out
    render_entity_basis_result Result = {};
    real32 NearClipPlane = 0.1f;
    vector_3 TransformedP = Vector3(OriginalP.xy, 0) + Transform->OffsetP;
    
    if(Transform->Perspective)
    {
        //real32 CameraToTargetZ = Transform->CameraToTargetZ; 
        real32 OffsetZ = 0.0f; 
        
        //if(DEBUG_SWITCH(EnableDebugCamera))
        {
            Transform->CameraToTargetZ = 15.0f;//DEBUG_SWITCH(DebugCameraDistance);
        }
        
        real32 RelPZ = (Transform->CameraToTargetZ - TransformedP.z);
        vector_2 TotalXY = TransformedP.xy;
        
        if(RelPZ > NearClipPlane)
        {
            real32 ProjectedRatio = (Transform->CameraToMonitorZ / RelPZ); 
            vector_2 ProjectedXY = (TotalXY * ProjectedRatio);
            Result.Scale = Transform->MetersToPixels * ProjectedRatio;
            vector_2 Position  = Transform->ScreenCenter + Transform->MetersToPixels * ProjectedXY + 
                Vector2(0,Result.Scale * OffsetZ) ;
            Result.Position = Position;
            Result.IsDrawable = true;
        }
    }
    else
    {
        Result.Scale = Transform->MetersToPixels;
        Result.Position = Transform->ScreenCenter + Transform->MetersToPixels * TransformedP.xy;
        Result.IsDrawable = true;
    }
    
    return Result;
}


//NOTE(Alex): This is in PixelSpace
inline vector_3 
Unproject(render_transform * Transform, vector_2 ProjectedP, r32 WorldPZ)
{
    IGNORED_TIMED_FUNCTION();
    vector_3 Result = {};
    
    r32 PixelsToMeters = (1.0f / Transform->MetersToPixels);
    vector_2 RelCameraP = ProjectedP - Transform->ScreenCenter;
    vector_2 RelCameraPInMeters = PixelsToMeters * RelCameraP;
    
    if(Transform->Perspective)
    {
        r32 TargetZ = (Transform->CameraToTargetZ - WorldPZ);
        r32 UnprojectedRatio = (TargetZ / Transform->CameraToMonitorZ);
        vector_2 ResultXY = UnprojectedRatio * RelCameraPInMeters;
        
        //Result.Scale = PixelsToMeters * UnprojectedRatio;
        Result = Vector3(ResultXY, WorldPZ);
    }
    else
    {
        //Result.Scale = PixelsToMeters;
        Result = Vector3(RelCameraPInMeters, WorldPZ);
    }
    
    return Result;
}


#define PushRenderElement(Group, type) (type *)PushRenderElement_(Group, sizeof(type),RenderGroupEntryType_##type)

inline render_group_entry_header * 
PushRenderElement_(render_group * RenderGroup, uint32 Size, render_group_entry_type Type)
{
    IGNORED_TIMED_FUNCTION();
    Assert(RenderGroup->InsideRenderer);
    
    render_group_entry_header * Result = 0;
    
    Size += sizeof(render_group_entry_header);
    
    if((RenderGroup->PushBufferSize + Size) < RenderGroup->MaxPushBufferSize)
    {
        render_group_entry_header * Header = (render_group_entry_header *)(RenderGroup->PushBufferBase + RenderGroup->PushBufferSize);
        Header->Type = Type;
        Result = (Header + 1);
        RenderGroup->PushBufferSize += Size;
    }
    else
    {
        InvalidCodePath;
    }
    return Result;
}


inline used_bitmap_size_info
GetUsedBitmapSizeInfo(render_group * RenderGroup, loaded_bitmap * Bitmap, real32 Height, 
                      vector_3 Offset, b32 DebugRender = false)
{
    used_bitmap_size_info Result = {};
    Result.Size = Vector2(Height * Bitmap->WidthOverHeight, Height);
    
    if(DebugRender)
    {
        Result.Align = 0.5f * Result.Size;
    }
    else
    {
        Result.Align = Hadamard(Bitmap->AlignPorcentage, Result.Size);
    }
    
    Result.Position = Offset - Vector3(Result.Align, 0);
    Result.Basis = GetRenderEntityBasis(&RenderGroup->Transform, Result.Position);
    return Result;
}

inline void
PushBitmap(render_group * RenderGroup, loaded_bitmap * Bitmap, real32 Height,  
           vector_3 Offset, vector_4 Color = Vector4(1.0f,1.0f,1.0f,1.0f), b32 DebugRender = false)
{
    used_bitmap_size_info Info = GetUsedBitmapSizeInfo(RenderGroup, Bitmap, Height, Offset, DebugRender);
    
    if(Info.Basis.IsDrawable)
    {
        render_entry_bitmap * Entry = PushRenderElement(RenderGroup, render_entry_bitmap);
        
        if(Entry)
        {
            Entry->Bitmap = Bitmap;
            Entry->Position = Info.Basis.Position;
            Entry->Color = RenderGroup->GlobalAlpha * Color; 
            Entry->Size = Info.Basis.Scale * Info.Size;
        }
    }
}


inline void
PushBitmap(render_group * RenderGroup, bitmap_id ID, real32 Height,  
           vector_3 Offset, vector_4 Color = Vector4(1.0f,1.0f,1.0f,1.0f), b32 DebugRender = false)
{
    IGNORED_TIMED_FUNCTION();
    
    //TODO: Here you move your asset_header data based on each time the asset gets pushed to render
    loaded_bitmap * Bitmap = GetBitmap(RenderGroup->Assets, ID, RenderGroup->GenerationIndex);
    
    if(!Bitmap && RenderGroup->IsBackgroundThread)
    {
        LoadBitmap(RenderGroup->Assets, ID, true);
        Bitmap = GetBitmap(RenderGroup->Assets, ID, RenderGroup->GenerationIndex);
    }
    
    if(Bitmap)
    {
        PushBitmap(RenderGroup, Bitmap, Height, Offset, Color, DebugRender);
    }
    else
    {
        Assert(!RenderGroup->IsBackgroundThread);
        LoadBitmap(RenderGroup->Assets, ID, false);
        ++RenderGroup->MissingResourceCount;
    }
}

inline loaded_font *
PushFont(render_group * RenderGroup, font_id ID)
{
    IGNORED_TIMED_FUNCTION();
    
    //TODO: Here you move your asset_header data based on each time the asset gets pushed to render
    loaded_font * Font = GetFont(RenderGroup->Assets, ID, RenderGroup->GenerationIndex);
    
    if(Font)
    {
        //NOTE: Nothing more to do
    }
    else
    {
        Assert(!RenderGroup->IsBackgroundThread);
        LoadFont(RenderGroup->Assets, ID);
    }
    
    return Font;
}

inline void
PushRect(render_group * RenderGroup, vector_3 Offset, vector_2 Dimension, 
         vector_4 Color = Vector4(1.0f,1.0f,1.0f,1.0f))
{
    IGNORED_TIMED_FUNCTION();
    
    vector_3 Position = Offset - Vector3(0.5f * Dimension, 0);
    render_entity_basis_result Basis = GetRenderEntityBasis(&RenderGroup->Transform, Position);
    if(Basis.IsDrawable)
    {
        render_entry_rectangle * Rect = PushRenderElement(RenderGroup, render_entry_rectangle);
        
        if(Rect)
        {
            Rect->Position = Basis.Position;
            Rect->Color = RenderGroup->GlobalAlpha * Color; 
            Rect->Dimension = Basis.Scale * Dimension;
        }
    } 
}

inline void 
PushRect(render_group * RenderGroup, rectangle_2 RegionRect, r32 ZOffset, vector_4 Color)
{
    PushRect(RenderGroup, Vector3(GetCenter(RegionRect), ZOffset), GetDim(RegionRect), Color);
}

inline void
PushRectOutline(render_group * RenderGroup, vector_3 Offset, vector_2 Dimension, 
                vector_4 Color = Vector4(1.0f,1.0f,1.0f,1.0f), real32 Thickness = 0.15f)
{
    IGNORED_TIMED_FUNCTION();
    //NOTE: Top Bottom
    PushRect(RenderGroup, Offset - Vector3(0,0.5f * Dimension.y, 0), 
             Vector2(Dimension.x, Thickness), Color);
    PushRect(RenderGroup, Offset + Vector3(0,0.5f * Dimension.y, 0), 
             Vector2(Dimension.x, Thickness), Color);
    
    //NOTE: Left Right
    PushRect(RenderGroup, Offset + Vector3(0.5f * Dimension.x, 0, 0), 
             Vector2(Thickness, Dimension.y), Color);
    PushRect(RenderGroup, Offset - Vector3(0.5f * Dimension.x, 0, 0), 
             Vector2(Thickness, Dimension.y), Color);
    
}


inline void
Clear(render_group * RenderGroup, vector_4 Color)
{
    render_entry_clear * Entry = PushRenderElement(RenderGroup, render_entry_clear);
    if(Entry)
    {
        Entry->Color = Color;
    }
}

inline void  
PushCoordinateSystem(render_group * RenderGroup, vector_2 Origin, vector_2 XAxis, 
                     vector_2 YAxis, vector_4 Color, loaded_bitmap * Texture, loaded_bitmap * Normalmap,
                     environment_map * Top, environment_map * Middle, environment_map * Bottom)
{
#if 0
    render_entity_basis_result Basis = GetRenderEntityBasis(RenderGroup, &Entry->EntityBasis, 
                                                            ScreenDimension);
    if(Basis.IsDrawable)
    {
        render_entry_coordinate_system * Entry = PushRenderElement(RenderGroup, render_entry_coordinate_system);
        if(Entry)
        {
            Entry->Origin = Origin;
            Entry->XAxis = XAxis;
            Entry->YAxis = YAxis;
            Entry->Color = Color;
            Entry->Texture = Texture;
            Entry->Normalmap = Normalmap;
            Entry->Top = Top;
            Entry->Middle = Middle;
            Entry->Bottom = Bottom;
        }
        
    }
#endif
}

inline vector_2 
UnprojectOld(r32 CameraToMonitorZ,  vector_2 ProjectedXY, real32 AtDistanceFromCamera)
{
    vector_2 WorldXY = (AtDistanceFromCamera / CameraToMonitorZ) * ProjectedXY;
    return WorldXY;
}


inline vector_2 
Unproject(render_group * RenderGroup, vector_2 ProjectedXY, real32 AtDistanceFromCamera)
{
    vector_2 WorldXY = UnprojectOld(RenderGroup->Transform.CameraToMonitorZ, ProjectedXY, AtDistanceFromCamera);
    return WorldXY;
}

inline rectangle_2 
GetCameraRectangleAtDistance(render_group * RenderGroup, real32 Distance)
{
    rectangle_2 Result = {};
    
    vector_2 WorldXY = Unproject(RenderGroup, RenderGroup->MonitorHalfDimInMeters, Distance);  
    
    Result = RectCenterHalfDim(Vector2(0,0) , WorldXY);
    return Result;
}


inline rectangle_2 
GetCameraRectangleAtTarget(render_group * RenderGroup)
{
    rectangle_2 Result = GetCameraRectangleAtDistance(RenderGroup, RenderGroup->Transform.CameraToTargetZ);
    
    return Result;
}

inline bool32
AllResourcesPresent(render_group * RenderGroup)
{
    bool32 Result = (RenderGroup->MissingResourceCount == 0);
    return Result;
}