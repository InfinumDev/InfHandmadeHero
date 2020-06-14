#if !defined(HANDMADE_MATH_H)

union v2
{
    struct
    {
        float X, Y;
    };
    float E[2];
};

union v3
{
    struct
    {
        float X, Y, Z;
    };
    float E[3];
};


union v4
{
    struct
    {
        float X, Y, Z, W;
    };
    float E[4];
};

inline v2 V2(float X, float Y)
{
    v2 Result;

    Result.X = X;
    Result.Y = Y;

    return Result;
}

inline v3 V3(float X, float Y, float Z)
{
    v3 Result;

    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return Result;
}

inline v4 V4(float X, float Y, float Z, float W)
{
    v4 Result;

    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;

    return Result;
}

inline v2 operator*(float A, v2 B)
{
    v2 Result;

    Result.X = A*B.X;
    Result.Y = A*B.Y;

    return Result;
}

inline v2 operator*(v2 A, float B)
{
    v2 Result;

    Result.X = B*A.X;
    Result.Y = B*A.Y;

    return(Result);
}

inline v2 operator+(v2 A, v2 B)
{
    v2 Result;

    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;

    return Result;
}

inline v2 & operator*=(v2 &B, float A)
{
    B = B * A;
    return B;
}

inline v2 & operator+=(v2 &A, v2 B)
{
    A = A + B;
    return A;
}

inline v2 operator-(v2 A)
{
    v2 Result;

    Result.X = -A.X;
    Result.Y = -A.Y;

    return Result;
}

inline v2 operator-(v2 A, v2 B)
{
    v2 Result;

    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;

    return Result;
}

inline float Square(float X)
{
    return X*X;
}

inline float Inner(v2 A, v2 B)
{
    float Result;
    Result = A.X*B.X + A.Y*B.Y;
    return Result; 
}

inline float LengthSq(v2 A)
{
    float Result;
    Result = Inner(A, A);
    return Result;
}

struct rectangle2
{
    v2 Min;
    v2 Max;
};

inline v2 GetMinCorner(rectangle2 Rect)
{
    return Rect.Min;
}

inline v2 GetMaxCorner(rectangle2 Rect)
{
    return Rect.Max;
}

inline v2 GetCenter(rectangle2 Rect)
{
    v2 Result = 0.5f*(Rect.Min + Rect.Max);
    return Result;
}

inline rectangle2 RectMinMax(v2 Min, v2 Max)
{
    rectangle2 Result;
    
    Result.Min = Min;
    Result.Max = Max;
    
    return Result;
}

inline rectangle2 RectCenterHalfDim(v2 Center, v2 HalfDim)
{
    rectangle2 Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return Result;
}

inline rectangle2 RectCenterDim(v2 Center, v2 Dim)
{
    rectangle2 Result = RectCenterHalfDim(Center, 0.5f*Dim);

    return Result;
}

inline bool32 IsInRectangle(rectangle2 Rectangle, v2 Test)
{
    bool32 Result = ((Test.X >= Rectangle.Min.X) &&
                      (Test.Y >= Rectangle.Min.Y) &&
                      (Test.X < Rectangle.Max.X) &&
                      (Test.Y < Rectangle.Max.Y));

    return Result;
}

#define HANDMADE_MATH_H
#endif