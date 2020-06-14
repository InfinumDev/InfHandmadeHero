#include "handmade.h"

#include "handmade_world.cpp"

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0;
        SampleIndex < SoundBuffer->SampleCount;
        ++SampleIndex)
    {
        // TODO(casey): Draw this out for people
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

#if 0
        GameState->tSine += 2.0f*Pi32*1.0f/(real32)WavePeriod;
        if(GameState->tSine > 2.0f*Pi32)
        {
            GameState->tSine -= 2.0f*Pi32;
        }
#endif
    }
}

internal void
DrawRectangle(game_offscreen_buffer *Buffer,
              v2 vMin, v2 vMax,
              real32 R, real32 G, real32 B)
{    
    int32 MinX = RoundReal32ToInt32(vMin.X);
    int32 MinY = RoundReal32ToInt32(vMin.Y);
    int32 MaxX = RoundReal32ToInt32(vMax.X);
    int32 MaxY = RoundReal32ToInt32(vMax.Y);

    if(MinX < 0)
    {
        MinX = 0;
    }

    if(MinY < 0)
    {
        MinY = 0;
    }

    if(MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }

    if(MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    uint32 Color = ((RoundReal32ToUInt32(R * 255.0f) << 16) |
                    (RoundReal32ToUInt32(G * 255.0f) << 8) |
                    (RoundReal32ToUInt32(B * 255.0f) << 0));

    uint8 *Row = ((uint8 *)Buffer->Memory +
                  MinX*Buffer->BytesPerPixel +
                  MinY*Buffer->Pitch);
    for(int Y = MinY;
        Y < MaxY;
        ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = MinX;
            X < MaxX;
            ++X)
        {            
            *Pixel++ = Color;
        }
        
        Row += Buffer->Pitch;
    }
}

#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType; 
	uint32 FileSize;     
	uint16 Reserved1;    
	uint16 Reserved2;    
	uint32 BitmapOffset;  
    uint32 Size;             
	int32 Width;
	int32 Height;
	uint16 Planes;
	uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitMap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;    
};
#pragma pack(pop)

//Pixel values are packed -> BB GG RR AA (little endian), bottom up
// !WARNING(infinum): Always check for bitmap's RGBA sequence in case you have to swap their order
static loaded_bitmap DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *FileName)
{
    debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);
    loaded_bitmap Result = {};
    if(ReadResult.ContentsSize != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        Result.Height = Header->Height;
        Result.Width = Header->Width;

        Assert(Header->Compression == 3);

        uint32 AlphaMask = ~(Header->RedMask | Header->GreenMask | Header->BlueMask);
        uint32 RedShift = FindFirstSetBitIndex(Header->RedMask);
        uint32 GreenShift = FindFirstSetBitIndex(Header->GreenMask);
        uint32 BlueShift = FindFirstSetBitIndex(Header->BlueMask);
        uint32 AlphaShift = FindFirstSetBitIndex(AlphaMask);

        Assert((RedShift == 0 && (GreenShift && BlueShift && AlphaShift)) ||
            (BlueShift == 0 && (GreenShift && RedShift && AlphaShift)) ||
            (GreenShift == 0 && (BlueShift && RedShift && AlphaShift)) ||
            (AlphaShift == 0 && (GreenShift && RedShift && BlueShift)));

        uint32 *Pixels = (uint32 *)((uint8 *)(ReadResult.Contents) + Header->BitmapOffset);
        uint32 *SwapDest = Pixels;
        for(int32 Y = 0; Y < Header->Height; ++Y)
        {
            for(int32 X = 0; X < Header->Width; ++X)
            {
                uint32 Temp = *SwapDest;
                *SwapDest = (Temp >> AlphaShift) << 24 |
                            (Temp >> RedShift)  << 16 | 
                            (Temp >> GreenShift) << 8 | 
                            (Temp >> BlueShift) << 0;
                // *SwapDest = (*SwapDest >> RedShift) | (*SwapDest << AlphaShift);
                ++SwapDest; 
            }
        }
        Result.PixelPointer = Pixels;
    }
    return Result;
}

inline uint32 NewChannelValue(uint32 *DestPixel, uint32 *SourcePixel, uint32 ChannelBitShift)
{
    uint32 Result;

    uint32 AlphaShift = 24;
    uint32 SourceAlpha = *SourcePixel >> AlphaShift;
    float AlphaPercent = SourceAlpha / 255.0f;

    uint32 DestChannelValue = *DestPixel >> ChannelBitShift;
    uint32 SourceChannelValue = *SourcePixel >> ChannelBitShift;

    Result = RoundReal32ToInt32(DestChannelValue + (SourceChannelValue - DestChannelValue) * AlphaPercent) << ChannelBitShift;
    return Result;
}

static void DrawBitMap(game_offscreen_buffer *Buffer, loaded_bitmap *BitMapData, float ScreenX, float ScreenY, int32 AlignX = 0, int32 AlignY = 0)
{
    ScreenX -= (float)AlignX;
    ScreenY -= (float)AlignY;

    int32 MinX = RoundReal32ToInt32(ScreenX);
    int32 MinY = RoundReal32ToInt32(ScreenY);
    int32 MaxX = MinX + BitMapData->Width;
    int32 MaxY = MinY + BitMapData->Height;

    int32 SourceOffsetX = 0;
    int32 SourceOffsetY = 0;

    if(MinX < 0)
    {
        SourceOffsetX = -MinX;
        MinX = 0;
    }
    if(MinY < 0)
    {
        SourceOffsetY = -MinY;
        MinY = 0;
    }
    if(MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }
    if(MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    uint32 *SourceRow = BitMapData->PixelPointer + (BitMapData->Height - 1)*BitMapData->Width;
    SourceRow += -SourceOffsetY*BitMapData->Width + SourceOffsetX;
    uint8 *DestRow = (uint8 *)Buffer->Memory + MinY*Buffer->Pitch + MinX*Buffer->BytesPerPixel;

    for(int32 Y = MinY; Y < MaxY; ++Y)
    {
        uint32 *Source = SourceRow;
        uint32 *Dest = (uint32 *)DestRow;
        for(int32 X = MinX; X < MaxX; ++X)
        {
            float A = (float)(*Source >> 24) / 255.0f;
            float SourceR = (float)((*Source >> 16) & 0xFF);
            float SourceG = (float)((*Source >> 8) & 0xFF);
            float SourceB = (float)((*Source >> 0) & 0xFF);

            float DestR = (float)((*Dest >> 16) & 0xFF);
            float DestG = (float)((*Dest >> 8) & 0xFF);
            float DestB = (float)((*Dest >> 0) & 0xFF);

            float R = (1.0f - A)*DestR + A*SourceR;
            float G = (1.0f - A)*DestG + A*SourceG;
            float B = (1.0f - A)*DestB + A*SourceB;

            *Dest = (((uint32)(R + 0.5f) << 16) |
                        ((uint32)(G + 0.5f) << 8) |
                        ((uint32)(B + 0.5f) << 0));

            ++Source;
            ++Dest;
        }

        DestRow += Buffer->Pitch;
        SourceRow -= BitMapData->Width;
    }
}

inline void DrawHpBars(game_offscreen_buffer *Buffer, float MetersToPixels, 
                        v2 GroundPoint, entity Entity)
{
    v2 HpRect = v2{5.0f, 5.0f};

    int32 Hps = Entity.Low->HitPoints;

    GroundPoint.Y += 0.2f*MetersToPixels;

    //TODO(infinum): Upgrade to generic case hp count
    if(Hps == 1)
    {
        DrawRectangle(Buffer, GroundPoint, GroundPoint + HpRect, 1.0f, 0.0f, 0.0f);
        return;
    }
    if(Hps == 2)
    {
        GroundPoint = GroundPoint - v2{10.0f, 0.0f};
        DrawRectangle(Buffer, GroundPoint, GroundPoint + HpRect, 1.0f, 0.0f, 0.0f);
        GroundPoint = GroundPoint + v2{15.0f, 0.0f};
        DrawRectangle(Buffer, GroundPoint, GroundPoint + HpRect, 1.0f, 0.0f, 0.0f);
        return;
    }

    float SpaceBetween = (Entity.Low->Width*MetersToPixels - HpRect.X*Hps) / (Hps - 1);

    GroundPoint.X -= (int32)(0.5f*Entity.Low->Width*MetersToPixels);

    for(int32 HpIndex = 0; HpIndex < Hps; ++HpIndex)
    {
        DrawRectangle(Buffer, GroundPoint, GroundPoint + HpRect, 1.0f, 0.0f, 0.0f);
        GroundPoint.X += HpRect.X + SpaceBetween;
    }
}

inline v2 GetCameraSpaceP(game_state *GameState, low_entity *EntityLow)
{
    world_difference Diff = Subtract(GameState->World, &EntityLow->P, &GameState->CameraP);
    v2 Result = Diff.dXY;
    return Result;
}

inline high_entity * 
MakeEntityHighFrequency(game_state *GameState, low_entity *EntityLow, uint32 LowIndex, v2 CameraSpaceP)
{
    high_entity *EntityHigh = 0;

    Assert(EntityLow->HighEntityIndex == 0)
    if(EntityLow->HighEntityIndex == 0)
    {
        if(EntityLow->HighEntityIndex < ArrayCount(GameState->HighEntities_))
        {
            uint32 HighIndex = GameState->HighEntityCount++;
            EntityHigh = GameState->HighEntities_ + HighIndex;

            EntityHigh->P = CameraSpaceP;
            EntityHigh->dP = v2{0,0};
            EntityHigh->ChunkZ = EntityLow->P.ChunkZ;
            EntityHigh->PlayerDirection = EntityFacingDirection_Down;
            EntityHigh->LowEntityIndex = LowIndex;

            EntityLow->HighEntityIndex = HighIndex;
        }
        else
        {
            InvalidCodePath;
        }
    }

    return EntityHigh;
}

inline high_entity * 
MakeEntityHighFrequency(game_state *GameState, uint32 LowIndex)
{
    high_entity *EntityHigh;
    low_entity *EntityLow = GameState->LowEntities + LowIndex;
    if(EntityLow->HighEntityIndex)
    {
        EntityHigh = GameState->HighEntities_ + EntityLow->HighEntityIndex;
    }
    else
    {
        v2 CameraSpaceP = GetCameraSpaceP(GameState, EntityLow);
        EntityHigh = MakeEntityHighFrequency(GameState, EntityLow, LowIndex, CameraSpaceP);
    }

    return EntityHigh;
}

inline void
MakeEntityLowFrequency(game_state *GameState, uint32 LowIndex)
{
    low_entity *EntityLow = &GameState->LowEntities[LowIndex];
    uint32 HighIndex = EntityLow->HighEntityIndex;
    if(HighIndex)
    {
        uint32 LastHighIndex = GameState->HighEntityCount - 1;
        if(HighIndex != LastHighIndex)
        {
            high_entity *LastEntity = GameState->HighEntities_ + LastHighIndex;
            high_entity *DelEntity = GameState->HighEntities_ + HighIndex;

            *DelEntity = *LastEntity;
            GameState->LowEntities[LastEntity->LowEntityIndex].HighEntityIndex = HighIndex;
        }
        --GameState->HighEntityCount;
        EntityLow->HighEntityIndex = 0;
    }  
}

inline bool32 ValidateEntityPairs(game_state *GameState)
{
    bool32 Valid = true;

    for(uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount; ++HighEntityIndex)
    {
        high_entity *High = GameState->HighEntities_ + HighEntityIndex;
        Valid = Valid && (GameState->LowEntities[High->LowEntityIndex].HighEntityIndex == 
                                                                        HighEntityIndex);
    }

    return Valid;
}

inline void OffsetAndCheckFrequencyByArea(game_state *GameState, v2 Offset, rectangle2 HighFrequencyBounds)
{
    for(uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount; )
    {
        high_entity *High = GameState->HighEntities_ + HighEntityIndex;

        High->P += Offset;

        if(IsInRectangle(HighFrequencyBounds, High->P))
        {
            ++HighEntityIndex;
        }
        else
        {
            Assert(GameState->LowEntities[High->LowEntityIndex].HighEntityIndex == HighEntityIndex);
            MakeEntityLowFrequency(GameState, High->LowEntityIndex);
        }
    }
}

inline low_entity *GetLowEntity(game_state *GameState, uint32 LowIndex)
{
    low_entity *Result = 0;

    if(LowIndex > 0 && LowIndex < GameState->LowEntityCount)
    {
        Result = GameState->LowEntities + LowIndex;
    }
    return Result;
}

inline entity ForceIntoHighEntity(game_state *GameState, uint32 LowIndex)
{
    entity Result = {}; 

    if(LowIndex > 0 && LowIndex < GameState->LowEntityCount)
    {
        Result.LowIndex = LowIndex;
        Result.Low = GameState->LowEntities + LowIndex;
        Result.High = MakeEntityHighFrequency(GameState, LowIndex);
    }

    return Result;
}

inline entity EntityFromHighIndex(game_state *GameState, uint32 HighIndex)
{
    entity Result;

    Assert(HighIndex > 0 && HighIndex < GameState->HighEntityCount);

    Result.High = GameState->HighEntities_ + HighIndex;
    Result.Low = GameState->LowEntities + Result.High->LowEntityIndex;
    Result.LowIndex = Result.High->LowEntityIndex;

    return Result;
}

struct add_low_entity_result
{
    low_entity *Low;
    uint32 Index;
};
static add_low_entity_result AddLowEntity(game_state *GameState, entity_type Type, world_position *P)
{
    add_low_entity_result Result;

    Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));
    uint32 EntityIndex = GameState->LowEntityCount++;

    low_entity *EntityLow = GameState->LowEntities + EntityIndex;
    *EntityLow = {};
    EntityLow->Type = Type;

    ChangeEntityLocation(&GameState->WorldArena, GameState->World, EntityIndex, EntityLow, 
                                0, P);

    Result.Low = EntityLow;
    Result.Index = EntityIndex;

    return Result;
}

internal add_low_entity_result AddSword(game_state *GameState)
{
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Sword, 0);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = false;

    return Entity;
}

static add_low_entity_result AddPlayer(game_state *GameState)
{
    world_position P = GameState->CameraP;
    //P.Offset_.X += 2.0f;
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Hero, &P);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = true;
    Entity.Low->HitPointMax = 4;
    Entity.Low->HitPoints = Entity.Low->HitPointMax;
    Entity.Low->SwordLowIndex = AddSword(GameState).Index;

    if(GameState->CameraFollowingEntityIndex == 0)
    {
        GameState->CameraFollowingEntityIndex = Entity.Index;
    }

    return Entity;
}

internal add_low_entity_result AddMonster(game_state *GameState, uint32 AbsTileX, 
                                                    uint32 AbsTileY, 
                                                    uint32 AbsTileZ)
{
    world_position P = ChunkPositionFormTilePosition(GameState->World, AbsTileX, 
                                                            AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Monster, &P);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = true;
    Entity.Low->HitPointMax = 3;
    Entity.Low->HitPoints = Entity.Low->HitPointMax; 

    return Entity;
}

internal add_low_entity_result AddFamiliar(game_state *GameState, uint32 AbsTileX, 
                                                    uint32 AbsTileY, 
                                                    uint32 AbsTileZ)
{
    world_position P = ChunkPositionFormTilePosition(GameState->World, AbsTileX, 
                                                            AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Familiar, &P);

    Entity.Low->Height = 0.5f;
    Entity.Low->Width = 1.0f;
    Entity.Low->Collides = false;

    return Entity;
}

static add_low_entity_result AddWall(game_state *GameState, 
                        uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFormTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Wall, &P);

    Entity.Low->Height = GameState->World->TileSideInMeters;
    Entity.Low->Width = GameState->World->TileSideInMeters;
    Entity.Low->Collides = true;

    return Entity;
}

static bool32 TestWall(float WallX, float RelX, float RelY, 
                     float PlayerDeltaX, float PlayerDeltaY, 
                     float *tMin, float MinY, float MaxY)
{
    bool32 Result = false;
    float tEpsilon = 0.0001f;
    if(PlayerDeltaX != 0.0f)
    {
        float tResult = (WallX - RelX) / PlayerDeltaX;
        float Y = RelY + tResult*PlayerDeltaY;
        if(tResult >= 0.0f && *tMin > tResult)
        {
            if(Y >= MinY && Y <= MaxY)
            {
                *tMin = Maximum(0.0f, tResult - tEpsilon);
                Result = true;
            }
        }
    }
    return Result;
}

static void SetCamera(game_state *GameState, world_position NewCameraP)
{
    world *World = GameState->World;

    Assert(ValidateEntityPairs(GameState));

    world_difference dCameraP = Subtract(World, &NewCameraP, &GameState->CameraP);
    GameState->CameraP = NewCameraP;

    uint32 TileSpanX = 17*3;
    uint32 TileSpanY = 9*3;

    rectangle2 CameraBounds = RectCenterDim(v2{0,0}, 
                                              World->TileSideInMeters*v2{(float)TileSpanX, 
                                                                            (float)TileSpanY});
    
    v2 EntityOffsetPerFrame = -dCameraP.dXY;
    OffsetAndCheckFrequencyByArea(GameState, EntityOffsetPerFrame, CameraBounds);

    world_position MinChunkP = MapIntoChunkSpace(World, NewCameraP, 
                                                GetMinCorner(CameraBounds));
    world_position MaxChunkP = MapIntoChunkSpace(World, NewCameraP, 
                                                GetMaxCorner(CameraBounds));

    for(int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ++ChunkY)
    {
        for(int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ++ChunkX)
        {
            world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, NewCameraP.ChunkZ);
            if(Chunk)
            {
                for(world_entity_block *Block = &Chunk->FirstBlock; 
                    Block; 
                    Block = Block->Next)
                {
                    for(uint32 EntityIndexIndex = 0; 
                        EntityIndexIndex < Block->EntityCount; 
                        ++EntityIndexIndex)
                    {
                        uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndexIndex];
                        low_entity *Low = GameState->LowEntities + LowEntityIndex;
                        if(Low->HighEntityIndex == 0)
                        {
                            v2 CameraSpaceP = GetCameraSpaceP(GameState, Low);
                            if(IsInRectangle(CameraBounds, CameraSpaceP))
                            {
                                MakeEntityHighFrequency(GameState, Low, LowEntityIndex, CameraSpaceP);
                            }
                        }
                    }
                }
            }
        }
    }

    Assert(ValidateEntityPairs(GameState));
}

static void MoveEntity(game_state *GameState, entity Entity, float dt, v2 ddP)
{
    world *World = GameState->World;

    float ddPLengthSq = LengthSq(ddP);
    if(ddPLengthSq > 1.0f)
    {
        ddP *= (1.0f / SquareRoot(ddPLengthSq));
    }

    real32 PlayerAcceleration = 35.0f; // m/s^2
    if(Entity.Low->Type == EntityType_Familiar)
    {
        PlayerAcceleration = 20.0f;
    }

    ddP *= PlayerAcceleration;
    ddP += -5.0f*Entity.High->dP;
            
    v2 OldPlayerP = Entity.High->P;
    v2 PlayerDelta = 0.5f*Square(dt)*ddP + dt*Entity.High->dP;
    Entity.High->dP = ddP*dt + Entity.High->dP;
    v2 NewPlayerP = OldPlayerP + PlayerDelta;

    // uint32 MinTileX = Minimum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
    // uint32 MinTileY = Minimum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);
    // uint32 MaxTileX = Maximum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
    // uint32 MaxTileY = Maximum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);

    // uint32 EntityTileWidth = CeilingFloatToUInt32(Entity.Low->Width / World->TileSideInMeters);
    // uint32 EntityTileHeight = CeilingFloatToUInt32(Entity.Low->Height / World->TileSideInMeters);

    // MinTileX -= EntityTileWidth;
    // MinTileY -= EntityTileHeight;
    // MaxTileX += EntityTileWidth;
    // MaxTileY += EntityTileHeight;
    
    // Assert(MaxTileY - MinTileY < 32);
    // Assert(MaxTileX - MinTileX < 32);

    // uint32 AbsTileZ = Entity.Low->P.AbsTileZ;

    for(uint32 Iteration = 0; Iteration < 4; ++Iteration)
    {
        v2 WallNormal = {};
        float tMin = 1.0f;
        uint32 HitHighEntityIndex = 0;

        v2 DesiredPosition = Entity.High->P + PlayerDelta;

        for(uint32 TestHighEntityIndex = 1; TestHighEntityIndex < GameState->HighEntityCount; ++TestHighEntityIndex)
        {
            if(TestHighEntityIndex != Entity.Low->HighEntityIndex)
            {
                entity TestEntity = {};
                TestEntity.High = GameState->HighEntities_ + TestHighEntityIndex;
                TestEntity.LowIndex = TestEntity.High->LowEntityIndex;
                TestEntity.Low = GameState->LowEntities + TestEntity.LowIndex;
                if(TestEntity.Low->Collides)
                {    
                    float DiameterW = TestEntity.Low->Width + Entity.Low->Width;
                    float DiameterH = TestEntity.Low->Height + Entity.Low->Height;
                    v2 MinCorner = -0.5f*v2{DiameterW, DiameterH};
                    v2 MaxCorner = 0.5f*v2{DiameterW, DiameterH};

                    v2 Rel = Entity.High->P - TestEntity.High->P;

                    if(TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
                    {
                        WallNormal = {1,0};
                        HitHighEntityIndex = TestHighEntityIndex;
                    }
                    if(TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y))
                    {
                        WallNormal = {-1.0};
                        HitHighEntityIndex = TestHighEntityIndex;
                    }
                    if(TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
                    {
                        WallNormal = {0,-1};
                        HitHighEntityIndex = TestHighEntityIndex;
                    }
                    if(TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X))
                    {
                        WallNormal = {0, 1};
                        HitHighEntityIndex = TestHighEntityIndex;
                    }
                }
            }
        }

        Entity.High->P += tMin*PlayerDelta;
        
        if(HitHighEntityIndex)
        {
            Entity.High->dP = Entity.High->dP - 1*Inner(Entity.High->dP, WallNormal)*WallNormal;
            PlayerDelta = DesiredPosition - Entity.High->P;
            PlayerDelta = PlayerDelta - 1*Inner(PlayerDelta, WallNormal)*WallNormal;
            
            high_entity *HitHigh = GameState->HighEntities_ + HitHighEntityIndex;
            low_entity *HitLow = GameState->LowEntities + HitHigh->LowEntityIndex;
//            Entity.High->AbsTileZ += HitLow->dAbsTileZ;
        }
        else
        {
            break;
        }
    }

    if(AbsoluteValue(Entity.High->dP.X) > AbsoluteValue(Entity.High->dP.Y))
    {
        if(Entity.High->dP.X > 0)
        {
            Entity.High->PlayerDirection = EntityFacingDirection_Right;
        }
        else
        {
            Entity.High->PlayerDirection = EntityFacingDirection_Left;
        }
    }
    else
    {
        if(Entity.High->dP.Y > 0)
        {
            Entity.High->PlayerDirection = EntityFacingDirection_Up;
        }
        else
        {
            Entity.High->PlayerDirection = EntityFacingDirection_Down;
        }
    }
    //NOTE(infinum): Player's direction encoding 0-down, 1-up, 2-right, 3-left

    world_position NewP = MapIntoChunkSpace(World, GameState->CameraP, Entity.High->P);
    ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity.LowIndex, Entity.Low,
                        &Entity.Low->P, &NewP);
}

internal void PlayerMeleeAtack(game_state *GameState, entity Entity)
{
    int32 AttackDirection = Entity.High->PlayerDirection;
    rectangle2 AttackArea = {};
    switch(AttackDirection)
    {
        case EntityFacingDirection_Down:
            AttackArea.Min = {Entity.High->P.X - 1.0f, Entity.High->P.Y - 1.5f};
            AttackArea.Max = {Entity.High->P.X + 1.0f, Entity.High->P.Y};
            break;
        case EntityFacingDirection_Up:
            AttackArea.Min = {Entity.High->P.X - 1.0f, Entity.High->P.Y};
            AttackArea.Max = {Entity.High->P.X + 1.0f, Entity.High->P.Y + 1.5f};
            break;
        case EntityFacingDirection_Right:
            AttackArea.Min = {Entity.High->P.X, Entity.High->P.Y - 1.0f};
            AttackArea.Max = {Entity.High->P.X + 1.5f, Entity.High->P.Y + 1.0f};
            break;
        case EntityFacingDirection_Left:
            AttackArea.Min = {Entity.High->P.X - 1.5f, Entity.High->P.Y - 1.0f};
            AttackArea.Max = {Entity.High->P.X, Entity.High->P.Y + 1.0f};
            break;
        default:
            Assert(!"Entity has no direction");
    }
    Entity.High->AttackArea = AttackArea;
    Entity.High->AttackedLastFrame = true;
    for(uint32 EntityIndex = 1; EntityIndex < GameState->HighEntityCount; ++EntityIndex)
    {
        entity TestEntity = EntityFromHighIndex(GameState, EntityIndex); 
        if((TestEntity.Low->Type == EntityType_Monster) &&  
            IsInRectangle(AttackArea, TestEntity.High->P))
        {
            --TestEntity.Low->HitPoints;
        }
    }
}

internal void 
UpdateFamiliar(game_state *GameState, float dt, entity Entity)
{
    entity ClosestPlayer = {};
    float ClosestDSq = 49.0f;
    entity PlayerEntity = {};
    for(uint32 HighEntityIndex = 1;
        HighEntityIndex < GameState->HighEntityCount; 
        ++HighEntityIndex)
    {
        PlayerEntity = EntityFromHighIndex(GameState, HighEntityIndex);
        if(PlayerEntity.Low->Type == EntityType_Hero)
        {
            float SqDistanceBetweenFamiliarAndPLayer = LengthSq(Entity.High->P - PlayerEntity.High->P);
            if(SqDistanceBetweenFamiliarAndPLayer < ClosestDSq)
            {
                ClosestDSq = SqDistanceBetweenFamiliarAndPLayer;
                ClosestPlayer = PlayerEntity;
            }
        }
    }
    if(ClosestPlayer.High && ClosestDSq > 6.5f)
    {
        MoveEntity(GameState, Entity, dt, 
                v2{ClosestPlayer.High->P.X - Entity.High->P.X, ClosestPlayer.High->P.Y - Entity.High->P.Y});
    }
    else
    {
        MoveEntity(GameState, Entity, dt, v2{0.0f,0.0f});
    }
}

internal void UpdateMonster(game_state *GameState, float dt, entity Entity)
{

}


extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if(!Memory->IsInitialized)
    {
        //NOTE: reserverd entity slot for null-entity
        AddLowEntity(GameState, EntityType_Null, 0);
        GameState->HighEntityCount = 1;

        GameState->BackDrop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "../Handmade/data/test/test_background.bmp");
        GameState->Tree = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "../Handmade/data/test2/tree00.bmp");
        GameState->Sword = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "../Handmade/data/test2/rock03.bmp");

        hero_direction_bitmaps *HeroBitmap;
        HeroBitmap = GameState->PlayerBitMaps;

        HeroBitmap->PlayerHead = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_front_head.bmp");
        HeroBitmap->PlayerCape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_front_cape.bmp");
        HeroBitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_front_torso.bmp");
        HeroBitmap->AlignX = 72;
        HeroBitmap->AlignY = 182;
        ++HeroBitmap;

        HeroBitmap->PlayerHead = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_back_head.bmp");
        HeroBitmap->PlayerCape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_back_cape.bmp");
        HeroBitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_back_torso.bmp");
        HeroBitmap->AlignX = 72;
        HeroBitmap->AlignY = 182;

        ++HeroBitmap;

        HeroBitmap->PlayerHead = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_right_head.bmp");
        HeroBitmap->PlayerCape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_right_cape.bmp");
        HeroBitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_right_torso.bmp");
        HeroBitmap->AlignX = 72;
        HeroBitmap->AlignY = 182;

        ++HeroBitmap;

        HeroBitmap->PlayerHead = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_left_head.bmp");
        HeroBitmap->PlayerCape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_left_cape.bmp");
        HeroBitmap->PlayerTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, 
                                                            "../Handmade/data/test/test_hero_left_torso.bmp");
        HeroBitmap->AlignX = 72;
        HeroBitmap->AlignY = 182;

        ++HeroBitmap;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;
        InitializeWorld(World, 1.4f);
    
        int32 TileSideInPixels = 60;
        GameState->MetersToPixels = TileSideInPixels / World->TileSideInMeters;
        
#define RandomBlockSize 47

        uint32 RandomBlock[RandomBlockSize] = {132,2,98,49,81,321,6541,9684,91,65,165,4984,651,321,321651,9479,84,6516,2132,1,
                                    6516,57498,76,516,165,165,4987,4,162,1651,984,984,651,621,321,652,18,
                                    1781,85,29396,35,141,526,28,48,165,265294};

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;

        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;

        uint32 ScreenBaseX = 0; 
        uint32 ScreenBaseY = 0; 
        uint32 ScreenBaseZ = 0;
        uint32 ScreenX = ScreenBaseX;
        uint32 ScreenY = ScreenBaseY;
        uint32 AbsTileZ = ScreenBaseZ;

        uint32 RandomNumberIndex = 0;
        
        uint32 EntityMemoryIndex = 0;
        for(uint32 ScreenIndex = 0; ScreenIndex < 15; ++ScreenIndex)
        {
            Assert(RandomNumberIndex < ArrayCount(RandomBlock));

            uint32 RandomChoice;
            //if(DoorUp || DoorDown)
            {
                RandomChoice = RandomBlock[RandomNumberIndex++] % 2;
            }
            // else
            // {
            //     RandomChoice = RandomBlock[RandomNumberIndex++] % 3;
            // }

            bool32 CreatedZDoor = false;

            if(RandomChoice == 2)
            {
                CreatedZDoor = true;
                if(AbsTileZ == ScreenBaseZ)
                {
                    DoorUp = true;
                }
                else
                {
                    DoorDown = true;
                }
            }
            else if(RandomChoice == 1)
            {
                DoorRight = true;
            }
            else
            {
                DoorTop = true;
            }
            
            for(uint32 TileY = 0;
                TileY < TilesPerHeight;
                ++TileY)
            {
                for(uint32 TileX = 0;
                    TileX < TilesPerWidth;
                    ++TileX)
                {
                    uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;

                    uint32 TileValue = 1;
                     
                    if((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight/2))))
                    {
                        TileValue = 2;
                    }
                    if((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight/2))))
                    {
                        TileValue = 2;
                    }
                    if((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth/2))))
                    {
                        TileValue = 2;
                    }
                    if((TileY == TilesPerHeight - 1) && ((!DoorTop || (TileX != TilesPerWidth/2))))
                    {
                        TileValue = 2;
                    }
                    if(TileX == 10 && TileY == 6)
                    {
                        if(DoorUp)
                        {
                            TileValue = 3;
                        }
                        if(DoorDown)
                        {
                            TileValue = 4;
                        }
                    }
                    
                    if(TileValue == 2)
                    {
                        AddWall(GameState, AbsTileX, AbsTileY, AbsTileZ);
                    }
                }
            }
            
            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            if(CreatedZDoor)
            {
                DoorDown = !DoorDown;
                DoorUp = !DoorUp;
            }
            else
            {
                DoorUp = false;
                DoorDown = false;
            }

            DoorRight = false;
            DoorTop = false;

            if(RandomChoice == 2)
            {
                if(AbsTileZ == ScreenBaseZ)
                {
                    AbsTileZ = ScreenBaseZ + 1;
                }
                else
                {
                    AbsTileZ = ScreenBaseZ;
                }                
            }
            else if(RandomChoice == 1)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }

#if 0
        while(GameState->LowEntityCount < ArrayCount(GameState->LowEntities) - 16)
        {
            uint32 Coordinate = 1024 + GameState->LowEntityCount;
            AddWall(GameState)
        }
#endif

        world_position NewCameraP = {};

        int32 CameraBaseX = ScreenBaseX*TilesPerWidth + 17/2;
        int32 CameraBaseY = ScreenBaseY*TilesPerHeight + 9/2;

        NewCameraP = ChunkPositionFormTilePosition(GameState->World, 
                                                    CameraBaseX, 
                                                    CameraBaseY, 
                                                    ScreenBaseZ);

        AddMonster(GameState, CameraBaseX + 1, CameraBaseY + 1, ScreenBaseZ);
        AddFamiliar(GameState, CameraBaseX - 1, CameraBaseY - 1, ScreenBaseZ);

        SetCamera(GameState, NewCameraP);

        Memory->IsInitialized = true;
    }

    world *World = GameState->World;        

    float MetersToPixels = GameState->MetersToPixels;

    //Input->Controllers[1].Start.EndedDown = true;

    for(int ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        uint32 LowIndex = GameState->PlayerIndexForController[ControllerIndex];
        if(LowIndex == 0)
        {
            if(Controller->Start.EndedDown)
            {
                uint32 EntityIndex = AddPlayer(GameState).Index;
                GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
            }
        }
        else
        {
            entity ControllingEntity = ForceIntoHighEntity(GameState, LowIndex);
            v2 ddP = {};     
            v2 dSword = {};
            if(Controller->IsAnalog)
            {
                // NOTE(casey): Use analog movement tuning
                ddP = v2{Controller->StickAverageX, Controller->StickAverageY};
            }
            else
            {
                // NOTE(casey): Use digital movement tuning
                if(Controller->MoveUp.EndedDown)
                {
                    ddP.Y = 1.0f;
                }
                if(Controller->MoveDown.EndedDown)
                {
                    ddP.Y = -1.0f;
                }
                if(Controller->MoveLeft.EndedDown)
                {
                    ddP.X = -1.0f;
                }
                if(Controller->MoveRight.EndedDown)
                {
                    ddP.X = 1.0f;
                }
                if(Controller->Start.EndedDown && 
                    Controller->Start.HalfTransitionCount == 1)
                {
                    PlayerMeleeAtack(GameState, ControllingEntity);
                }

                if(Controller->ActionLeft.EndedDown)
                {
                    dSword = {-1.0f, 0.0f};
                }
                if(Controller->ActionRight.EndedDown)
                {
                    dSword = {1.0f, 0.0f};
                }
                if(Controller->ActionDown.EndedDown)
                {
                    dSword = {0.0f, -1.0f};
                }
                if(Controller->ActionUp.EndedDown)
                {
                    dSword = {0.0f, 1.0f};
                }
            }
            MoveEntity(GameState, ControllingEntity, Input->dtForFrame, ddP);
            if((dSword.X != 0) || (dSword.Y != 0))
            {
                low_entity *Sword = GetLowEntity(GameState, ControllingEntity.Low->SwordLowIndex);
                if(Sword && !IsValid(Sword->P))
                {
                    world_position SwordP = ControllingEntity.Low->P;
                    ChangeEntityLocation(&GameState->WorldArena, World, 
                                            ControllingEntity.Low->SwordLowIndex, 
                                            Sword, 0, &SwordP);
                }
            }
        }
    }

    entity CameraFollowingEntity = ForceIntoHighEntity(GameState, GameState->CameraFollowingEntityIndex);
    if(CameraFollowingEntity.High)
    {
        GameState->CameraP.ChunkZ = CameraFollowingEntity.Low->P.ChunkZ;

        world_position NewCameraP = GameState->CameraP;
    
#if 0
        if(CameraFollowingEntity.High->P.X > (9.0f*World->TileSideInMeters))
        {
            NewCameraP.AbsTileX += 17;
        }
        if(CameraFollowingEntity.High->P.X < -(9.0f*World->TileSideInMeters))
        {
            NewCameraP.AbsTileX -= 17;
        }

        if(CameraFollowingEntity.High->P.Y > (5.0f*World->TileSideInMeters))
        {
            NewCameraP.AbsTileY += 9;
        }
        if(CameraFollowingEntity.High->P.Y < -(5.0f*World->TileSideInMeters))
        {
            NewCameraP.AbsTileY -= 9;
        }
        SetCamera(GameState, NewCameraP);
#else
        SetCamera(GameState, CameraFollowingEntity.Low->P);
#endif
    }

    DrawRectangle(Buffer, v2{0.0f, 0.0f}, v2{(float)Buffer->Width, (float)Buffer->Height}, 1.0f, 1.0f, 0.0f);

    real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
    real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

    for(uint32 HighEntityIndex = 1; HighEntityIndex < GameState->HighEntityCount; ++HighEntityIndex)
    {
        high_entity *HighEntity = GameState->HighEntities_ + HighEntityIndex;
        low_entity *LowEntity = GameState->LowEntities + HighEntity->LowEntityIndex;

        entity Entity;
        Entity.High = HighEntity;
        Entity.Low = LowEntity;
        Entity.LowIndex = HighEntity->LowEntityIndex;

        v2 PlayerGroundPoint = {ScreenCenterX + HighEntity->P.X*MetersToPixels, ScreenCenterY - HighEntity->P.Y*MetersToPixels};
        v2 PlayerLeftTop = {PlayerGroundPoint.X - 0.5f*MetersToPixels*LowEntity->Width, PlayerGroundPoint.Y - 0.5f*MetersToPixels*LowEntity->Height};
        v2 EntityWidthHeight = {LowEntity->Width, LowEntity->Height};

        hero_direction_bitmaps *PlayerDirectionBitMap = &GameState->PlayerBitMaps[HighEntity->PlayerDirection];
        
        switch (LowEntity->Type)
        {
        case EntityType_Wall:
            DrawRectangle(Buffer, PlayerLeftTop, PlayerLeftTop + 0.9f*EntityWidthHeight*MetersToPixels, 1.0f, 1.0f, 1.0f);
            DrawBitMap(Buffer, &GameState->Tree, PlayerGroundPoint.X, PlayerGroundPoint.Y, 40, 80);    
            break;

        case EntityType_Monster:
            DrawRectangle(Buffer, PlayerLeftTop, PlayerLeftTop + EntityWidthHeight*MetersToPixels, 1.0f, 0.5f, 1.0f);
            DrawBitMap(Buffer, &PlayerDirectionBitMap->PlayerCape, PlayerGroundPoint.X, PlayerGroundPoint.Y, PlayerDirectionBitMap->AlignX, PlayerDirectionBitMap->AlignY);
            DrawHpBars(Buffer, MetersToPixels, PlayerGroundPoint, Entity);
            break;
        case EntityType_Sword:
            DrawBitMap(Buffer, &GameState->Sword, PlayerGroundPoint.X, PlayerGroundPoint.Y, 28, 17);
            break;
        case EntityType_Familiar:
            UpdateFamiliar(GameState, Input->dtForFrame, Entity);
            DrawRectangle(Buffer, PlayerLeftTop, PlayerLeftTop + EntityWidthHeight*MetersToPixels, 0.5f, 0.5f, 1.0f);
            DrawBitMap(Buffer, &PlayerDirectionBitMap->PlayerHead, PlayerGroundPoint.X, PlayerGroundPoint.Y, PlayerDirectionBitMap->AlignX, PlayerDirectionBitMap->AlignY);
            break;
        case EntityType_Hero:
            DrawBitMap(Buffer, &PlayerDirectionBitMap->PlayerTorso, PlayerGroundPoint.X, PlayerGroundPoint.Y, PlayerDirectionBitMap->AlignX, PlayerDirectionBitMap->AlignY);
            DrawBitMap(Buffer, &PlayerDirectionBitMap->PlayerCape, PlayerGroundPoint.X, PlayerGroundPoint.Y, PlayerDirectionBitMap->AlignX, PlayerDirectionBitMap->AlignY);
            DrawBitMap(Buffer, &PlayerDirectionBitMap->PlayerHead, PlayerGroundPoint.X, PlayerGroundPoint.Y, PlayerDirectionBitMap->AlignX, PlayerDirectionBitMap->AlignY);
            if(Entity.High->AttackedLastFrame)
            {
                DrawRectangle(Buffer, 
                                PlayerGroundPoint + GameState->MetersToPixels*Entity.High->AttackArea.Min, 
                                PlayerGroundPoint + GameState->MetersToPixels*Entity.High->AttackArea.Max, 
                                0.0f, 0.0f, 1.0f);
            }
            DrawHpBars(Buffer, MetersToPixels, PlayerGroundPoint, Entity);
            break;
        default:
            InvalidCodePath;
        }

        Entity.High->AttackedLastFrame = false;
    }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}