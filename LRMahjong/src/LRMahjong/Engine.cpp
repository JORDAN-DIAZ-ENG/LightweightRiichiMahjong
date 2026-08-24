#include "Engine.h"

#include "Model/WinCheck.h"

namespace LRMahjong
{
	using namespace Model;

	Engine::Engine( const Rules &rules, const uint64_t seed )
		: _rules( rules )
		, _rng( seed )
		, _seed( seed )
	{
		_state.Reset( _rules, 0 );
	}

	void Engine::Reset( const uint64_t seed )
	{
		_seed = seed;
		_rng.Seed( seed );
		_state.Reset( _rules, 0 );
	}

	// -------------------------------------------------------------------
	// Setup
	// -------------------------------------------------------------------

	void Engine::BuildWall()
	{
		uint8_t written = 0;

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			const uint8_t copies = _rules.CopiesOf( t );

			// How many copies of this kind are red. Hand carries one aka bit
			// per suit, so at most one red five per suit is representable; a
			// ruleset asking for more is clamped rather than silently losing
			// track of which copy is which.
			uint8_t akaCopies = 0;
			if ( t == MAN_5_ID )      akaCopies = _rules.akaPerSuit[0];
			else if ( t == PIN_5_ID ) akaCopies = _rules.akaPerSuit[1];
			else if ( t == SOU_5_ID ) akaCopies = _rules.akaPerSuit[2];
			if ( akaCopies > 1 ) akaCopies = 1;

			for ( uint8_t copy = 0; copy < copies; ++copy )
			{
				_state.wall[written++] = MakeInstance( t, copy < akaCopies );
			}
		}

		_state.wallCount    = written;
		_state.liveWallHead = 0;
		_state.liveWallTail = static_cast<uint8_t>( written - DEAD_WALL_TILES );

		_rng.Shuffle( _state.wall, _state.wall + written );
	}

	void Engine::DealHands()
	{
		// A real deal takes blocks of four; from a uniformly shuffled wall a
		// sequential deal is the same distribution. Replaying a specific
		// recorded game goes through the observation log, not through a seed,
		// so the block order never needs to be reproduced.
		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			for ( uint8_t i = 0; i < STARTING_HAND_SIZE; ++i )
			{
				const TileInstance tile = _state.DrawLive();
				_state.players[seat].hand.Add( InstanceTile( tile ), InstanceIsAka( tile ) );
			}
		}
	}

	void Engine::StartHand( const uint8_t dealer )
	{
		_state.Reset( _rules, dealer );
		BuildWall();
		DealHands();
		_state.RevealDoraIndicator();

		_state.currentPlayer = dealer;
		GiveDrawTo( dealer, false );
		_state.phase = Phase::DISCARD;
	}

	bool Engine::GiveDrawTo( const uint8_t seat, const bool fromDeadWall )
	{
		const TileInstance tile = fromDeadWall ? _state.DrawReplacement() : _state.DrawLive();
		if ( !IsValidInstance( tile ) ) return false;

		Player &p = _state.players[seat];
		p.hand.Add( InstanceTile( tile ), InstanceIsAka( tile ) );
		p.drawn           = tile;
		p.awaitingDiscard = true;

		// Temporary furiten lifts the moment you draw again.
		p.furitenTemporary = false;

		_state.drewFromDeadWall = fromDeadWall;
		return true;
	}

	// -------------------------------------------------------------------
	// Step
	// -------------------------------------------------------------------

	StepResult Engine::Step( const Action &action )
	{
		if ( _state.phase == Phase::HAND_OVER )  return StepResult::ILLEGAL;
		if ( action.actor >= _rules.numPlayers ) return StepResult::ILLEGAL;

		// One source of truth for legality. Anything the generator does not
		// offer is refused here, which keeps the two from drifting apart.
		ActionList legal;
		LegalActions( _state, action.actor, legal );
		if ( !legal.Contains( action ) ) return StepResult::ILLEGAL;

		switch ( _state.phase )
		{
		case Phase::DISCARD: return StepDiscardPhase( action );
		case Phase::CALL:    return StepCallPhase( action );
		default:             return StepResult::ILLEGAL;
		}
	}

	StepResult Engine::StepDiscardPhase( const Action &action )
	{
		switch ( action.type )
		{
		case ActionType::DISCARD:
		case ActionType::RIICHI:
			return ApplyDiscard( action );

		case ActionType::ANKAN:      return ApplyAnkan( action );
		case ActionType::SHOUMINKAN: return ApplyShouminkan( action );
		case ActionType::KITA:       return ApplyKita( action );

		case ActionType::TSUMO:
			EndHand( HandOutcome::TSUMO, action.actor );
			return StepResult::HAND_ENDED;

		case ActionType::KYUUSHU:
			EndHand( HandOutcome::ABORT_KYUUSHU );
			return StepResult::HAND_ENDED;

		default:
			return StepResult::ILLEGAL;
		}
	}

	StepResult Engine::ApplyDiscard( const Action &action )
	{
		Player &p = _state.players[action.actor];

		const TileId t   = InstanceTile( action.tile );
		const bool   aka = InstanceIsAka( action.tile );

		if ( !p.hand.Remove( t, aka ) ) return StepResult::ILLEGAL;

		DiscardFlags flags = ( action.tile == p.drawn ) ? DISCARD_TSUMOGIRI : DISCARD_NONE;

		if ( action.type == ActionType::RIICHI )
		{
			p.riichiDeclared     = true;
			p.doubleRiichi       = _state.firstGoAround;
			p.ippatsu            = true;
			p.riichiDiscardIndex = p.discardCount;
			p.points            -= 1000;
			++_state.riichiSticks;
			flags = static_cast<DiscardFlags>( flags | DISCARD_RIICHI );
		}
		else
		{
			// Your own next discard closes your ippatsu window.
			p.ippatsu = false;
		}

		p.discards[p.discardCount]     = action.tile;
		p.discardFlags[p.discardCount] = flags;
		++p.discardCount;

		p.drawn           = INVALID_INSTANCE;
		p.awaitingDiscard = false;

		_state.lastDiscard       = action.tile;
		_state.lastDiscarder     = action.actor;
		_state.forbiddenDiscards = 0; // the kuikae restriction lasts one discard

		// A called kan's indicator waits for the caller's discard; a concealed
		// kan already flipped its own.
		if ( _state.pendingDoraFlip )
		{
			_state.RevealDoraIndicator();
			_state.pendingDoraFlip = false;
		}

		return OpenCallWindow( action.actor );
	}

	StepResult Engine::ApplyAnkan( const Action &action )
	{
		Player &p = _state.players[action.actor];

		const AkaMask akaBit = AkaBitFor( action.base );
		const bool takesAka  = akaBit != AKA_NONE && ( p.hand.Aka() & akaBit ) != 0;

		for ( uint8_t i = 0; i < 4; ++i )
		{
			p.hand.Remove( action.base, takesAka && i == 0 );
		}

		Meld &meld = p.melds[p.meldCount++];
		meld.type = MeldType::ANKAN;
		meld.base = action.base;
		meld.from = CalledFrom::SELF;
		meld.aka  = takesAka ? akaBit : AKA_NONE;

		// A concealed kan turns its indicator immediately.
		_state.RevealDoraIndicator();

		if ( !GiveDrawTo( action.actor, true ) ) return StepResult::ILLEGAL;

		// Robbing a concealed kan is a rules variant this engine does not
		// implement; only added kans open a window.
		if ( FourKanAbort() )
		{
			EndHand( HandOutcome::ABORT_FOUR_KAN );
			return StepResult::HAND_ENDED;
		}

		return StepResult::OK;
	}

	StepResult Engine::ApplyShouminkan( const Action &action )
	{
		Player &p = _state.players[action.actor];

		Meld *pon = nullptr;
		for ( uint8_t i = 0; i < p.meldCount; ++i )
		{
			if ( p.melds[i].type == MeldType::PON && p.melds[i].base == action.base )
			{
				pon = &p.melds[i];
				break;
			}
		}
		if ( pon == nullptr ) return StepResult::ILLEGAL;

		const AkaMask akaBit = AkaBitFor( action.base );
		const bool takesAka  = akaBit != AKA_NONE && ( p.hand.Aka() & akaBit ) != 0;

		p.hand.Remove( action.base, takesAka );

		pon->type = MeldType::SHOUMINKAN;
		if ( takesAka ) pon->aka = static_cast<AkaMask>( pon->aka | akaBit );

		// The added tile is open to being robbed before the replacement draw
		// happens, so the window opens here and the draw waits for it to close.
		_state.lastDiscard      = MakeInstance( action.base, takesAka );
		_state.lastDiscarder    = action.actor;
		_state.awaitingChankan  = true;

		return OpenCallWindow( action.actor );
	}

	StepResult Engine::ApplyKita( const Action &action )
	{
		Player &p = _state.players[action.actor];

		const TileId north = Id( RiichiMahjongTile::NORTH );
		p.hand.Remove( north );
		++p.nukiCount;

		// Whether a pulled North can be ronned is one of the unverified
		// Mahjong Soul rules; for now it simply leaves play.
		if ( !GiveDrawTo( action.actor, true ) ) return StepResult::ILLEGAL;

		return StepResult::OK;
	}

	// -------------------------------------------------------------------
	// Call phase
	// -------------------------------------------------------------------

	StepResult Engine::OpenCallWindow( const uint8_t excludingSeat )
	{
		_state.phase          = Phase::CALL;
		_state.pendingCallers = OtherSeatsMask( excludingSeat );

		for ( uint8_t seat = 0; seat < MAX_PLAYERS; ++seat )
		{
			_state.pendingResponse[seat] = Action{};
		}

		// Most discards are of no interest to anybody. A seat whose only legal
		// answer is "pass" is not making a decision, so the engine answers for
		// it, the same way it draws for a seat whose turn arrives. Without
		// this, the overwhelming majority of steps in a rollout would be
		// forced passes.
		ActionList legal;
		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			if ( ( _state.pendingCallers & ( 1u << seat ) ) == 0 ) continue;

			LegalActions( _state, seat, legal );
			if ( legal.count == 1 && legal[0].type == ActionType::PASS )
			{
				_state.pendingResponse[seat] = legal[0];
				_state.pendingCallers = static_cast<uint8_t>( _state.pendingCallers & ~( 1u << seat ) );
			}
		}

		if ( _state.pendingCallers == 0 ) return ResolveResponses();
		return StepResult::OK;
	}

	StepResult Engine::StepCallPhase( const Action &action )
	{
		const uint8_t bit = static_cast<uint8_t>( 1u << action.actor );

		// Responses are banked rather than applied, because ron outranks pon
		// and kan, which outrank chi, and that ordering cannot be honoured
		// until every seat has spoken.
		_state.pendingResponse[action.actor] = action;
		_state.pendingCallers = static_cast<uint8_t>( _state.pendingCallers & ~bit );

		if ( _state.pendingCallers != 0 ) return StepResult::OK;

		return ResolveResponses();
	}

	StepResult Engine::ResolveResponses()
	{
		// ---- ron, which beats everything --------------------------
		uint8_t ronMask  = 0;
		uint8_t ronCount = 0;

		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			if ( _state.pendingResponse[seat].type == ActionType::RON )
			{
				ronMask = static_cast<uint8_t>( ronMask | ( 1u << seat ) );
				++ronCount;
			}
		}

		if ( ronCount > 0 )
		{
			if ( ronCount >= 3 )
			{
				EndHand( HandOutcome::ABORT_TRIPLE_RON );
				return StepResult::HAND_ENDED;
			}

			// Head bump: without double ron only the seat nearest the
			// discarder collects.
			if ( ronCount > 1 && !_rules.doubleRon )
			{
				for ( uint8_t i = 1; i <= _rules.numPlayers; ++i )
				{
					const uint8_t seat = static_cast<uint8_t>( ( _state.lastDiscarder + i ) % _rules.numPlayers );
					if ( ( ronMask & ( 1u << seat ) ) != 0 )
					{
						ronMask = static_cast<uint8_t>( 1u << seat );
						break;
					}
				}
			}

			EndHandWithRon( ronMask );
			return StepResult::HAND_ENDED;
		}

		// Everyone who passed on a tile they could have won on is now furiten.
		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			if ( _state.pendingResponse[seat].type == ActionType::PASS ) UpdateFuritenOnPass( seat );
		}

		// A robbed kan window only ever accepted ron and pass.
		if ( _state.awaitingChankan ) return CompleteChankanPass();

		// ---- pon and kan ------------------------------------------
		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			const Action &r = _state.pendingResponse[seat];
			if ( r.type == ActionType::PON )       return ApplyPon( r );
			if ( r.type == ActionType::DAIMINKAN ) return ApplyDaiminkan( r );
		}

		// ---- chi --------------------------------------------------
		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			const Action &r = _state.pendingResponse[seat];
			if ( r.type == ActionType::CHI ) return ApplyChi( r );
		}

		ResolveDiscard();
		return ( _state.phase == Phase::HAND_OVER ) ? StepResult::HAND_ENDED : StepResult::OK;
	}

	StepResult Engine::CompleteChankanPass()
	{
		_state.awaitingChankan = false;

		const uint8_t seat = _state.currentPlayer;

		if ( !GiveDrawTo( seat, true ) ) return StepResult::ILLEGAL;

		// A called kan's indicator waits for the discard that follows.
		_state.pendingDoraFlip = true;
		_state.phase           = Phase::DISCARD;

		if ( FourKanAbort() )
		{
			EndHand( HandOutcome::ABORT_FOUR_KAN );
			return StepResult::HAND_ENDED;
		}

		return StepResult::OK;
	}

	void Engine::UpdateFuritenOnPass( const uint8_t seat )
	{
		const Player &p      = _state.players[seat];
		const TileId  called = InstanceTile( _state.lastDiscard );

		if ( !IsValidTile( called ) ) return;

		Counts34 test = p.hand.Counts();
		if ( test[called] >= 4 ) return;
		++test[called];

		if ( !IsWinningHand( test, p.meldCount ) ) return;

		// Declining a winning tile costs you the rest of the go-around, and
		// the whole hand if your wait can no longer change.
		_state.players[seat].furitenTemporary = true;
		if ( p.riichiDeclared ) _state.players[seat].furitenPermanent = true;
	}

	StepResult Engine::ApplyChi( const Action &action )
	{
		Player &p = _state.players[action.actor];

		const TileId base   = action.base;
		const TileId called = InstanceTile( _state.lastDiscard );

		TileId  needed[2];
		uint8_t neededCount = 0;
		for ( TileId t = base; t <= base + 2; ++t )
		{
			if ( t != called ) needed[neededCount++] = t;
		}

		for ( uint8_t i = 0; i < neededCount; ++i )
		{
			const AkaMask bit = AkaBitFor( needed[i] );
			p.hand.Remove( needed[i], bit != AKA_NONE && ( action.meldAka & bit ) != 0 );
		}

		Meld &meld = p.melds[p.meldCount++];
		meld.type   = MeldType::CHI;
		meld.base   = base;
		meld.called = _state.lastDiscard;
		meld.from   = CalledFrom::LEFT;
		meld.aka    = static_cast<AkaMask>( action.meldAka |
			( InstanceIsAka( _state.lastDiscard ) ? AkaBitFor( called ) : AKA_NONE ) );

		CompleteCall( action.actor, MeldType::CHI, base );
		return StepResult::OK;
	}

	StepResult Engine::ApplyPon( const Action &action )
	{
		Player &p = _state.players[action.actor];

		const TileId  called   = InstanceTile( _state.lastDiscard );
		const AkaMask bit      = AkaBitFor( called );
		const bool    takesAka = bit != AKA_NONE && ( action.meldAka & bit ) != 0;

		p.hand.Remove( called, takesAka );
		p.hand.Remove( called, false );

		Meld &meld = p.melds[p.meldCount++];
		meld.type   = MeldType::PON;
		meld.base   = called;
		meld.called = _state.lastDiscard;
		meld.from   = static_cast<CalledFrom>(
			( action.actor + _rules.numPlayers - _state.lastDiscarder ) % _rules.numPlayers );
		meld.aka    = static_cast<AkaMask>( ( takesAka ? bit : AKA_NONE ) |
			( InstanceIsAka( _state.lastDiscard ) ? bit : AKA_NONE ) );

		CompleteCall( action.actor, MeldType::PON, called );
		return StepResult::OK;
	}

	StepResult Engine::ApplyDaiminkan( const Action &action )
	{
		Player &p = _state.players[action.actor];

		const TileId  called   = InstanceTile( _state.lastDiscard );
		const AkaMask bit      = AkaBitFor( called );
		const bool    takesAka = bit != AKA_NONE && ( p.hand.Aka() & bit ) != 0;

		for ( uint8_t i = 0; i < 3; ++i )
		{
			p.hand.Remove( called, takesAka && i == 0 );
		}

		Meld &meld = p.melds[p.meldCount++];
		meld.type   = MeldType::MINKAN;
		meld.base   = called;
		meld.called = _state.lastDiscard;
		meld.from   = static_cast<CalledFrom>(
			( action.actor + _rules.numPlayers - _state.lastDiscarder ) % _rules.numPlayers );
		meld.aka    = static_cast<AkaMask>( ( takesAka ? bit : AKA_NONE ) |
			( InstanceIsAka( _state.lastDiscard ) ? bit : AKA_NONE ) );

		CompleteCall( action.actor, MeldType::MINKAN, called );

		if ( !GiveDrawTo( action.actor, true ) ) return StepResult::ILLEGAL;

		// A called kan's indicator waits for the discard that follows.
		_state.pendingDoraFlip = true;

		if ( FourKanAbort() )
		{
			EndHand( HandOutcome::ABORT_FOUR_KAN );
			return StepResult::HAND_ENDED;
		}

		return StepResult::OK;
	}

	void Engine::CompleteCall( const uint8_t seat, const MeldType type, const TileId base )
	{
		// The claimed tile belongs to the meld now, so it must not be counted
		// again in the discarder's pond.
		Player &discarder = _state.players[_state.lastDiscarder];
		if ( discarder.discardCount > 0 )
		{
			const uint8_t index = static_cast<uint8_t>( discarder.discardCount - 1 );
			discarder.discardFlags[index] = static_cast<DiscardFlags>( discarder.discardFlags[index] | DISCARD_CALLED );
		}

		// Any call kills every outstanding ippatsu window and ends the first
		// go-around.
		for ( uint8_t s = 0; s < _rules.numPlayers; ++s )
		{
			_state.players[s].ippatsu = false;
		}
		_state.firstGoAround = false;

		_state.players[seat].awaitingDiscard = true;
		_state.players[seat].drawn           = INVALID_INSTANCE;

		// Kuikae: a kan leaves nothing to swap back, but a chi or pon does.
		_state.forbiddenDiscards = ( type == MeldType::MINKAN )
			? 0
			: KuikaeMask( type, base, InstanceTile( _state.lastDiscard ) );

		_state.currentPlayer  = seat;
		_state.pendingCallers = 0;
		_state.phase          = Phase::DISCARD;
	}

	void Engine::ResolveDiscard()
	{
		if ( FourWindsAbort() )
		{
			EndHand( HandOutcome::ABORT_FOUR_WINDS );
			return;
		}

		if ( FourRiichiAbort() )
		{
			EndHand( HandOutcome::ABORT_FOUR_RIICHI );
			return;
		}

		const uint8_t next = _state.NextSeat( _state.lastDiscarder );

		// The first go-around closes once the turn comes back to the dealer.
		if ( next == _state.dealer ) _state.firstGoAround = false;

		// Nobody claimed the final discard, so the hand is exhausted. This is
		// checked after the call window, which is what leaves room for a win
		// on the last discard.
		if ( _state.LiveWallEmpty() )
		{
			EndHand( HandOutcome::EXHAUSTIVE_DRAW );
			return;
		}

		_state.currentPlayer = next;
		_state.phase         = Phase::DRAW;
		GiveDrawTo( next, false );
		_state.phase         = Phase::DISCARD;
	}

	void Engine::EndHand( const HandOutcome outcome, const uint8_t winner, const uint8_t loser )
	{
		HandResult &r = _state.result;

		r.outcome = outcome;
		r.winner  = winner;
		r.loser   = loser;

		if ( outcome == HandOutcome::TSUMO && winner < _rules.numPlayers )
		{
			r.winnerMask  = static_cast<uint8_t>( 1u << winner );
			r.winningTile = _state.players[winner].drawn;
			r.rinshan     = _state.drewFromDeadWall;
			r.haitei      = !_state.drewFromDeadWall && _state.LiveWallEmpty();
		}

		_state.phase = Phase::HAND_OVER;
	}

	void Engine::EndHandWithRon( const uint8_t ronMask )
	{
		HandResult &r = _state.result;

		r.outcome     = HandOutcome::RON;
		r.winnerMask  = ronMask;
		r.loser       = _state.lastDiscarder;
		r.winningTile = _state.lastDiscard;
		r.chankan     = _state.awaitingChankan;
		r.houtei      = !_state.awaitingChankan && _state.LiveWallEmpty();

		// The headline winner is the claimant sitting nearest the discarder.
		for ( uint8_t i = 1; i <= _rules.numPlayers; ++i )
		{
			const uint8_t seat = static_cast<uint8_t>( ( _state.lastDiscarder + i ) % _rules.numPlayers );
			if ( ( ronMask & ( 1u << seat ) ) != 0 )
			{
				r.winner = seat;
				break;
			}
		}

		_state.awaitingChankan = false;
		_state.phase           = Phase::HAND_OVER;
	}

	// -------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------

	uint8_t Engine::OtherSeatsMask( const uint8_t seat ) const
	{
		uint8_t mask = 0;
		for ( uint8_t s = 0; s < _rules.numPlayers; ++s )
		{
			if ( s != seat ) mask = static_cast<uint8_t>( mask | ( 1u << s ) );
		}
		return mask;
	}

	bool Engine::FourKanAbort() const
	{
		if ( _state.TotalKans() < 4 ) return false;

		// Four kans in a single hand are allowed and keep playing; four spread
		// across seats abort.
		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			uint8_t kans = 0;
			for ( uint8_t i = 0; i < _state.players[seat].meldCount; ++i )
			{
				if ( _state.players[seat].melds[i].IsKan() ) ++kans;
			}
			if ( kans == 4 ) return false;
		}

		return true;
	}

	bool Engine::FourRiichiAbort() const
	{
		for ( uint8_t seat = 0; seat < _rules.numPlayers; ++seat )
		{
			if ( !_state.players[seat].riichiDeclared ) return false;
		}
		return true;
	}

	bool Engine::FourWindsAbort() const
	{
		// Suufon renda needs four seats, so it cannot happen in sanma.
		if ( _rules.numPlayers != 4 )  return false;
		if ( !_state.firstGoAround )   return false;

		TileId wind = INVALID_TILE;
		for ( uint8_t seat = 0; seat < 4; ++seat )
		{
			const Player &p = _state.players[seat];
			if ( p.discardCount != 1 || p.meldCount != 0 ) return false;

			const TileId t = InstanceTile( p.discards[0] );
			if ( !IsWind( t ) ) return false;

			if ( wind == INVALID_TILE ) wind = t;
			else if ( wind != t )       return false;
		}

		return true;
	}

} // namespace LRMahjong
