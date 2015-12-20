// Copyright (c) 2005 - 2015 Settlers Freaks (sf-team at siedler25.org)
//
// This file is part of Return To The Roots.
//
// Return To The Roots is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Return To The Roots is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Return To The Roots. If not, see <http://www.gnu.org/licenses/>.

///////////////////////////////////////////////////////////////////////////////
// Header
#include "defines.h"
#include "Ware.h"

#include "GameWorldGame.h"
#include "GameClientPlayer.h"
#include "buildings/nobBaseWarehouse.h"
#include "figures/nofCarrier.h"
#include "SerializedGameData.h"
#include "buildings/nobHarborBuilding.h"
#include "GameClient.h"
#include "gameData/ShieldConsts.h"
#include "gameData/GameConsts.h"
#include "Log.h"

///////////////////////////////////////////////////////////////////////////////
// Makros / Defines
#if defined _WIN32 && defined _DEBUG && defined _MSC_VER
#define new new(_NORMAL_BLOCK, THIS_FILE, __LINE__)
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

Ware::Ware(const GoodType type, noBaseBuilding* goal, noRoadNode* location) :
    next_dir(INVALID_DIR), state(STATE_WAITINWAREHOUSE), location(location),
    type(type == GD_SHIELDROMANS ? SHIELD_TYPES[GAMECLIENT.GetPlayer(location->GetPlayer()).nation] : type ),// Bin ich ein Schild? Dann evtl. Typ nach Nation anpassen
    goal(goal), next_harbor(MapPoint::Invalid())
{
    // Ware in den Index mit eintragen
    gwg->GetPlayer(location->GetPlayer()).RegisterWare(this);
    if(goal)
        goal->TakeWare(this);
}

Ware::~Ware()
{
    /*assert(!gwg->GetPlayer((location->GetPlayer()].IsWareRegistred(this));*/
    //if(location)
    //  assert(!gwg->GetPlayer((location->GetPlayer()).IsWareDependent(this));
}

void Ware::Destroy(void)
{
    assert(!goal);
}

void Ware::Serialize_Ware(SerializedGameData& sgd) const
{
    Serialize_GameObject(sgd);

    sgd.PushUnsignedChar(next_dir);
    sgd.PushUnsignedChar(static_cast<unsigned char>(state));
    sgd.PushObject(location, false);
    sgd.PushUnsignedChar(static_cast<unsigned char>(type));
    sgd.PushObject(goal, false);
    sgd.PushMapPoint(next_harbor);
}

Ware::Ware(SerializedGameData& sgd, const unsigned obj_id) : GameObject(sgd, obj_id),
    next_dir(sgd.PopUnsignedChar()),
    state(State(sgd.PopUnsignedChar())),
    location(sgd.PopObject<noRoadNode>(GOT_UNKNOWN)),
    type(GoodType(sgd.PopUnsignedChar())),
    goal(sgd.PopObject<noBaseBuilding>(GOT_UNKNOWN)),
    next_harbor(sgd.PopMapPoint())
{}

void Ware::SetGoal(noBaseBuilding* newGoal)
{
    goal = newGoal;
    if(goal)
        goal->TakeWare(this);
}

void Ware::RecalcRoute()
{
    // Nächste Richtung nehmen
    if(location && goal)
        next_dir = gwg->FindPathForWareOnRoads(*location, *goal, NULL, &next_harbor);
    else
        next_dir = INVALID_DIR;

    // Evtl gibts keinen Weg mehr? Dann wieder zurück ins Lagerhaus (wenns vorher überhaupt zu nem Ziel ging)
    if(next_dir == INVALID_DIR && goal)
    {
        // Tell goal about this
        NotifyGoalAboutLostWare();
        if(state == STATE_WAITFORSHIP)
        {
            // Ware was waiting for a ship so send the ware into the harbor
            assert(location);
            assert(location->GetGOT() == GOT_NOB_HARBORBUILDING);
            state = STATE_WAITINWAREHOUSE;
            SetGoal(static_cast<nobHarborBuilding*>(location));
            // but not going by ship
            static_cast<nobHarborBuilding*>(goal)->WareDontWantToTravelByShip(this);
        }
        else
        {
            assert(location);
            FindRouteToWarehouse();
        }
    }
    else
    {
        // If we waited in the harbor for the ship before and don't want to travel now
        // -> inform the harbor so that it can remove us from its list
        if(state == STATE_WAITFORSHIP && next_dir != SHIP_DIR)
        {
            assert(location);
            assert(location->GetGOT() == GOT_NOB_HARBORBUILDING);
            state = STATE_WAITINWAREHOUSE;
            static_cast<nobHarborBuilding*>(location)->WareDontWantToTravelByShip(this);
        }
    }
}

void Ware::GoalDestroyed()
{
    if(state == STATE_WAITINWAREHOUSE)
    {
        // Ware ist noch im Lagerhaus auf der Warteliste
        assert(false); // Should not happen. noBaseBuilding::WareNotNeeded handles this case!
        goal = NULL; // just in case: avoid corruption although the ware itself might be lost (won't ever be carried again)
    }
    // Ist sie evtl. gerade mit dem Schiff unterwegs?
    else if(state == STATE_ONSHIP)
    {
        // Ziel zunächst auf NULL setzen, was dann vom Zielhafen erkannt wird,
        // woraufhin dieser die Ware gleich in sein Inventar mit übernimmt
        goal = NULL;
    }
    // Oder wartet sie im Hafen noch auf ein Schiff
    else if(state == STATE_WAITFORSHIP)
    {
        // Dann dem Hafen Bescheid sagen
        assert(location);
        assert(location->GetGOT() == GOT_NOB_HARBORBUILDING);
        static_cast<nobHarborBuilding*>(location)->CancelWareForShip(this);
        GAMECLIENT.GetPlayer(location->GetPlayer()).RemoveWare(this);
        em->AddToKillList(this);
    }
    else
    {
        // Ware ist unterwegs, Lagerhaus finden und Ware dort einliefern
        assert(location);
        assert(location->GetPlayer() < MAX_PLAYERS);

        // Wird sie gerade aus einem Lagerhaus rausgetragen?
        if(location->GetGOT() == GOT_NOB_STOREHOUSE || location->GetGOT() == GOT_NOB_HARBORBUILDING || location->GetGOT() == GOT_NOB_HQ)
        {
            if(location != goal)
            {
                SetGoal(static_cast<noBaseBuilding*>(location));
            }
            else //at the goal (which was just destroyed) and get carried out right now? -> we are about to get destroyed...
            {
                goal = NULL;
                next_dir = INVALID_DIR;
            }
        }
        // Wenn sie an einer Flagge liegt, muss der Weg neu berechnet werden und dem Träger Bescheid gesagt werden
        else if(state == STATE_WAITATFLAG)
        {
            goal = NULL;
            unsigned char oldNextDir = next_dir;
            FindRouteToWarehouse();
            if(oldNextDir != next_dir)
            {
                RemoveWareJobForDir(oldNextDir);
                if(next_dir != INVALID_DIR)
                {
                    assert(goal); // Having a nextDir implies having a goal
                    CallCarrier();
                }else
                    assert(!goal); // Can only have a goal with a valid path
            }
        }
        else if(state == STATE_CARRIED)
        {
            if(goal != location)
            {
                // find a warehouse for us (if we are entering a warehouse already set this as new goal (should only happen if its a harbor for shipping as the building wasnt our goal))
                if(location->GetGOT() == GOT_NOB_STOREHOUSE || location->GetGOT() == GOT_NOB_HARBORBUILDING || location->GetGOT() == GOT_NOB_HQ) //currently carried into a warehouse? -> add ware (pathfinding will not return this wh because of path lengths 0)
                {
                    if(location->GetGOT()!=GOT_NOB_HARBORBUILDING)
                        LOG.lprintf("WARNING: Ware::GoalDestroyed() -- ware is currently being carried into warehouse or hq that was not it's goal! ware id %i, type %i, player %i, wareloc %i,%i, goal loc %i,%i \n",GetObjId(),type,location->GetPlayer(),GetLocation()->GetX(),GetLocation()->GetY(), goal->GetX(), goal->GetY());
                    SetGoal(static_cast<noBaseBuilding*>(location));
                }
                else
                {
                    goal = NULL;
                    FindRouteToWarehouse();
                }
            }else
            {
                // too late to do anything our road will be removed and ware destroyed when the carrier starts walking about
                goal = NULL;
            }
        }
    }
}

void Ware::WaitAtFlag(noFlag* flag)
{
    assert(flag);
    state = STATE_WAITATFLAG;
    location = flag;
}

void Ware::WaitInWarehouse(nobBaseWarehouse* wh)
{
    assert(wh);
    state = STATE_WAITINWAREHOUSE;
    location = wh;
}

void Ware::Carry(noRoadNode* nextGoal)
{
    assert(nextGoal);
    state = STATE_CARRIED;
    location = nextGoal;
}

/// Gibt dem Ziel der Ware bekannt, dass diese nicht mehr kommen kann
void Ware::NotifyGoalAboutLostWare()
{
    // Meinem Ziel Bescheid sagen, dass ich weg vom Fenster bin (falls ich ein Ziel habe!)
    if(goal)
    {
        goal->WareLost(this);
        goal = NULL;
        next_dir = INVALID_DIR;
    }
}

/// Wenn die Ware vernichtet werden muss
void Ware::WareLost(const unsigned char player)
{
    // Inventur verringern
    gwg->GetPlayer(player).DecreaseInventoryWare(type, 1);
    // Ziel der Ware Bescheid sagen
    NotifyGoalAboutLostWare();
    // Zentrale Registrierung der Ware löschen
    gwg->GetPlayer(player).RemoveWare(this);
}


void Ware::RemoveWareJobForDir(const unsigned char last_next_dir)
{
    // last_next_dir war die letzte Richtung, in die die Ware eigentlich wollte,
    // aber nun nicht mehr will, deshalb muss dem Träger Bescheid gesagt werden

    // War's überhaupt ne richtige Richtung?
    if(last_next_dir >= 6)
        return;
    // Existiert da noch ne Straße?
    if(!location->routes[last_next_dir])
        return;
    // Den Trägern Bescheid sagen
    location->routes[last_next_dir]->WareJobRemoved(NULL);
    // Wenn nicht, könntes ja sein, dass die Straße in ein Lagerhaus führt, dann muss dort Bescheid gesagt werden
    if(location->routes[last_next_dir]->GetF2()->GetType() == NOP_BUILDING)
    {
        noBuilding* bld = static_cast<noBuilding*>(location->routes[1]->GetF2());
        if(bld->GetBuildingType() == BLD_HEADQUARTERS || bld->GetBuildingType() == BLD_STOREHOUSE || bld->GetBuildingType() == BLD_HARBORBUILDING)
            static_cast<nobBaseWarehouse*>(bld)->DontFetchNextWare();
    }
}

void Ware::CallCarrier()
{
    assert(IsWaitingAtFlag());
    assert(next_dir != INVALID_DIR);
    assert(location);
    location->routes[next_dir]->AddWareJob(location);
}

bool Ware::FindRouteToWarehouse()
{
    assert(location);
    assert(!goal); // Goal should have been notified and therefore reset
    SetGoal(gwg->GetPlayer(location->GetPlayer()).FindWarehouse(*location, FW::Condition_StoreWare, 0, true, &type, true));

    if(goal)
    {
        // Find path if not already carried (will be called after arrival in that case)
        if(state != STATE_CARRIED)
        {
            next_dir = gwg->FindPathForWareOnRoads(*location, *goal, NULL, &next_harbor);
            assert(next_dir != INVALID_DIR);
        }
    }else
        next_dir = INVALID_DIR; // Make sure we are not going anywhere
    return goal != NULL;
}

///a lost ware got ordered
unsigned Ware::CheckNewGoalForLostWare(noBaseBuilding* newgoal)
{
    unsigned tlength = 0xFFFFFFFF;
    if (!IsWaitingAtFlag()) //todo: check all special cases for wares being carried right now and where possible allow them to be ordered
        return 0xFFFFFFFF;
    unsigned char possibledir = gwg->FindPathForWareOnRoads(*location, *newgoal, &tlength);
    if(possibledir != INVALID_DIR) //there is a valid path to the goal? -> ordered!
    {
        //in case the ware is right in front of the goal building the ware has to be moved away 1 flag and then back because non-warehouses cannot just carry in new wares they need a helper to do this
        if(possibledir==1 && newgoal->GetFlag()->GetPos() == location->GetPos())
        {
            for(unsigned i=0; i<6; i++)
            {
                if(i!=1 && location->routes[i])
                {
                    possibledir = i;
                    break;
                }
            }
            if(possibledir == 1) //got no other route from the flag -> impossible
                return 0xFFFFFFFF;
        }
        //at this point there either is a road to the goal or if we are at the flag of the goal we have a road to a different flag to bounce off of to get to the goal
        return tlength;
    }
    else
        return 0xFFFFFFFF;
}

/// this assumes that the ware is at a flag (todo: handle carried wares) and that there is a valid path to the goal
void Ware::SetNewGoalForLostWare(noBaseBuilding* newgoal)
{
    assert(location && newgoal);
    unsigned char possibledir = gwg->FindPathForWareOnRoads(*location, *newgoal);
    if(possibledir != INVALID_DIR) //there is a valid path to the goal? -> ordered!
    {
        //in case the ware is right in front of the goal building the ware has to be moved away 1 flag and then back because non-warehouses cannot just carry in new wares they need a helper to do this
        if(possibledir == 1 && newgoal->GetFlag()->GetPos() == location->GetPos())
        {
            for(unsigned i=0; i<6; i++)
            {
                if(i!=1 && location->routes[i])
                {
                    possibledir = i;
                    break;
                }
            }
            if(possibledir == 1) //got no other route from the flag -> impossible
                return;
        }
        //at this point there either is a road to the goal or if we are at the flag of the goal we have a road to a different flag to bounce off of to get to the goal
        next_dir = possibledir;
        SetGoal(newgoal);
        CallCarrier();
    }
}

bool Ware::IsRouteToGoal()
{
    assert(location);
    if(!goal)
        return false;
    if(location == goal)
        return true; // We are at our goal. All ok
    return (gwg->FindPathForWareOnRoads(*location, *goal) != INVALID_DIR);
}

/// Informiert Ware, dass eine Schiffsreise beginnt
void Ware::StartShipJourney()
{
    state = STATE_ONSHIP;
    location = NULL;
}

/// Informiert Ware, dass Schiffsreise beendet ist und die Ware nun in einem Hafengebäude liegt
void Ware::ShipJorneyEnded(nobHarborBuilding* hb)
{
    assert(hb);
    state = STATE_WAITINWAREHOUSE;
    location = hb;
}

/// Beginnt damit auf ein Schiff im Hafen zu warten
void Ware::WaitForShip(nobHarborBuilding* hb)
{
    state = STATE_WAITFORSHIP;
    location = hb;
}

std::string Ware::ToString() const
{
    std::stringstream s;
    s << "Ware(" << GetObjId() << "): type=" << GoodType2String(type) << ", location=" << location->GetX() << "," << location->GetY();
    return s.str();
}
