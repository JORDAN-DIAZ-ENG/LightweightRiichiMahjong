#include "Player.h"

namespace LRMahjong::Model
{
	void Player::AddRevealedTo( Counts34 &counts ) const
	{
		for ( uint8_t i = 0; i < meldCount; ++i )
		{
			melds[i].AddTo( counts );
		}

		for ( uint8_t i = 0; i < discardCount; ++i )
		{
			// A called tile now lives in the caller's meld and is counted there.
			if ( ( discardFlags[i] & DISCARD_CALLED ) != 0 ) continue;

			const TileId t = InstanceTile( discards[i] );
			if ( IsValidTile( t ) ) ++counts[t];
		}

		if ( nukiCount > 0 )
		{
			const TileId north = Id( RiichiMahjongTile::NORTH );
			counts[north] = static_cast<uint8_t>( counts[north] + nukiCount );
		}
	}

} // namespace LRMahjong::Model
