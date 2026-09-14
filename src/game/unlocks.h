// What the sandbox is allowed to offer, given the campaign progress in the current save slot.
//
// Unlock order: 9x9 vs EASY after mission 7, NORMAL after 11, 13x13 after 15,
// HARD after 17, 19x19 and MASTER after 20. Holding SELECT on the title unlocks everything for
// testing; the debug flag lives in the save file so a test can set it once.
#pragma once

#include <cstdint>

#include "ai/ai.h"
#include "game/missions.h"
#include "game/settings.h"

namespace unlocks {

[[nodiscard]] inline bool has(int unlock_id)
{
    return (settings::slot().unlocks & (1u << unlock_id)) != 0;
}

inline void grant(int unlock_id)
{
    if(unlock_id != campaign::UNLOCK_NONE)
    {
        settings::slot().unlocks |= (1u << unlock_id);
        settings::touch();
    }
}

inline void grant_all()
{
    settings::slot().unlocks = 0xFFFFFFFF;
    settings::touch();
}

// The board sizes the sandbox offers: 7x7 and 9x9 from the start.
[[nodiscard]] inline bool board_size_available(int size)
{
    if(size <= 9) return true;
    if(size == 13) return has(campaign::UNLOCK_13);
    return has(campaign::UNLOCK_19_MASTER);
}

[[nodiscard]] inline bool level_available(ai::Level level)
{
    switch(level)
    {
    case ai::Level::VERY_EASY: return true;
    case ai::Level::EASY:      return has(campaign::UNLOCK_SANDBOX_9_EASY);
    case ai::Level::NORMAL:    return has(campaign::UNLOCK_NORMAL);
    case ai::Level::HARD:      return has(campaign::UNLOCK_HARD);
    default:                   return has(campaign::UNLOCK_19_MASTER);
    }
}

// Mission n is playable once mission n-1 is cleared.
[[nodiscard]] inline bool mission_available(int index)
{
    return index <= settings::slot().cleared_count;
}

[[nodiscard]] inline int mission_rank(int index)
{
    return settings::slot().ranks[index];
}

[[nodiscard]] inline bool mission_starred(int index)
{
    return (settings::slot().stars_mask & (1u << index)) != 0;
}

// Records a cleared mission: the rank never goes down, and the campaign only ever moves forward.
inline void record_clear(int index, int rank, bool starred, int unlock_id)
{
    save::Slot& slot = settings::slot();
    slot.used = 1;

    if(rank > slot.ranks[index])
    {
        slot.ranks[index] = uint8_t(rank);
    }

    if(starred)
    {
        slot.stars_mask |= (1u << index);
    }

    if(index + 1 > slot.cleared_count)
    {
        slot.cleared_count = uint8_t(index + 1);
    }

    grant(unlock_id);
    settings::touch();
}

}  // namespace unlocks
