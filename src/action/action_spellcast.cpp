//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name action_spellcast.cpp - The spell cast action. */
//
//      (c) Copyright 2000-2005 by Vladi Belperchinov-Shabanski and Jimmy Salmon
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

/*
** This is inherited from action_attack.c, actually spell casting will
** be considered a `special' case attack action... //Vladi
*/

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"

#include "action/action_spellcast.h"

#include "animation.h"
//Wyrmgus start
#include "commands.h"
//Wyrmgus end
#include "iolib.h"
#include "map.h"
#include "missile.h"
#include "pathfinder.h"
#include "player.h"
#include "script.h"
#include "sound.h"
#include "spells.h"
//Wyrmgus start
#include "tileset.h"
//Wyrmgus end
#include "translate.h"
#include "ui.h"
#include "unit.h"
//Wyrmgus start
#include "unit_find.h"
//Wyrmgus end
#include "unittype.h"
#include "video.h"

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

/* static */ COrder *COrder::NewActionSpellCast(const SpellType &spell, const Vec2i &pos, CUnit *target, bool isAutocast)
{
	COrder_SpellCast *order = new COrder_SpellCast(isAutocast);

	order->Range = spell.Range;
	if (target) {
		// Destination could be killed.
		// Should be handled in action, but is not possible!
		// Unit::Refs is used as timeout counter.
		if (target->Destroyed) {
			// FIXME: where check if spell needs a unit as destination?
			// FIXME: target->Type is now set to 0. maybe we shouldn't bother.
			const Vec2i diag(order->Range, order->Range);
			order->goalPos = target->tilePos /* + target->Type->GetHalfTileSize() */ - diag;
			order->Range <<= 1;
		} else {
			order->SetGoal(target);
		}
	} else {
		order->goalPos = pos;
	}
	order->SetSpell(spell);

	return order;
}

/* virtual */ void COrder_SpellCast::Save(CFile &file, const CUnit &unit) const
{
	file.printf("{\"action-spell-cast\",");

	if (this->Finished) {
		file.printf(" \"finished\", ");
	}
	file.printf(" \"range\", %d,", this->Range);
	if (this->HasGoal()) {
		file.printf(" \"goal\", \"%s\",", UnitReference(this->GetGoal()).c_str());
	}
	file.printf(" \"tile\", {%d, %d},", this->goalPos.x, this->goalPos.y);

	file.printf("\"state\", %d,", this->State);
	file.printf(" \"spell\", \"%s\"", this->Spell->Ident.c_str());

	file.printf("}");
}

/* virtual */ bool COrder_SpellCast::ParseSpecificData(lua_State *l, int &j, const char *value, const CUnit &unit)
{
	if (!strcmp(value, "spell")) {
		++j;
		this->Spell = SpellTypeByIdent(LuaToString(l, -1, j + 1));
	} else if (!strcmp(value, "range")) {
		++j;
		this->Range = LuaToNumber(l, -1, j + 1);
	} else if (!strcmp(value, "state")) {
		++j;
		this->State = LuaToNumber(l, -1, j + 1);
	} else if (!strcmp(value, "tile")) {
		++j;
		lua_rawgeti(l, -1, j + 1);
		CclGetPos(l, &this->goalPos.x , &this->goalPos.y);
		lua_pop(l, 1);
	} else {
		return false;
	}
	return true;
}

/* virtual */ bool COrder_SpellCast::IsValid() const
{
	Assert(Action == UnitActionSpellCast);
	if (this->HasGoal()) {
		return this->GetGoal()->IsAliveOnMap();
	} else {
		return Map.Info.IsPointOnMap(this->goalPos);
	}
}

/* virtual */ PixelPos COrder_SpellCast::Show(const CViewport &vp, const PixelPos &lastScreenPos) const
{
	PixelPos targetPos;

	if (this->HasGoal()) {
		targetPos = vp.MapToScreenPixelPos(this->GetGoal()->GetMapPixelPosCenter());
	} else {
		targetPos = vp.TilePosToScreen_Center(this->goalPos);
	}
	//Wyrmgus start
//	Video.FillCircleClip(ColorBlue, lastScreenPos, 2);
//	Video.DrawLineClip(ColorBlue, lastScreenPos, targetPos);
//	Video.FillCircleClip(ColorBlue, targetPos, 3);
	if (Preference.ShowPathlines) {
		Video.FillCircleClip(ColorBlue, lastScreenPos, 2);
		Video.DrawLineClip(ColorBlue, lastScreenPos, targetPos);
		Video.FillCircleClip(ColorBlue, targetPos, 3);
	}
	//Wyrmgus end
	return targetPos;
}

/* virtual */ void COrder_SpellCast::UpdatePathFinderData(PathFinderInput &input)
{
	input.SetMinRange(0);
	input.SetMaxRange(this->Range);

	Vec2i tileSize;
	if (this->HasGoal()) {
		CUnit *goal = this->GetGoal();
		tileSize.x = goal->Type->TileWidth;
		tileSize.y = goal->Type->TileHeight;
		input.SetGoal(goal->tilePos, tileSize);
	} else {
		tileSize.x = 0;
		tileSize.y = 0;
		input.SetGoal(this->goalPos, tileSize);
	}
}

/**
**  Call when animation step is "attack"
*/
/* virtual */ void COrder_SpellCast::OnAnimationAttack(CUnit &unit)
{
	UnHideUnit(unit); // unit is invisible until attacks
	CUnit *goal = GetGoal();
	if (goal && !goal->IsVisibleAsGoal(*unit.Player)) {
		unit.ReCast = 0;
	} else {
		unit.ReCast = SpellCast(unit, *this->Spell, goal, goalPos);
	}
}

/**
**  Get goal position
*/
/* virtual */ const Vec2i COrder_SpellCast::GetGoalPos() const
{
	const Vec2i invalidPos(-1, -1);
	if (goalPos != invalidPos) {
		return goalPos;
	}
	if (this->HasGoal()) {
		return this->GetGoal()->tilePos;
	}
	return invalidPos;
}

/**
**  Animate unit spell cast
**
**  @param unit  Unit, for that spell cast/attack animation is played.
*/
static void AnimateActionSpellCast(CUnit &unit, COrder_SpellCast &order)
{
	//Wyrmgus start
//	const CAnimations *animations = unit.Type->Animations;
	const CAnimations *animations = unit.GetAnimations();
	//Wyrmgus end

	if (!animations || (!animations->Attack && !animations->SpellCast)) {
		// if don't have animations just cast spell
		order.OnAnimationAttack(unit);
		return;
	}
	if (animations->SpellCast) {
		UnitShowAnimation(unit, animations->SpellCast);
	} else {
		UnitShowAnimation(unit, animations->Attack);
	}
}

/**
**  Check for dead goal.
**
**  @warning  The caller must check, if he likes the restored SavedOrder!
**
**  @todo     If a unit enters into a building, then the caster chooses an
**            other goal, perhaps it is better to wait for the goal?
**
**  @param unit  Unit using the goal.
**
**  @return      true if order have changed, false else.
*/
bool COrder_SpellCast::CheckForDeadGoal(CUnit &unit)
{
	const CUnit *goal = this->GetGoal();

	// Position or valid target, it is ok.
	if (!goal || goal->IsVisibleAsGoal(*unit.Player)) {
		return false;
	}

	// Goal could be destroyed or unseen
	// So, cannot use type.
	this->goalPos = goal->tilePos;
	this->Range = 0;
	this->ClearGoal();

	// If we have a saved order continue this saved order.
	if (unit.RestoreOrder()) {
		return true;
	}
	return false;
}

/**
**  Handle moving to the target.
**
**  @param unit  Unit, for that the spell cast is handled.
*/
bool COrder_SpellCast::SpellMoveToTarget(CUnit &unit)
{
	// Unit can't move
	int err = 1;
	if (unit.CanMove()) {
		err = DoActionMove(unit);
		if (unit.Anim.Unbreakable) {
			return false;
		}
	}

	// when reached DoActionMove changes unit action
	// FIXME: use return codes from pathfinder
	CUnit *goal = this->GetGoal();

	if (goal && unit.MapDistanceTo(*goal) <= this->Range) {
		// there is goal and it is in range
		//Wyrmgus start
//		UnitHeadingFromDeltaXY(unit, goal->tilePos + goal->Type->GetHalfTileSize() - unit.tilePos);
		UnitHeadingFromDeltaXY(unit, Vec2i(goal->tilePos.x * PixelTileSize.x, goal->tilePos.y * PixelTileSize.y) + goal->Type->GetHalfTilePixelSize() - Vec2i(unit.tilePos.x * PixelTileSize.x, unit.tilePos.y * PixelTileSize.y) - unit.Type->GetHalfTilePixelSize());
		//Wyrmgus end
		this->State++; // cast the spell
		return false;
	} else if (!goal && unit.MapDistanceTo(this->goalPos) <= this->Range) {
		// there is no goal and target spot is in range
		UnitHeadingFromDeltaXY(unit, this->goalPos - unit.tilePos);
		this->State++; // cast the spell
		return false;
	} else if (err == PF_UNREACHABLE || !unit.CanMove()) {
		//Wyrmgus start
		//if is unreachable and is on a raft, see if the raft can move closer to the target
		if ((Map.Field(unit.tilePos)->Flags & MapFieldBridge) && !unit.Type->BoolFlag[BRIDGE_INDEX].value && unit.Type->UnitType == UnitTypeLand) {
			std::vector<CUnit *> table;
			Select(unit.tilePos, unit.tilePos, table);
			for (size_t i = 0; i != table.size(); ++i) {
				if (!table[i]->Removed && table[i]->Type->BoolFlag[BRIDGE_INDEX].value && table[i]->CanMove()) {
					if (table[i]->CurrentAction() == UnitActionStill) {
						CommandStopUnit(*table[i]);
						CommandMove(*table[i], this->HasGoal() ? this->GetGoal()->tilePos : this->goalPos, FlushCommands);
					}
					return false;
				}
			}
		}
		//Wyrmgus end
		// goal/spot unreachable and out of range -- give up
		return true;
	}
	return false;
}


/* virtual */ void COrder_SpellCast::Execute(CUnit &unit)
{
	COrder_SpellCast &order = *this;

	if (unit.Wait) {
		if (!unit.Waiting) {
			unit.Waiting = 1;
			unit.WaitBackup = unit.Anim;
		}
		//Wyrmgus start
//		UnitShowAnimation(unit, unit.Type->Animations->Still);
		UnitShowAnimation(unit, unit.GetAnimations()->Still);
		//Wyrmgus end
		unit.Wait--;
		return;
	}
	if (unit.Waiting) {
		unit.Anim = unit.WaitBackup;
		unit.Waiting = 0;
	}
	const SpellType &spell = order.GetSpell();
	switch (this->State) {
		case 0:
			// Check if we can cast the spell.
			if (!CanCastSpell(unit, spell, order.GetGoal(), order.goalPos)) {
				// Notify player about this problem
				if (unit.Variable[MANA_INDEX].Value < spell.ManaCost) {
					unit.Player->Notify(NotifyYellow, unit.tilePos,
										//Wyrmgus start
//										_("%s: not enough mana for spell: %s"),
										_("%s does not have enough mana to use the %s ability."),
//										unit.Type->Name.c_str(), spell.Name.c_str());
										unit.GetMessageName().c_str(), spell.Name.c_str());
										//Wyrmgus end
				} else if (unit.SpellCoolDownTimers[spell.Slot]) {
					unit.Player->Notify(NotifyYellow, unit.tilePos,
										//Wyrmgus start
//										_("%s: spell is not ready yet: %s"),
										_("%s's ability %s is not ready yet."),
//										unit.Type->Name.c_str(), spell.Name.c_str());
										unit.GetMessageName().c_str(), spell.Name.c_str());
										//Wyrmgus end
				} else if (unit.Player->CheckCosts(spell.Costs, false)) {
					unit.Player->Notify(NotifyYellow, unit.tilePos,
										//Wyrmgus start
//										_("%s: not enough resources to cast spell: %s"),
										_("You do not have enough resources for %s to use the %s ability."),
//										unit.Type->Name.c_str(), spell.Name.c_str());
										unit.GetMessageName().c_str(), spell.Name.c_str());
										//Wyrmgus end
				//Wyrmgus start
				} else if (spell.Target == TargetUnit && order.GetGoal() == NULL) {
					unit.Player->Notify(NotifyYellow, unit.tilePos,
										_("%s needs a target to use the %s ability."),
										unit.GetMessageName().c_str(), spell.Name.c_str());
				//Wyrmgus end
				} else {
					unit.Player->Notify(NotifyYellow, unit.tilePos,
										//Wyrmgus start
//										_("%s: can't cast spell: %s"),
										_("%s cannot use the %s ability under these conditions."),
//										unit.Type->Name.c_str(), spell.Name.c_str());
										unit.GetMessageName().c_str(), spell.Name.c_str());
										//Wyrmgus end
				}

				if (unit.Player->AiEnabled) {
					DebugPrint("FIXME: do we need an AI callback?\n");
				}
				if (!unit.RestoreOrder()) {
					this->Finished = true;
				}
				return ;
			}
			if (CheckForDeadGoal(unit)) {
				return;
			}
			// FIXME FIXME FIXME: Check if already in range and skip straight to 2(casting)
			unit.ReCast = 0; // repeat spell on next pass? (defaults to `no')
			this->State = 1;
		// FALL THROUGH
		case 1:                         // Move to the target.
			//Wyrmgus start
//			if (spell.Range != INFINITE_RANGE) {
			if (spell.Range != INFINITE_RANGE && spell.Range != 0) {
			//Wyrmgus end
				if (SpellMoveToTarget(unit) == true) {
					if (!unit.RestoreOrder()) {
						this->Finished = true;
					}
					return ;
				}
				return ;
			} else {
				this->State = 2;
			}
		// FALL THROUGH
		case 2:                         // Cast spell on the target.
			if (!spell.IsCasterOnly() || spell.ForceUseAnimation) {
				AnimateActionSpellCast(unit, *this);
				if (unit.Anim.Unbreakable) {
					return ;
				}
			} else {
				// FIXME: what todo, if unit/goal is removed?
				CUnit *goal = order.GetGoal();
				if (goal && goal != &unit && !goal->IsVisibleAsGoal(*unit.Player)) {
					unit.ReCast = 0;
				} else {
					unit.ReCast = SpellCast(unit, spell, goal, order.goalPos);
				}
			}

			if (unit.IsAlive() == false) { // unit has died when casting spell (demolish)
				return;
			}
			// Target is dead ? Change order ?
			if (CheckForDeadGoal(unit)) {
				return;
			}

			// Check, if goal has moved (for ReCast)
			if (unit.ReCast) {
				if (order.GetGoal() && unit.MapDistanceTo(*order.GetGoal()) > this->Range) {
					this->State = 0;
					return;
				}
				if (this->isAutocast) {
					if (order.GetGoal() && order.GetGoal()->tilePos != order.goalPos) {
						order.goalPos = order.GetGoal()->tilePos;
					}
					if (unit.Player->AiEnabled) {
						if (!unit.RestoreOrder()) {
							this->Finished = true;
						}
						return ;
					}
				}
			}
			if (!unit.ReCast && unit.CurrentAction() != UnitActionDie) {
				if (!unit.RestoreOrder()) {
					this->Finished = true;
				}
				return ;
			}
			break;

		default:
			this->State = 0; // Reset path, than move to target
			break;
	}
}

//@}
