
#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "Enderman.h"
#include "../Entities/Player.h"
#include "../Tracer.h"




////////////////////////////////////////////////////////////////////////////////
// cPlayerLookCheck
class cPlayerLookCheck :
	public cPlayerListCallback
{
public:
	cPlayerLookCheck(Vector3d a_EndermanPos, int a_SightDistance) :
		m_Player(nullptr),
		m_EndermanPos(a_EndermanPos),
		m_SightDistance(a_SightDistance)
	{
	}

	virtual bool Item(cPlayer * a_Player) override
	{
		// Don't check players who are in creative gamemode
		if (a_Player->IsGameModeCreative())
		{
			return false;
		}

		Vector3d Direction = m_EndermanPos - a_Player->GetPosition();

		// Don't check players who are more then SightDistance (64) blocks away
		if (Direction.Length() > m_SightDistance)
		{
			return false;
		}

		// Don't check if the player has a pumpkin on his head
		if (a_Player->GetEquippedHelmet().m_ItemType == E_BLOCK_PUMPKIN)
		{
			return false;
		}


		Vector3d LookVector = a_Player->GetLookVector();
		double dot = Direction.Dot(LookVector);

		// 0.09 rad ~ 5 degrees
		// If the player's crosshair is within 5 degrees of the enderman, it counts as looking
		if (dot <= cos(0.09))
		{
			return false;
		}

		cTracer LineOfSight(a_Player->GetWorld());
		if (LineOfSight.Trace(m_EndermanPos, Direction, (int)Direction.Length()))
		{
			// No direct line of sight
			return false;
		}

		m_Player = a_Player;
		return true;
	}

	cPlayer * GetPlayer(void) const { return m_Player; }

protected:
	cPlayer * m_Player;
	Vector3d m_EndermanPos;
	int m_SightDistance;
} ;


////////////////////////////////////////////////////////////////////////////////
// cEnderman
const unsigned int cEnderman::XZ_TELEPORT_RANGE = 32;
const unsigned int cEnderman::Y_TELEPORT_RANGE = 5;
const unsigned int cEnderman::DAY_WARP_MAX_TICKS = 10;

cEnderman::cEnderman(void) :
	super("Enderman", mtEnderman, "mob.endermen.hit", "mob.endermen.death", 0.5, 2.9),
	m_bIsScreaming(false),
	m_dayWarpTicks(DAY_WARP_MAX_TICKS),
	CarriedBlock(E_BLOCK_AIR),
	CarriedMeta(0)
{
}





void cEnderman::GetDrops(cItems & a_Drops, cEntity * a_Killer)
{
	int LootingLevel = 0;
	if (a_Killer != nullptr)
	{
		LootingLevel = a_Killer->GetEquippedWeapon().m_Enchantments.GetLevel(cEnchantments::enchLooting);
	}
	AddRandomDropItem(a_Drops, 0, 1 + LootingLevel, E_ITEM_ENDER_PEARL);
}




void cEnderman::CheckEventSeePlayer()
{
	if (m_Target != nullptr)
	{
		return;
	}

	cPlayerLookCheck Callback(GetPosition(), m_SightDistance);
	if (m_World->ForEachPlayer(Callback))
	{
		return;
	}

	ASSERT(Callback.GetPlayer() != nullptr);

	if (!CheckLight())
	{
		// Insufficient light for enderman to become aggravated
		TeleportRandomLocation();
		return;
	}

	if (!Callback.GetPlayer()->IsGameModeCreative())
	{
		super::EventSeePlayer(Callback.GetPlayer());
		m_EMState = CHASING;
		m_bIsScreaming = true;
		GetWorld()->BroadcastEntityMetadata(*this);
	}
}





void cEnderman::CheckEventLostPlayer(void)
{
	super::CheckEventLostPlayer();
	if (!CheckLight())
	{
		EventLosePlayer();
	}
}





void cEnderman::EventLosePlayer()
{
	super::EventLosePlayer();
	m_bIsScreaming = false;
	GetWorld()->BroadcastEntityMetadata(*this);
}





bool cEnderman::CheckLight()
{
	int ChunkX, ChunkZ;
	cChunkDef::BlockToChunk(POSX_TOINT, POSZ_TOINT, ChunkX, ChunkZ);

	// Check if the chunk the enderman is in is lit
	if (!m_World->IsChunkLighted(ChunkX, ChunkZ))
	{
		m_World->QueueLightChunk(ChunkX, ChunkZ);
		return true;
	}

	// Enderman only attack if the skylight is lower or equal to 8
	if (m_World->GetBlockSkyLight(POSX_TOINT, POSY_TOINT, POSZ_TOINT) - GetWorld()->GetSkyDarkness() > 8)
	{
		return false;
	}

	return true;
}





void cEnderman::Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	super::Tick(a_Dt, a_Chunk);

	// Day time, teleport away
	if(!CheckLight())
	{
        if(m_dayWarpTicks == 0)
        {
            TeleportRandomLocation();
            m_dayWarpTicks = DAY_WARP_MAX_TICKS;
        }
        else
        {
            m_dayWarpTicks--;
        }
	}

	// TODO take damage in rain

	// Take damage when touching water, drowning damage seems to be most appropriate
	if (IsSwimming())
	{
        EventLosePlayer();
        TakeDamage(dtDrowning, nullptr, 1, 1, 0);
        TeleportRandomLocation();
	}
}




bool cEnderman::TeleportRandomLocation()
{
    const Vector3d currPos = GetPosition();
    cFastRandom frand;
    int randomValue = frand.NextInt(XZ_TELEPORT_RANGE*2);
    int yRandom = frand.NextInt(Y_TELEPORT_RANGE*2);

    // randomly generate coordinates of next location
    int x = (randomValue - XZ_TELEPORT_RANGE) + (int)currPos.x;
    int y = (yRandom - Y_TELEPORT_RANGE) + (int)currPos.y;
    int z = (randomValue - XZ_TELEPORT_RANGE) + (int)currPos.z;

    int cX, cZ;

    cChunkDef::BlockToChunk(x, z, cX, cZ);

    if(y > 0 && m_World->IsChunkValid(cX, cZ))
    {
        // Check if location has a solid block below
        BLOCKTYPE floor = m_World->GetBlock(x, y-1, z);

        if(floor == E_BLOCK_AIR || floor == E_BLOCK_WATER || floor == E_BLOCK_LAVA)
        {
            return false;
        }

        // Check if location generated has enough free blocks to fit the endermen, if not, return false
        BLOCKTYPE blocks[] = {
            m_World->GetBlock(x, y, z),
            m_World->GetBlock(x, y+1, z),
            m_World->GetBlock(x, y+2, z)
        };

        for(int i = 0; i < 3; i++)
        {
            if(blocks[i] != E_BLOCK_AIR || blocks[i] != E_BLOCK_WATER || blocks[i] != E_BLOCK_LAVA)
            {
                return false;
            }
        }

        // Play warp sound
        m_World->BroadcastSoundEffect("mob.endermen.portal", currPos.x, currPos.y, currPos.z, 1.0f, 0.8f);

        // Teleport endermen to location
        TeleportToCoords((double)x, (double)y, (double)z);

        return true;
    }

    return false;
}
