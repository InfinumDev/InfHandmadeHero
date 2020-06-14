#if !defined(HANDMADE_WORLD_H)

struct world_position
{
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    //NOTE(infinum): offsets are from chunk center
    v2 Offset_;
};

struct world_difference
{
    v2 dXY;
    float dZ;
};

struct world_entity_block
{
    uint32 EntityCount;
    uint32 LowEntityIndex[16];
    world_entity_block *Next;
};

struct world_chunk
{
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    world_entity_block FirstBlock;

    world_chunk *NextInHash;
};

struct world
{
    float TileSideInMeters;
    real32 ChunkSideInMeters;

    world_entity_block *FirstFree;

    world_chunk ChunkHash[4096];
};


#define HANDMADE_WORLD_H
#endif
