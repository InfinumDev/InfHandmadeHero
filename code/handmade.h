#if !defined(HANDMADE_H)


#include "handmade_platform.h"
#include "handmade_math.h"

#define internal static 
#define local_persist static 
#define global_variable static

#define Pi32 3.14159265359f

#if HANDMADE_SLOW
// TODO(casey): Complete assertion macro - don't worry everyone!
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
// TODO(casey): swap, min, max ... macros???

inline uint32
SafeTruncateUInt64(uint64 Value)
{
    // TODO(casey): Defines for maximum values
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return(Result);
}

inline game_controller_input *GetController(game_input *Input, int unsigned ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

//
//
//

#include "handmade_intrinsics.h"
#include "handmade_world.h"

struct memory_arena
{
    memory_index Size;
    uint8 *Base;
    memory_index Used;
};

internal void
InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base)
{
    Arena->Size = Size;
    Arena->Base = Base;
    Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
void *
PushSize_(memory_arena *Arena, memory_index Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    
    return(Result);
}

struct loaded_bitmap
{
    int32 Width;
    int32 Height;
    uint32 *PixelPointer;
};

struct hero_direction_bitmaps
{
    int32 AlignX;
    int32 AlignY;
    loaded_bitmap PlayerHead;
    loaded_bitmap PlayerCape;
    loaded_bitmap PlayerTorso;
    uint32 PlayerDirection;
};

enum
{
    EntityFacingDirection_Down,
    EntityFacingDirection_Up,
    EntityFacingDirection_Right,
    EntityFacingDirection_Left,
};

enum entity_type
{
    EntityType_Null,
    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monster,
    EntityType_Sword,
};

struct low_entity
{
    entity_type Type;
    world_position P;
    float Width;
    float Height;

    bool32 Collides;
    int32 dAbsTileZ;
    uint32 HighEntityIndex;

    int32 HitPointMax;
    int32 HitPoints;

    uint32 SwordLowIndex;
    float DistanceRemaining;
};

struct high_entity
{
    v2 P; //NOTE(Infinum): already relative to the camera
    v2 dP;
    uint32 ChunkZ;
    uint32 PlayerDirection;
    uint32 LowEntityIndex;
    
    /*TODO: DELETE THIS */
    bool32 AttackedLastFrame;
    rectangle2 AttackArea;
};

struct entity
{
    uint32 LowIndex;
    high_entity *High;
    low_entity *Low;
};

struct game_state
{
    memory_arena WorldArena;
    world *World;

    uint32 CameraFollowingEntityIndex;    
    world_position CameraP;

    uint32 PlayerIndexForController[ArrayCount(((game_input *)0)->Controllers)];
    uint32 LowEntityCount;
    low_entity LowEntities[100000];

    uint32 HighEntityCount;
    high_entity HighEntities_[256];

    loaded_bitmap BackDrop;
    
    float MetersToPixels;
    hero_direction_bitmaps PlayerBitMaps[4];

    loaded_bitmap Tree;
    loaded_bitmap Sword;
};

#define HANDMADE_H
#endif
