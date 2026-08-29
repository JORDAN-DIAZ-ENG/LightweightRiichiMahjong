#include "LegalActions.h"

#include "Shanten.h"
#include "WinCheck.h"

namespace LRMahjong::Model
{
	namespace
	{
		constexpr uint64_t Bit( const TileId t ) { return 1ULL << t; }

		// The concealed hand as it would be after removing one tile.
		Counts34 WithoutTile( const Counts34 &counts, const TileId t )
		{
			Counts34 work = counts;
			if ( work[t] > 0 ) --work[t];
			return work;
		}

		// A call that would leave the hand with nothing it is allowed to
		// discard is not a legal call. Rare in practice, but a search that hit
		// it would deadlock on a state with no legal actions.
		bool HasLegalDiscard( const Counts34 &after, const uint64_t forbidden )
		{
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( after[t] > 0 && ( forbidden & Bit( t ) ) == 0 ) return true;
			}
			return false;
		}

		// Emits every way of discarding tile `t`, which is two distinct choices
		// when the hand holds both the red five and an ordinary one.
		void AddDiscardVariants( ActionList &out, const uint8_t seat, const Hand &hand,
			const TileId t, const bool asRiichi )
		{
			const AkaMask bit    = AkaBitFor( t );
			const bool    hasAka = bit != AKA_NONE && ( hand.Aka() & bit ) != 0;
			const uint8_t copies = hand.Count( t );

			// The ordinary copy only exists if the hand holds more than the red one.
			if ( !hasAka || copies > 1 )
			{
				const Action a = asRiichi
					? Action::Riichi( seat, MakeInstance( t, false ) )
					: Action::Discard( seat, MakeInstance( t, false ) );
				out.Add( a );
			}

			if ( hasAka )
			{
				const Action a = asRiichi
					? Action::Riichi( seat, MakeInstance( t, true ) )
					: Action::Discard( seat, MakeInstance( t, true ) );
				out.Add( a );
			}
		}

		// A concealed kan during riichi is only allowed when it cannot change
		// what the hand is waiting on, and when the kan tile is the one just
		// drawn.
		bool AnkanKeepsRiichiWait( const GameState &state, const uint8_t seat, const TileId t )
		{
			const Player &p = state.players[seat];

			if ( InstanceTile( p.drawn ) != t ) return false;

			// Waits before: the hand minus the drawn tile.
			const Counts34 before = WithoutTile( p.hand.Counts(), t );
			const uint64_t waitsBefore = WaitingTiles( before, p.meldCount );

			// Waits after: the four copies leave for a meld.
			Counts34 after = p.hand.Counts();
			after[t] = 0;
			const uint64_t waitsAfter = WaitingTiles( after, static_cast<uint8_t>( p.meldCount + 1 ) );

			return waitsBefore != 0 && waitsBefore == waitsAfter;
		}

		void GenerateTurnActions( const GameState &state, const uint8_t seat, ActionList &out )
		{
			const Player  &p      = state.players[seat];
			const Counts34 &counts = p.hand.Counts();

			// ---- winning ----------------------------------------------
			// Shape only; whether a yaku is present is M4's question.
			if ( IsWinningHand( counts, p.meldCount ) )
			{
				out.Add( Action::Tsumo( seat ) );
			}

			// ---- abortive draw ----------------------------------------
			if ( state.firstGoAround )
			{
				uint8_t terminalKinds = 0;
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( counts[t] > 0 && IsTerminalOrHonor( t ) ) ++terminalKinds;
				}
				if ( terminalKinds >= 9 ) out.Add( Action::Kyuushu( seat ) );
			}

			// ---- discards ---------------------------------------------
			const bool riichiLocked = p.riichiDeclared;

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( counts[t] == 0 ) continue;

				// Kuikae: the tile just claimed, and the far end of a chi.
				if ( ( state.forbiddenDiscards & Bit( t ) ) != 0 && !state.rules.kuikae ) continue;

				// A declared hand is frozen apart from the tile it just drew.
				if ( riichiLocked && t != InstanceTile( p.drawn ) ) continue;

				AddDiscardVariants( out, seat, p.hand, t, false );
			}

			// ---- riichi -----------------------------------------------
			const bool canDeclare =
				!p.riichiDeclared &&
				p.IsMenzen() &&
				p.points >= 1000 &&
				state.LiveWallRemaining() >= 4 &&
				// One shanten call settles whether any discard could leave the
				// hand tenpai. Without this the loop below runs a tenpai test
				// per candidate discard even for hands nowhere near ready,
				// which dominated the cost of action generation.
				Shanten( counts, p.meldCount ) <= 0;

			if ( canDeclare )
			{
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( counts[t] == 0 ) continue;
					if ( ( state.forbiddenDiscards & Bit( t ) ) != 0 && !state.rules.kuikae ) continue;

					// Declaring requires the hand to be tenpai once the tile is gone.
					if ( Shanten( WithoutTile( counts, t ), p.meldCount ) != 0 ) continue;

					AddDiscardVariants( out, seat, p.hand, t, true );
				}
			}

			// ---- kans and kita ----------------------------------------
			const bool replacementsLeft = state.deadWallDraws < 4 && !state.LiveWallEmpty();

			if ( replacementsLeft && p.meldCount < MAX_MELDS )
			{
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( counts[t] < 4 ) continue;
					if ( riichiLocked && !AnkanKeepsRiichiWait( state, seat, t ) ) continue;

					out.Add( Action::Ankan( seat, t ) );
				}
			}

			if ( replacementsLeft && !riichiLocked )
			{
				for ( uint8_t i = 0; i < p.meldCount; ++i )
				{
					const Meld &meld = p.melds[i];
					if ( meld.type != MeldType::PON ) continue;
					if ( counts[meld.base] == 0 )     continue;

					out.Add( Action::Shouminkan( seat, meld.base ) );
				}
			}

			if ( replacementsLeft && state.rules.nukidora )
			{
				const TileId north = Id( RiichiMahjongTile::NORTH );
				if ( counts[north] > 0 ) out.Add( Action::Kita( seat ) );
			}
		}

		void GenerateCallActions( const GameState &state, const uint8_t seat, ActionList &out )
		{
			const Player &p      = state.players[seat];
			const TileId  called = InstanceTile( state.lastDiscard );

			out.Add( Action::Pass( seat ) );

			if ( !IsValidTile( called ) ) return;

			// ---- ron --------------------------------------------------
			Counts34 withWinning = p.hand.Counts();
			if ( withWinning[called] < 4 )
			{
				++withWinning[called];
				if ( IsWinningHand( withWinning, p.meldCount ) && !IsFuriten( state, seat ) )
				{
					out.Add( Action::Ron( seat ) );
				}
			}

			// Robbing a kan is a ron and nothing else; no meld may be formed
			// from a tile that is already part of one.
			if ( state.awaitingChankan ) return;

			// A declared hand cannot call.
			if ( p.riichiDeclared )        return;
			if ( p.meldCount >= MAX_MELDS ) return;

			const uint8_t copies = p.hand.Count( called );
			const AkaMask akaBit = AkaBitFor( called );
			const bool    hasAka = akaBit != AKA_NONE && ( p.hand.Aka() & akaBit ) != 0;

			// ---- pon --------------------------------------------------
			if ( copies >= 2 )
			{
				Counts34 afterPon = p.hand.Counts();
				afterPon[called] = static_cast<uint8_t>( afterPon[called] - 2 );

				const uint64_t forbidden = state.rules.kuikae ? 0 : KuikaeMask( MeldType::PON, called, called );

				if ( HasLegalDiscard( afterPon, forbidden ) )
				{
					// Whether to feed the red copy into the meld is a real
					// choice whenever the hand holds it alongside an ordinary one.
					if ( !hasAka || copies > 2 ) out.Add( Action::Pon( seat, called, AKA_NONE ) );
					if ( hasAka )                out.Add( Action::Pon( seat, called, akaBit ) );
				}
			}

			// ---- daiminkan --------------------------------------------
			if ( copies >= 3 && state.deadWallDraws < 4 && !state.LiveWallEmpty() )
			{
				out.Add( Action::Daiminkan( seat, called, hasAka ? akaBit : AKA_NONE ) );
			}

			// ---- chi --------------------------------------------------
			if ( !state.rules.allowChi )                        return;
			if ( seat != state.NextSeat( state.lastDiscarder ) ) return;
			if ( !IsSuited( called ) )                          return;

			for ( int offset = 0; offset < 3; ++offset )
			{
				const int baseValue = static_cast<int>( called ) - offset;
				if ( baseValue < 0 ) continue;

				const TileId base = static_cast<TileId>( baseValue );
				if ( !IsSuited( base ) || RankOf( base ) > 7 ) continue;
				if ( SuitOf( base ) != SuitOf( called ) )      continue;

				// The two tiles the hand has to supply.
				TileId  needed[2];
				uint8_t neededCount = 0;
				for ( TileId t = base; t <= base + 2; ++t )
				{
					if ( t != called ) needed[neededCount++] = t;
				}

				if ( p.hand.Count( needed[0] ) == 0 || p.hand.Count( needed[1] ) == 0 ) continue;

				Counts34 afterChi = p.hand.Counts();
				--afterChi[needed[0]];
				--afterChi[needed[1]];

				const uint64_t forbidden = state.rules.kuikae ? 0 : KuikaeMask( MeldType::CHI, base, called );
				if ( !HasLegalDiscard( afterChi, forbidden ) ) continue;

				// Which red fives the two supplied tiles could carry.
				AkaMask available = AKA_NONE;
				for ( uint8_t i = 0; i < neededCount; ++i )
				{
					const AkaMask bit = AkaBitFor( needed[i] );
					if ( bit != AKA_NONE && ( p.hand.Aka() & bit ) != 0 &&
						p.hand.Count( needed[i] ) >= 1 )
					{
						available = static_cast<AkaMask>( available | bit );
					}
				}

				out.Add( Action::Chi( seat, base, AKA_NONE ) );

				// Only offer the red variant when an ordinary copy also exists,
				// otherwise the plain form above already means the red tile.
				if ( available != AKA_NONE )
				{
					bool ordinaryExists = false;
					for ( uint8_t i = 0; i < neededCount; ++i )
					{
						const AkaMask bit = AkaBitFor( needed[i] );
						if ( ( available & bit ) != 0 && p.hand.Count( needed[i] ) > 1 ) ordinaryExists = true;
					}
					if ( ordinaryExists ) out.Add( Action::Chi( seat, base, available ) );
				}
			}
		}
	}

	uint64_t KuikaeMask( const MeldType type, const TileId base, const TileId called )
	{
		uint64_t mask = Bit( called );

		if ( type == MeldType::CHI && IsSuited( base ) )
		{
			// Taking the bottom of a run forbids the tile above it, and taking
			// the top forbids the tile below: either would be the same wait
			// swapped for the tile just claimed.
			if ( called == base && RankOf( base ) <= 6 )
			{
				mask |= Bit( static_cast<TileId>( base + 3 ) );
			}
			else if ( called == base + 2 && RankOf( base ) >= 2 )
			{
				mask |= Bit( static_cast<TileId>( base - 1 ) );
			}
		}

		return mask;
	}

	uint64_t SeatWaits( const GameState &state, const uint8_t seat )
	{
		if ( seat >= state.rules.numPlayers ) return 0;

		const Player &p = state.players[seat];

		// A seat holding a drawn tile has fourteen; the wait is a property of
		// the thirteen underneath it, so this only answers for a resting hand.
		return WaitingTiles( p.hand.Counts(), p.meldCount );
	}

	bool IsFuriten( const GameState &state, const uint8_t seat )
	{
		if ( seat >= state.rules.numPlayers ) return false;

		const Player &p = state.players[seat];

		if ( p.furitenPermanent ) return true;
		if ( p.furitenTemporary ) return true;

		// The wait is computed from the thirteen tiles the hand rests on. When
		// the seat is holding a draw, drop it first.
		Counts34 resting = p.hand.Counts();
		const uint8_t expected = static_cast<uint8_t>( 13 - 3 * p.meldCount );

		uint8_t total = 0;
		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) total = static_cast<uint8_t>( total + resting[t] );

		if ( total == expected + 1 )
		{
			const TileId drawn = InstanceTile( p.drawn );
			if ( IsValidTile( drawn ) && resting[drawn] > 0 ) --resting[drawn];
			else return false;
		}
		else if ( total != expected )
		{
			return false;
		}

		// Only a tenpai hand has a wait to be furiten against, and settling that
		// with one shanten call is far cheaper than enumerating the waits of a
		// hand that has none.
		if ( Shanten( resting, p.meldCount ) != 0 ) return false;

		const uint64_t waits = WaitingTiles( resting, p.meldCount );
		if ( waits == 0 ) return false;

		// Any waited-on tile sitting in the seat's own pond blocks ron.
		for ( uint8_t i = 0; i < p.discardCount; ++i )
		{
			const TileId t = InstanceTile( p.discards[i] );
			if ( IsValidTile( t ) && ( waits & Bit( t ) ) != 0 ) return true;
		}

		return false;
	}

	uint8_t LegalActions( const GameState &state, const uint8_t seat, ActionList &out )
	{
		out.Clear();

		if ( seat >= state.rules.numPlayers ) return 0;

		switch ( state.phase )
		{
		case Phase::DISCARD:
			if ( seat == state.currentPlayer ) GenerateTurnActions( state, seat, out );
			break;

		case Phase::CALL:
			if ( ( state.pendingCallers & ( 1u << seat ) ) != 0 ) GenerateCallActions( state, seat, out );
			break;

		default:
			break;
		}

		return out.count;
	}

} // namespace LRMahjong::Model
