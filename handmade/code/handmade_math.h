#ifndef HANDMADE_MATH_H


inline vector_2
Vector2(real32 X, real32 Y)
{
    vector_2 Result;
    
    Result.x = X;
    Result.y = Y;
    
    return Result;
}

inline vector_3
Vector3(real32 X, real32 Y, real32 Z)
{
    vector_3 Result;
    
    Result.x = X;
    Result.y = Y;
    Result.z = Z;
    
    return Result;
}

inline vector_3
Vector3(vector_2 XY, real32 Z)
{
    vector_3 Result;
    
    Result.xy = XY;
    Result.z = Z;
    
    return Result;
}

inline vector_4
Vector4(real32 X, real32 Y, real32 Z, real32 W)
{
    vector_4 Result;
    
    Result.x = X;
    Result.y = Y;
    Result.z = Z;
    Result.w = W;
    
    return Result;
}


inline vector_4
Vector4(vector_3 XYZ, real32 W)
{
    vector_4 Result;
    
    Result.xyz = XYZ;
    Result.w = W;
    
    return Result;
}

//NOTE: Scalar operations

inline real32 
Square(real32 A)
{
    real32 Result = A*A;
    return Result;
}
inline real32
Lerp(real32 A,real32 t, real32 B )
{
    real32 Result = (1 - t) * A + (t * B);
    return Result;
}

inline real32 
Clamp(real32 Min,real32 Value, real32 Max)
{
    real32 Result = Value;
    if(Result < Min)
    {
        Result = Min;
    }
    else if (Result > Max)
    {
        Result = Max;
    }
    
    return Result;
}

inline real32
Clamp01(real32 Value)
{
    real32 Result = Clamp(0.0f, Value, 1.0f);
    return Result;
}

inline real32
Clamp01FromRange(real32 Min , real32 t, real32 Max)
{
    real32 Result = 0;
    real32 Range = Max - Min;
    
    if(Range != 0)
    {
        
        Result = Clamp01((t - Min) / Range);
    }
    return Result;
}

//NOTE: Automatically checks if two real numbers can be devided if not result returns a predefined v
inline real32 
SafeRatioN(real32 Numerator, real32 Divisor, real32 N)
{
    real32 Result = N;
    
    if(Divisor != 0.0f)
    {
        Result = Numerator / Divisor;
    }
    
    return Result;
}


inline real32 
SafeRatio0(real32 Numerator, real32 Divisor)
{
    real32 Result = SafeRatioN(Numerator, Divisor, 0.0f);
    
    return Result;
}


inline real32 
SafeRatio1(real32 Numerator, real32 Divisor)
{
    real32 Result = SafeRatioN(Numerator, Divisor, 1.0f);
    
    return Result;
}




//NOTE: Vector Operations

inline vector_2
Vector2i(int32 A, int32 B)
{
    vector_2 Result = {(real32)A , (real32)B};
    return Result;
}


inline vector_2
Vector2i(uint32 A, uint32 B)
{
    vector_2 Result = {(real32)A , (real32)B};
    return Result;
}

inline vector_2 
operator*(real32 LeftScalar, vector_2 RightVector)
{
    vector_2 Result;
    
    Result.x = LeftScalar * RightVector.x;
    Result.y = LeftScalar * RightVector.y;
    
    return Result;
}

inline vector_2 
operator*(vector_2 LeftVector, real32 RightScalar)
{
    vector_2 Result = RightScalar * LeftVector;
    return (Result);
}


inline vector_2 
operator-(vector_2 Vector2)
{
    vector_2 Result;
    
    Result.x = -Vector2.x;
    Result.y = -Vector2.y;
    
    return Result;
}

inline vector_2 
operator+(vector_2 LeftVector, vector_2 RightVector)
{
    vector_2 Result;
    
    Result.x = LeftVector.x + RightVector.x;
    Result.y = LeftVector.y + RightVector.y;
    
    return Result;
}


inline vector_2 
operator-(vector_2 LeftVector, vector_2 RightVector)
{
    vector_2 Result;
    
    Result.x = LeftVector.x - RightVector.x;
    Result.y = LeftVector.y - RightVector.y;
    
    return Result;
}



inline vector_2 &
operator+=(vector_2 &LeftVector, vector_2 RightVector)
{
    LeftVector = LeftVector + RightVector;
    
    return(LeftVector);
}


inline vector_2 &
operator*=(vector_2 &LeftVector, real32 RightScalar)
{
    LeftVector = RightScalar * LeftVector;
    
    return(LeftVector);
}

inline vector_2
Hadamard(vector_2 LeftVector , vector_2 RightVector)
{
    vector_2 Result = {LeftVector.x * RightVector.x , LeftVector.y * RightVector.y};
    return Result;
}

inline real32
InnerProduct(vector_2 LeftVector, vector_2 RightVector)
{
    real32 Result = LeftVector.x * RightVector.x + LeftVector.y * RightVector.y;
    
    return Result;
}

inline real32
LengthSq(vector_2 A)
{
    real32 Result = InnerProduct(A, A);
    
    return(Result);
}

inline real32
Length(vector_2 A)
{
    real32 Result = SquareRoot(LengthSq(A));
    return Result;
}

inline r32
GetDistance(vector_2 A, vector_2 B)
{
    vector_2 V2 = A - B;
    r32 Result = Length(V2);
    return Result;
}

inline vector_2
Clamp01(vector_2 A)
{
    vector_2 Result = {};
    
    Result.x = Clamp01(A.x);
    Result.y = Clamp01(A.y);
    
    return Result;
}

inline vector_2 
Normalize(vector_2 A)
{
    vector_2 Result = A * (1.0f / Length(A));
    return Result;
}


inline vector_2 
SafeRatioN(vector_2 Numerator, vector_2 Divisor, vector_2 N)
{
    vector_2 Result = {};
    
    Result.x = SafeRatioN(Numerator.x , Divisor.x, N.x);
    Result.y = SafeRatioN(Numerator.y , Divisor.y, N.y);
    
    return Result;
}


inline vector_2 
SafeRatio0(vector_2 Numerator, vector_2 Divisor)
{
    vector_2 Result = SafeRatioN(Numerator, Divisor, Vector2(0,0));
    
    return Result;
}


inline vector_2 
SafeRatio1(vector_2 Numerator, vector_2 Divisor)
{
    vector_2 Result = SafeRatioN(Numerator, Divisor, Vector2(1.0f,1.0f));
    
    return Result;
}



inline vector_2
Perp(vector_2 A)
{
    vector_2 Result ={};
    Result.x = -A.y;
    Result.y = A.x;
    return Result;
}

inline vector_2
Arm2(r32 Angle)
{
    vector_2 Result = Vector2(Cos(Angle), Sin(Angle));
    return Result;
}


//Vector3 Operations

inline vector_3 
operator*(real32 LeftScalar, vector_3 RightVector)
{
    vector_3 Result;
    
    Result.x = LeftScalar * RightVector.x;
    Result.y = LeftScalar * RightVector.y;
    Result.z = LeftScalar * RightVector.z;
    
    return Result;
}

inline vector_3 
operator*(vector_3 LeftVector, real32 RightScalar)
{
    vector_3 Result = RightScalar * LeftVector;
    return (Result);
}


inline vector_3 
operator-(vector_3 Vector3)
{
    vector_3 Result;
    
    Result.x = -Vector3.x;
    Result.y = -Vector3.y;
    Result.z = -Vector3.z;
    
    return Result;
}

inline vector_3 
operator+(vector_3 LeftVector, vector_3 RightVector)
{
    vector_3 Result;
    
    Result.x = LeftVector.x + RightVector.x;
    Result.y = LeftVector.y + RightVector.y;
    Result.z = LeftVector.z + RightVector.z;
    
    return Result;
}


inline vector_3 
operator-(vector_3 LeftVector, vector_3 RightVector)
{
    vector_3 Result;
    
    Result.x = LeftVector.x - RightVector.x;
    Result.y = LeftVector.y - RightVector.y;
    Result.z = LeftVector.z - RightVector.z;
    
    return Result;
}



inline vector_3 &
operator+=(vector_3 & LeftVector, vector_3 RightVector)
{
    LeftVector = LeftVector + RightVector;
    
    return(LeftVector);
}


inline vector_3 &
operator*=(vector_3 & LeftVector, real32 RightScalar)
{
    LeftVector = RightScalar * LeftVector;
    
    return(LeftVector);
}


inline vector_3
Hadamard(vector_3 LeftVector , vector_3 RightVector)
{
    vector_3 Result = {LeftVector.x * RightVector.x , LeftVector.y * RightVector.y, LeftVector.z * RightVector.z};
    return Result;
}

inline real32
InnerProduct(vector_3 LeftVector, vector_3 RightVector)
{
    real32 Result = LeftVector.x * RightVector.x + LeftVector.y * RightVector.y + LeftVector.z * RightVector.z;
    
    return Result;
}

inline real32
LengthSq(vector_3 A)
{
    real32 Result = InnerProduct(A, A);
    
    return(Result);
}

inline real32
Length(vector_3 A)
{
    real32 Result = SquareRoot(LengthSq(A));
    return Result;
}


inline vector_3
Clamp01(vector_3 A)
{
    vector_3 Result = {};
    
    Result.x = Clamp01(A.x);
    Result.y = Clamp01(A.y);
    Result.z = Clamp01(A.z);
    
    return Result;
}

inline vector_3 
Normalize(vector_3 A)
{
    vector_3 Result = A * (1.0f / Length(A));
    return Result;
}

inline vector_3
Lerp(vector_3 A,real32 t, vector_3 B )
{
    vector_3 Result = (1 - t)*A + t * B;
    
    return Result;
}
//Vector4 Operations

inline vector_4 
operator*(real32 LeftScalar, vector_4 RightVector)
{
    vector_4 Result;
    
    Result.x = LeftScalar * RightVector.x;
    Result.y = LeftScalar * RightVector.y;
    Result.z = LeftScalar * RightVector.z;
    Result.w = LeftScalar * RightVector.w;
    
    return Result;
}

inline vector_4 
operator*(vector_4 LeftVector, real32 RightScalar)
{
    vector_4 Result = RightScalar * LeftVector;
    return (Result);
}


inline vector_4 
operator-(vector_4 Vector2)
{
    vector_4 Result;
    
    Result.x = -Vector2.x;
    Result.y = -Vector2.y;
    Result.z = -Vector2.z;
    Result.w = -Vector2.w;
    
    return Result;
}

inline vector_4 
operator+(vector_4 LeftVector, vector_4 RightVector)
{
    vector_4 Result;
    
    Result.x = LeftVector.x + RightVector.x;
    Result.y = LeftVector.y + RightVector.y;
    Result.z = LeftVector.z + RightVector.z;
    Result.w = LeftVector.w + RightVector.w;
    
    return Result;
}


inline vector_4 
operator-(vector_4 LeftVector, vector_4 RightVector)
{
    vector_4 Result;
    
    Result.x = LeftVector.x - RightVector.x;
    Result.y = LeftVector.y - RightVector.y;
    Result.z = LeftVector.z - RightVector.z;
    Result.w = LeftVector.w - RightVector.w;
    
    return Result;
}


inline vector_4 &
operator+=(vector_4 &LeftVector, vector_4 RightVector)
{
    LeftVector = LeftVector + RightVector;
    
    return(LeftVector);
}


inline vector_4 &
operator*=(vector_4 &LeftVector, real32 RightScalar)
{
    LeftVector = RightScalar * LeftVector;
    
    return(LeftVector);
}


inline vector_4
Hadamard(vector_4 LeftVector , vector_4 RightVector)
{
    vector_4 Result = {LeftVector.x * RightVector.x , LeftVector.y * RightVector.y, LeftVector.z * RightVector.z, LeftVector.w * RightVector.w};
    return Result;
}

inline real32
InnerProduct(vector_4 LeftVector, vector_4 RightVector)
{
    real32 Result = LeftVector.x * RightVector.x + LeftVector.y * RightVector.y + LeftVector.z * RightVector.z + LeftVector.w * RightVector.w;
    
    return Result;
}

inline real32
LengthSq(vector_4 A)
{
    real32 Result = InnerProduct(A, A);
    
    return(Result);
}

inline real32
Length(vector_4 A)
{	
    real32 Result = SquareRoot(LengthSq(A));
    return Result;
}

inline vector_4
Lerp(vector_4 A,real32 t, vector_4 B )
{
    vector_4 Result = (1 - t)*A + t * B;
    
    return Result;
}

//NOTE: Rectangle_2 Operations

#define InvertedDimensions(Dim) ((Dim).x < 0 || (Dim).y < 0)

inline vector_2 
GetMin(rectangle_2 Rect)
{
    vector_2 Result = Rect.Min;
    return Result;
}

inline vector_2 
GetMax(rectangle_2 Rect)
{	
    vector_2 Result = Rect.Max;
    return Result;
}


inline vector_2 
GetCenter(rectangle_2 Rect)
{
    vector_2 Result = 0.5f * (Rect.Min + Rect.Max);
    return Result;
}

inline vector_2
GetDim(rectangle_2 Rect)
{
    vector_2 Result = (Rect.Max - Rect.Min);
    return Result;
}


inline rectangle_2
RectMinMax(vector_2 Min, vector_2 Max)
{
    rectangle_2 Rect = {};
    Rect.Min = Min;
    Rect.Max = Max;
    
    return Rect;
}


inline rectangle_2
RectCenterHalfDim(vector_2 Center, vector_2 HalfDim)
{
    rectangle_2 Rect = {};
    
    Rect.Min = Center - HalfDim;
    Rect.Max = Center + HalfDim;
    
    return Rect;
}

inline rectangle_2
RectCenterDim(vector_2 Center, vector_2 Dim)
{
    rectangle_2 Rect = RectCenterHalfDim(Center, 0.5*Dim);
    return Rect;
}


inline rectangle_2
RectMinDim(vector_2 Min, vector_2 Dim)
{
    rectangle_2 Rect = {};
    
    Rect.Min = Min;
    Rect.Max = Min + Dim;
    
    return Rect;
}

inline rectangle_2
AddRadiusTo(rectangle_2 BaseRect, vector_2 Radius)
{
    rectangle_2 Rect = {};
    
    Rect.Min = BaseRect.Min - Radius;
    Rect.Max = BaseRect.Max + Radius;
    
    return Rect;
}



inline bool32 
IsInRectangle(rectangle_2 Rect, vector_2 Test)
{
    if(Test.x >= Rect.Min.x && 
       Test.x < Rect.Max.x && 
       Test.y >= Rect.Min.y && 
       Test.y < Rect.Max.y )
    {
        return true;
    }
    
    return false;
}


inline vector_2
GetBarycentricPosition(rectangle_2 Region, vector_2 Position)
{
    vector_2 Result = {};
    
    Result.x = SafeRatio0(Position.x - Region.Min.x , Region.Max.x - Region.Min.x); 
    Result.y = SafeRatio0(Position.y - Region.Min.y , Region.Max.y - Region.Min.y); 
    
    return Result;
}


inline rectangle_2
Union(rectangle_2 A, rectangle_2 B)
{
    rectangle_2 Result;
    Result.Min.y = (A.Min.y < B.Min.y) ? A.Min.y : B.Min.y;
    Result.Max.y = (A.Max.y > B.Max.y) ? A.Max.y : B.Max.y;
    Result.Min.x = (A.Min.x < B.Min.x) ? A.Min.x : B.Min.x;
    Result.Max.x = (A.Max.x > B.Max.x) ? A.Max.x : B.Max.x;
    return Result;
}


inline rectangle_2
Offset(rectangle_2 A, vector_2 Vec2)
{
    rectangle_2 Rect = {};
    
    Rect.Min = A.Min + Vec2;
    Rect.Max = A.Max + Vec2;
    
    return Rect;
}


inline rectangle_2
InvertedInfinityRectangle2(void)
{
    rectangle_2 Result = {};
    Result.Min.x =  Result.Min.y = Real32Max;
    Result.Max.x =  Result.Max.y = -Real32Max;
    return Result;
}


//NOTE: rectangle_3 Operations

inline vector_3 GetMin(rectangle_3 Rect)
{
    vector_3 Result = Rect.Min;
    return Result;
}

inline vector_3 GetMax(rectangle_3 Rect)
{	
    vector_3 Result = Rect.Max;
    return Result;
}


inline vector_3 GetCenter(rectangle_3 Rect)
{
    vector_3 Result = 0.5f * (Rect.Min + Rect.Max);
    return Result;
}


inline vector_3
GetDim(rectangle_3 Rect)
{
    vector_3 Result = (Rect.Max - Rect.Min);
    return Result;
}

inline rectangle_3
RectMinMax(vector_3 Min, vector_3 Max)
{
    rectangle_3 Rect = {};
    Rect.Min = Min;
    Rect.Max = Max;
    
    return Rect;
}



inline rectangle_3
RectCenterHalfDim(vector_3 Center, vector_3 HalfDim)
{
    rectangle_3 Rect = {};
    
    Rect.Min = Center - HalfDim;
    Rect.Max = Center + HalfDim;
    
    return Rect;
}

inline rectangle_3
RectCenterDim(vector_3 Center, vector_3 Dim)
{
    rectangle_3 Rect = RectCenterHalfDim(Center, 0.5*Dim);
    return Rect;
}


inline rectangle_3
RectMinDim(vector_3 Min, vector_3 Dim)
{
    rectangle_3 Rect = {};
    
    Rect.Min = Min;
    Rect.Max = Min + Dim;
    
    return Rect;
}

inline rectangle_3
AddRadiusTo(rectangle_3 BaseRect, vector_3 Radius)
{
    rectangle_3 Rect = {};
    
    Rect.Min = BaseRect.Min - Radius;
    Rect.Max = BaseRect.Max + Radius;
    
    return Rect;
}


inline rectangle_3
Offset(rectangle_3 A, vector_3 Vec3)
{
    rectangle_3 Rect = {};
    
    Rect.Min = A.Min + Vec3;
    Rect.Max = A.Max + Vec3;
    
    return Rect;
}


inline bool32 
IsInRectangle(rectangle_3 Rect, vector_3 Test)
{
    if(Test.x >= Rect.Min.x && 
       Test.x < Rect.Max.x && 
       Test.y >= Rect.Min.y && 
       Test.y < Rect.Max.y && 
       Test.z >= Rect.Min.z && 
       Test.z < Rect.Max.z)
    {
        return true;
    }
    
    return false;
}


inline bool32
RectanglesOverlap(rectangle_3 A , rectangle_3 B)
{
    bool32 Result = !((A.Min.x >= B.Max.x ||  
                       B.Min.x >= A.Max.x || 
                       A.Min.y >= B.Max.y ||  
                       B.Min.y >= A.Max.y || 
                       A.Min.z >= B.Max.z ||  
                       B.Min.z >= A.Max.z));
    return Result;
    
}

inline vector_3
GetBarycentricPosition(rectangle_3 Region, vector_3 Position)
{
    vector_3 Result = {};
    
    Result.x = SafeRatio0(Position.x - Region.Min.x , Region.Max.x - Region.Min.x); 
    Result.y = SafeRatio0(Position.y - Region.Min.y , Region.Max.y - Region.Min.y); 
    Result.z = SafeRatio0(Position.z - Region.Min.z , Region.Max.z - Region.Min.z); 
    
    return Result;
}


inline rectangle_2
ToRectangle2(rectangle_3 A)
{
    rectangle_2 Result = {};
    Result.Min = A.Min.xy;
    Result.Max = A.Max.xy;
    
    return Result;
}


inline rectangle_3
Union(rectangle_3 A, rectangle_3 B)
{
    rectangle_3 Result;
    Result.Min.y = (A.Min.y < B.Min.y) ? A.Min.y : B.Min.y;
    Result.Max.y = (A.Max.y > B.Max.y) ? A.Max.y : B.Max.y;
    Result.Min.x = (A.Min.x < B.Min.x) ? A.Min.x : B.Min.x;
    Result.Max.x = (A.Max.x > B.Max.x) ? A.Max.x : B.Max.x;
    Result.Min.z = (A.Min.z < B.Min.z) ? A.Min.z : B.Min.z;
    Result.Max.z = (A.Max.z > B.Max.z) ? A.Max.z : B.Max.z;
    return Result;
}

inline rectangle_3
InvertedInfinityRectangle3(void)
{
    rectangle_3 Result = {};
    Result.Min.x =  Result.Min.y = Result.Min.z = Real32Max;
    Result.Max.x =  Result.Max.y = Result.Max.z =-Real32Max;
    return Result;
}


//
//NOTE: Discrete Rectangle Operations  
//


inline rectangle_2i
RectMinMax(s32 Minx, s32 Miny,
           s32 Maxx, s32 Maxy)
{
    rectangle_2i Rect = {};
    
    Rect.Minx = Minx;
    Rect.Miny = Miny;
    Rect.Maxx = Maxx;
    Rect.Maxy = Maxy;
    
    return Rect;
}


inline rectangle_2i
Intersect(rectangle_2i A, rectangle_2i B)
{
    rectangle_2i Result;
    
    Result.Miny = (A.Miny < B.Miny) ? B.Miny : A.Miny;
    Result.Maxy = (A.Maxy > B.Maxy) ? B.Maxy : A.Maxy;
    Result.Minx = (A.Minx < B.Minx) ? B.Minx : A.Minx;
    Result.Maxx = (A.Maxx > B.Maxx) ? B.Maxx : A.Maxx;
    
    return Result;
}

inline rectangle_2i
Union(rectangle_2i A, rectangle_2i B)
{
    rectangle_2i Result;
    Result.Miny = (A.Miny < B.Miny) ? A.Miny : B.Miny;
    Result.Maxy = (A.Maxy > B.Maxy) ? A.Maxy : B.Maxy;
    Result.Minx = (A.Minx < B.Minx) ? A.Minx : B.Minx;
    Result.Maxx = (A.Maxx > B.Maxx) ? A.Maxx : B.Maxx;
    return Result;
}




inline int32 
GetClampedRectArea(rectangle_2i A)
{
    int32 Result = 0;
    if(A.Maxx >= A.Minx  && A.Maxy >= A.Miny)
    {
        Result = (A.Maxx - A.Minx) * (A.Maxy - A.Miny);
    }
    
    return Result;
}


inline bool32
RectanglesOverlap(rectangle_2i A , rectangle_2i B)
{
    bool32 Result = !((A.Minx >= B.Maxx ||  
                       B.Minx >= A.Maxx || 
                       A.Miny >= B.Maxy ||  
                       B.Miny >= A.Maxy));
    return Result;
    
}

inline rectangle_2i
NullRectangle()
{
    rectangle_2i Result = {};
    return Result;
}

inline rectangle_2i
InvertedInfinityRectangle2i(void)
{
    rectangle_2i Result = {};
    Result.Minx =  Result.Miny = S32Max;
    Result.Maxx =  Result.Maxy = -S32Max;
    return Result;
}


//NOTE: Linear space to SRGB conversions
inline vector_4 
SRGB255ToLinear1(vector_4 A)
{
    vector_4 Result = {};
    
    real32 Inv255 = 1 / 255.0f;
    
    Result.r = Square(Inv255 * A.r);
    Result.g = Square(Inv255 * A.g);
    Result.b = Square(Inv255 * A.b);
    Result.a = Inv255 * A.a;
    
    return Result;
}

inline vector_4
Linear1ToSRGB255(vector_4 A)
{
    vector_4 Result = {};
    
    real32 One255 = 255.0f;
    
    Result.r = One255 * SquareRoot(A.r);
    Result.g = One255 * SquareRoot(A.g);
    Result.b = One255 * SquareRoot(A.b);
    Result.a = One255 * A.a;
    
    return Result;
}

#define HANDMADE_MATH_H
#endif

