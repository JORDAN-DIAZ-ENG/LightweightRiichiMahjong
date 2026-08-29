#include "Observation.h"

#include "Shanten.h"

namespace LRMahjong::Model
{
	// -------------------------------------------------------------------
	// Events
	// -------------------------------------------------------------------

	Event Event::Draw( const uint8_t seat, const TileInstance t, const Conf c )
	{
		Event e;
		e.type  = EventType::DRAW;
		e.actor = seat;
		e.tile  = t;
		e.conf  = c;
		e.tileBelief = IsValidInstance( t ) ? TileBelief::Exactly( InstanceTile( t ) ) : TileBelief::Any();
		return e;
	}

	Event Event::Discard( const uint8_t seat, const TileInstance t, const Conf c )
	{
		Event e;
		e.type  = EventType::DISCARD;
		e.actor = seat;
		e.tile  = t;
		e.conf  = c;
		e.tileBelief = IsValidInstance( t ) ? TileBelief::Exactly( InstanceTile( t ) ) : TileBelief::Any();
		return e;
	}

	Event Event::Call( const uint8_t seat, const MeldType type, const TileId base,
		const TileInstance called, const uint8_t from, const AkaMask aka, const Conf c )
	{
		Event e;
		e.type     = EventType::CALL;
		e.actor    = seat;
		e.meldType = type;
		e.base     = base;
		e.tile     = called;
		e.target   = from;
		e.meldAka  = aka;
		e.conf     = c;
		return e;
	}

	Event Event::Ankan( const uint8_t seat, const TileId base, const AkaMask aka, const Conf c )
	{
		Event e;
		e.type     = EventType::ANKAN;
		e.actor    = seat;
		e.meldType = MeldType::ANKAN;
		e.base     = base;
		e.meldAka  = aka;
		e.conf     = c;
		return e;
	}

	Event Event::Kita( const uint8_t seat, const Conf c )
	{
		Event e;
		e.type  = EventType::KITA;
		e.actor = seat;
		e.conf  = c;
		return e;
	}

	Event Event::Riichi( const uint8_t seat, const Conf c )
	{
		Event e;
		e.type  = EventType::RIICHI;
		e.actor = seat;
		e.conf  = c;
		return e;
	}

	Event Event::DoraFlip( const TileInstance indicator, const Conf c )
	{
		Event e;
		e.type = EventType::DORA_FLIP;
		e.tile = indicator;
		e.conf = c;
		return e;
	}

	bool Observation::Append( const Event &e )
	{
		if ( eventCount >= MAX_EVENTS ) return false;
		events[eventCount++] = e;
		return true;
	}

	void Observation::Clear()
	{
		eventCount  = 0;
		startingHand.fill( 0 );
		startingAka = AKA_NONE;
	}

	// -------------------------------------------------------------------
	// Belief
	// -------------------------------------------------------------------

	namespace
	{
		// Removes one copy from the unseen pool, and tightens every seat's
		// upper bound to what the pool can still supply.
		void ConsumeUnseen( Belief &belief, const TileId t )
		{
			if ( !IsValidTile( t ) ) return;
			if ( belief.unseen[t] > 0 ) --belief.unseen[t];
		}

		void ClampMaxHeldToPool( Belief &belief )
		{
			for ( uint8_t seat = 0; seat < belief.rules.numPlayers; ++seat )
			{
				if ( seat == belief.viewer ) continue;

				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( belief.hands[seat].maxHeld[t] > belief.unseen[t] )
					{
						belief.hands[seat].maxHeld[t] = belief.unseen[t];
					}
				}
			}
		}

		uint8_t TotalOf( const Counts34 &counts )
		{
			uint16_t total = 0;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) total = static_cast<uint16_t>( total + counts[t] );
			return static_cast<uint8_t>( total );
		}
	}

	void Belief::Reset( const Observation &obs )
	{
		*this = Belief{};

		rules  = obs.rules;
		viewer = obs.viewer;

		publicState.Reset( obs.rules, obs.dealer );
		publicState.roundWind    = obs.roundWind;
		publicState.honba        = obs.honba;
		publicState.riichiSticks = obs.riichiSticks;
		publicState.phase        = Phase::DISCARD;
		publicState.currentPlayer = obs.dealer;

		// The wall is filled in at determinization time; only its bookkeeping
		// is known up front.
		publicState.liveWallHead = static_cast<uint8_t>( rules.numPlayers * STARTING_HAND_SIZE );

		// Every copy is unseen until something accounts for it.
		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) unseen[t] = rules.CopiesOf( t );

		for ( uint8_t seat = 0; seat < rules.numPlayers; ++seat )
		{
			hands[seat].size = STARTING_HAND_SIZE;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) hands[seat].maxHeld[t] = rules.CopiesOf( t );
		}

		// The viewer's own hand is not a guess.
		if ( viewer < rules.numPlayers )
		{
			Hand &own = publicState.players[viewer].hand;
			own.Clear();

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const AkaMask bit = AkaBitFor( t );
				const bool holdsAka = bit != AKA_NONE && ( obs.startingAka & bit ) != 0;

				for ( uint8_t i = 0; i < obs.startingHand[t]; ++i )
				{
					own.Add( t, holdsAka && i == 0 );
					ConsumeUnseen( *this, t );
				}
			}

			hands[viewer].known = obs.startingHand;
			hands[viewer].size  = TotalOf( obs.startingHand );
		}

		ClampMaxHeldToPool( *this );
		consistent = true;
	}

	void Belief::Apply( const Event &e )
	{
		if ( e.type == EventType::NONE ) return;
		if ( e.conf == Conf::UNKNOWN )   return;

		const uint8_t seat = e.actor;
		const bool    isViewer = seat == viewer;

		switch ( e.type )
		{
		case EventType::DRAW:
		{
			if ( seat >= rules.numPlayers ) break;

			publicState.currentPlayer = seat;
			publicState.players[seat].awaitingDiscard = true;
			++hands[seat].size;

			if ( publicState.liveWallHead < publicState.liveWallTail ) ++publicState.liveWallHead;

			// Temporary furiten lifts on your own draw.
			publicState.players[seat].furitenTemporary = false;

			const TileId t = InstanceTile( e.tile );
			if ( !IsValidTile( t ) ) break;

			// Only the viewer's own draws are identified.
			if ( isViewer )
			{
				publicState.players[seat].hand.Add( t, InstanceIsAka( e.tile ) );
				publicState.players[seat].drawn = e.tile;
				++hands[seat].known[t];
				ConsumeUnseen( *this, t );
			}
			break;
		}

		case EventType::DISCARD:
		{
			if ( seat >= rules.numPlayers ) break;

			Player &p = publicState.players[seat];
			const TileId t = InstanceTile( e.tile );

			if ( p.discardCount < MAX_DISCARDS && IsValidTile( t ) )
			{
				p.discards[p.discardCount] = e.tile;
				p.discardFlags[p.discardCount] = DISCARD_NONE;
				++p.discardCount;
			}

			p.awaitingDiscard = false;
			p.drawn = INVALID_INSTANCE;

			publicState.lastDiscard   = e.tile;
			publicState.lastDiscarder = seat;

			if ( hands[seat].size > 0 ) --hands[seat].size;

			if ( !IsValidTile( t ) ) break;

			if ( isViewer )
			{
				p.hand.Remove( t, InstanceIsAka( e.tile ) );
				if ( hands[seat].known[t] > 0 ) --hands[seat].known[t];
			}
			else
			{
				// A discard is a tile leaving the unseen pool.
				ConsumeUnseen( *this, t );
			}

			ClampMaxHeldToPool( *this );
			break;
		}

		case EventType::CALL:
		{
			if ( seat >= rules.numPlayers ) break;
			if ( publicState.players[seat].meldCount >= MAX_MELDS ) break;

			Player &p = publicState.players[seat];

			Meld &meld = p.melds[p.meldCount++];
			meld.type   = e.meldType;
			meld.base   = e.base;
			meld.called = e.tile;
			meld.aka    = e.meldAka;
			meld.from   = static_cast<CalledFrom>(
				( seat + rules.numPlayers - ( e.target < rules.numPlayers ? e.target : seat ) ) % rules.numPlayers );

			// The claimed tile came out of the discarder's pond.
			if ( e.target < rules.numPlayers )
			{
				Player &discarder = publicState.players[e.target];
				if ( discarder.discardCount > 0 )
				{
					const uint8_t index = static_cast<uint8_t>( discarder.discardCount - 1 );
					discarder.discardFlags[index] = static_cast<DiscardFlags>( discarder.discardFlags[index] | DISCARD_CALLED );
				}
			}

			// The tiles the caller supplied from hand come off the pool, and the
			// seat owes a discard.
			const uint8_t supplied = static_cast<uint8_t>( meld.TileCount() - 1 );

			Counts34 meldTiles{};
			meld.AddTo( meldTiles );

			const TileId called = InstanceTile( e.tile );
			if ( IsValidTile( called ) && meldTiles[called] > 0 ) --meldTiles[called];

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				for ( uint8_t i = 0; i < meldTiles[t]; ++i )
				{
					if ( isViewer ) publicState.players[seat].hand.Remove( t, false );
					else            ConsumeUnseen( *this, t );
				}
			}

			hands[seat].size = static_cast<uint8_t>( hands[seat].size > supplied ? hands[seat].size - supplied : 0 );

			// A called kan draws a replacement, so the size comes straight back.
			if ( meld.IsKan() ) ++hands[seat].size;

			p.awaitingDiscard = true;
			publicState.currentPlayer = seat;
			publicState.firstGoAround = false;

			for ( uint8_t s = 0; s < rules.numPlayers; ++s ) publicState.players[s].ippatsu = false;

			ClampMaxHeldToPool( *this );
			break;
		}

		case EventType::ANKAN:
		{
			if ( seat >= rules.numPlayers ) break;
			if ( publicState.players[seat].meldCount >= MAX_MELDS ) break;

			Player &p = publicState.players[seat];

			Meld &meld = p.melds[p.meldCount++];
			meld.type = MeldType::ANKAN;
			meld.base = e.base;
			meld.from = CalledFrom::SELF;
			meld.aka  = e.meldAka;

			// A concealed kan is face up, so all four copies become visible.
			for ( uint8_t i = 0; i < 4; ++i )
			{
				if ( isViewer ) p.hand.Remove( e.base, false );
				else            ConsumeUnseen( *this, e.base );
			}

			// Four out of hand, one replacement in.
			hands[seat].size = static_cast<uint8_t>( hands[seat].size >= 4 ? hands[seat].size - 3 : 1 );

			p.awaitingDiscard = true;
			publicState.currentPlayer = seat;

			if ( publicState.deadWallDraws < 4 ) ++publicState.deadWallDraws;
			if ( publicState.liveWallTail > publicState.liveWallHead ) --publicState.liveWallTail;

			ClampMaxHeldToPool( *this );
			break;
		}

		case EventType::KITA:
		{
			if ( seat >= rules.numPlayers ) break;

			++publicState.players[seat].nukiCount;

			const TileId north = Id( RiichiMahjongTile::NORTH );
			if ( isViewer ) publicState.players[seat].hand.Remove( north, false );
			else            ConsumeUnseen( *this, north );

			publicState.players[seat].awaitingDiscard = true;
			publicState.currentPlayer = seat;

			if ( publicState.deadWallDraws < 4 ) ++publicState.deadWallDraws;
			if ( publicState.liveWallTail > publicState.liveWallHead ) --publicState.liveWallTail;

			ClampMaxHeldToPool( *this );
			break;
		}

		case EventType::RIICHI:
		{
			if ( seat >= rules.numPlayers ) break;

			Player &p = publicState.players[seat];
			p.riichiDeclared = true;
			p.ippatsu        = true;
			p.points        -= 1000;
			++publicState.riichiSticks;
			break;
		}

		case EventType::DORA_FLIP:
		{
			const TileId t = InstanceTile( e.tile );
			if ( !IsValidTile( t ) ) break;

			const uint8_t slot = publicState.doraIndicators;
			if ( slot >= MAX_DORA_INDICATORS ) break;

			publicState.wall[publicState.DeadWallStart() + slot] = e.tile;
			publicState.RevealDoraIndicator();

			ConsumeUnseen( *this, t );
			ClampMaxHeldToPool( *this );
			break;
		}

		case EventType::TSUMO:
		case EventType::RON:
		case EventType::ABORT:
			publicState.phase = Phase::HAND_OVER;
			break;

		default:
			break;
		}
	}

	void Belief::BuildFrom( const Observation &obs )
	{
		Reset( obs );

		for ( uint16_t i = 0; i < obs.eventCount; ++i ) Apply( obs.events[i] );
	}

	// -------------------------------------------------------------------
	// Reconciliation
	// -------------------------------------------------------------------

	namespace
	{
		// Counts every tile the observation claims to have seen, and checks no
		// kind was seen more often than it exists.
		bool IsReachable( const Observation &obs )
		{
			Counts34 seen{};

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) seen[t] = obs.startingHand[t];

			uint8_t meldCount[MAX_PLAYERS]{};

			for ( uint16_t i = 0; i < obs.eventCount; ++i )
			{
				const Event &e = obs.events[i];
				if ( e.conf == Conf::UNKNOWN ) continue;

				switch ( e.type )
				{
				case EventType::DISCARD:
				case EventType::DORA_FLIP:
				{
					const TileId t = InstanceTile( e.tile );
					if ( IsValidTile( t ) ) ++seen[t];
					break;
				}

				case EventType::DRAW:
				{
					// Only an identified draw adds a tile; an opponent's draw is
					// already counted in the pool.
					if ( e.actor == obs.viewer )
					{
						const TileId t = InstanceTile( e.tile );
						if ( IsValidTile( t ) ) ++seen[t];
					}
					break;
				}

				case EventType::CALL:
				case EventType::ANKAN:
				{
					if ( e.actor >= obs.rules.numPlayers ) return false;
					if ( ++meldCount[e.actor] > MAX_MELDS ) return false;

					Meld meld;
					meld.type = e.type == EventType::ANKAN ? MeldType::ANKAN : e.meldType;
					meld.base = e.base;

					Counts34 tiles{};
					meld.AddTo( tiles );

					// The called tile was already counted as a discard.
					const TileId called = InstanceTile( e.tile );
					if ( e.type != EventType::ANKAN && IsValidTile( called ) && tiles[called] > 0 ) --tiles[called];

					for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) seen[t] = static_cast<uint8_t>( seen[t] + tiles[t] );
					break;
				}

				case EventType::KITA:
					++seen[Id( RiichiMahjongTile::NORTH )];
					break;

				default:
					break;
				}
			}

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( seen[t] > obs.rules.CopiesOf( t ) ) return false;
			}

			return true;
		}
	}

	ReconcileResult Reconcile( Observation &obs, const RepairPolicy policy )
	{
		if ( IsReachable( obs ) ) return ReconcileResult::CONSISTENT;
		if ( policy == RepairPolicy::STRICT ) return ReconcileResult::IMPOSSIBLE;

		// Drop the least trustworthy readings first. Anything CERTAIN is left
		// alone: if the state is still unreachable without any doubtful events,
		// the problem is not one this can fix.
		for ( uint8_t level = static_cast<uint8_t>( Conf::LOW );
			level < static_cast<uint8_t>( Conf::CERTAIN ); ++level )
		{
			bool droppedAny = false;

			for ( uint16_t i = 0; i < obs.eventCount; ++i )
			{
				if ( obs.events[i].conf != static_cast<Conf>( level ) ) continue;

				obs.events[i].conf = Conf::UNKNOWN;
				droppedAny = true;

				if ( IsReachable( obs ) ) return ReconcileResult::REPAIRED;
			}

			( void )droppedAny;
		}

		return ReconcileResult::IMPOSSIBLE;
	}

	// -------------------------------------------------------------------
	// Determinization
	// -------------------------------------------------------------------

	namespace
	{
		// Draws one tile at random from what the pool still allows this seat.
		TileId PickAllowed( const Counts34 &pool, const Counts34 &allowance, Rng &rng )
		{
			uint16_t total = 0;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				total = static_cast<uint16_t>( total + ( pool[t] < allowance[t] ? pool[t] : allowance[t] ) );
			}

			if ( total == 0 ) return INVALID_TILE;

			int pick = static_cast<int>( rng.Below( total ) );

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const int available = pool[t] < allowance[t] ? pool[t] : allowance[t];
				if ( pick < available ) return t;
				pick -= available;
			}

			return INVALID_TILE;
		}

		bool TryDeal( const Belief &belief, Rng &rng, GameState &state, const DeterminizeOptions &options )
		{
			Counts34 pool = belief.unseen;

			for ( uint8_t seat = 0; seat < belief.rules.numPlayers; ++seat )
			{
				if ( seat == belief.viewer ) continue;

				const HandBelief &hb = belief.hands[seat];

				Player &p = state.players[seat];
				p.hand.Clear();

				Counts34 allowance = hb.maxHeld;

				// Anything already confirmed goes in first and is not sampled.
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					for ( uint8_t i = 0; i < hb.known[t]; ++i )
					{
						if ( pool[t] == 0 ) return false;
						--pool[t];
						p.hand.Add( t, false );
						if ( allowance[t] > 0 ) --allowance[t];
					}
				}

				const uint8_t needed = static_cast<uint8_t>(
					hb.size > p.hand.TotalTiles() ? hb.size - p.hand.TotalTiles() : 0 );

				for ( uint8_t i = 0; i < needed; ++i )
				{
					const TileId t = PickAllowed( pool, allowance, rng );
					if ( !IsValidTile( t ) ) return false;

					--pool[t];
					--allowance[t];
					p.hand.Add( t, false );
				}

				// A seat that declared riichi was tenpai when it did, so a hand
				// that is not tenpai is a world that cannot have happened.
				if ( options.respectRiichiTenpai && p.riichiDeclared )
				{
					const uint8_t resting = static_cast<uint8_t>( 13 - 3 * p.meldCount );
					const Counts34 &counts = p.hand.Counts();

					Counts34 test = counts;
					if ( p.hand.TotalTiles() == resting + 1 )
					{
						// Holding a draw: drop one tile before asking.
						for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
						{
							if ( test[t] > 0 ) { --test[t]; break; }
						}
					}

					if ( Shanten( test, p.meldCount ) != 0 ) return false;
				}
			}

			// Whatever is left is the wall. The dora indicators already sit in
			// their slots and must not be overwritten.
			TileInstance remaining[MAX_WALL_TILES];
			uint16_t count = 0;

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				for ( uint8_t i = 0; i < pool[t]; ++i ) remaining[count++] = MakeInstance( t, false );
			}

			rng.Shuffle( remaining, remaining + count );

			const uint8_t deadWallStart = state.DeadWallStart();
			uint16_t next = 0;

			for ( uint16_t slot = 0; slot < state.wallCount; ++slot )
			{
				const bool isRevealedIndicator =
					slot >= deadWallStart &&
					slot < static_cast<uint16_t>( deadWallStart + state.doraIndicators );

				if ( isRevealedIndicator ) continue;
				if ( next >= count ) break;

				state.wall[slot] = remaining[next++];
			}

			return true;
		}
	}

	bool Determinize( const Belief &belief, Rng &rng, GameState &outState,
		const DeterminizeOptions &options )
	{
		if ( !belief.consistent ) return false;

		for ( uint16_t attempt = 0; attempt < options.maxAttempts; ++attempt )
		{
			outState = belief.publicState;
			if ( TryDeal( belief, rng, outState, options ) ) return true;
		}

		// The riichi constraint is the one most likely to be unsatisfiable from
		// a thin pool; a world without it beats no world at all.
		if ( options.respectRiichiTenpai )
		{
			DeterminizeOptions relaxed = options;
			relaxed.respectRiichiTenpai = false;

			for ( uint16_t attempt = 0; attempt < relaxed.maxAttempts; ++attempt )
			{
				outState = belief.publicState;
				if ( TryDeal( belief, rng, outState, relaxed ) ) return true;
			}
		}

		return false;
	}

} // namespace LRMahjong::Model
