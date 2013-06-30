/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#include <algorithm>
#include <sstream>
#include "rpg_battlecommand.h"
#include "input.h"
#include "output.h"
#include "player.h"
#include "sprite.h"
#include "graphics.h"
#include "filefinder.h"
#include "cache.h"
#include "game_system.h"
#include "game_temp.h"
#include "game_party.h"
#include "game_enemy.h"
#include "game_enemyparty.h"
#include "game_interpreter_battle.h"
#include "game_message.h"
#include "game_switches.h"
#include "game_battle.h"
#include "battle_battler.h"
#include "battle_animation.h"
#include "battle_actions.h"
#include "scene_battle_rpg2k.h"
#include "scene_battle.h"

////////////////////////////////////////////////////////////
Scene_Battle_Rpg2k::Scene_Battle_Rpg2k() : Scene_Battle(),
actor_index(0),
active_actor(NULL),
turn(0)
{
}

////////////////////////////////////////////////////////////
Scene_Battle_Rpg2k::~Scene_Battle_Rpg2k() {
}

void Scene_Battle_Rpg2k::Start() {
	if (Player::battle_test_flag) {
		if (Player::battle_test_troop_id <= 0) {
			Output::Error("Invalid Monster Party Id");
		} else {
			InitBattleTest();
		}
	}

	Game_Battle::Init();

	CreateWindows();

	SetState(State_Start);
}

void Scene_Battle_Rpg2k::Update() {
	options_window->Update();
	status_window->Update();
	command_window->Update();
	help_window->Update();
	item_window->Update();
	skill_window->Update();
	target_window->Update();
	message_window->Update();

	ProcessActions();
	ProcessInput();

	Game_Battle::Update();
	/*DoAuto();

	UpdateBackground();
	UpdateCursors();
	UpdateSprites();
	UpdateFloaters();
	UpdateAnimations();*/
}

void Scene_Battle_Rpg2k::Terminate() {

}

void Scene_Battle_Rpg2k::InitBattleTest() {
	Game_Temp::battle_troop_id = Player::battle_test_troop_id;
	Game_Temp::battle_background = Data::system.battletest_background;

	Game_Party().SetupBattleTestMembers();

	Game_EnemyParty().Setup(Game_Temp::battle_troop_id);
}

void Scene_Battle_Rpg2k::CreateWindows() {
	CreateBattleOptionWindow();
	CreateBattleTargetWindow();
	CreateBattleCommandWindow();
	CreateBattleMessageWindow();

	help_window.reset(new Window_Help(0, 0, 320, 32));
	item_window.reset(new Window_Item(0, 160, 320, 80));
	item_window->SetHelpWindow(help_window.get());
	item_window->Refresh();
	item_window->SetIndex(0);

	skill_window.reset(new Window_Skill(0, 160, 320, 80));
	skill_window->SetHelpWindow(help_window.get());

	status_window.reset(new Window_BattleStatus_Rpg2k(0, 160, 320 - 76, 80));
}

void Scene_Battle_Rpg2k::CreateBattleOptionWindow() {
	std::vector<std::string> commands;
	commands.push_back(Data::terms.battle_fight);
	commands.push_back(Data::terms.battle_auto);
	commands.push_back(Data::terms.battle_escape);

	options_window.reset(new Window_Command(commands, 76));
	options_window->SetHeight(80);
	options_window->SetY(160);
	// TODO: Auto Battle not implemented
	options_window->DisableItem(1);
}

void Scene_Battle_Rpg2k::CreateBattleTargetWindow() {
	std::vector<std::string> commands;
	std::vector<Game_Enemy*> enemies = Game_EnemyParty().GetAliveEnemies();

	for (std::vector<Game_Enemy*>::iterator it = enemies.begin();
		it != enemies.end(); ++it) {
		commands.push_back((*it)->GetName());
	}

	target_window.reset(new Window_Command(commands, 136, 4));
	target_window->SetHeight(80);
	target_window->SetY(160);
	target_window->SetZ(200);
}

void Scene_Battle_Rpg2k::CreateBattleCommandWindow() {
	std::vector<std::string> commands;
	commands.push_back(Data::terms.command_attack);
	commands.push_back(Data::terms.command_skill);
	commands.push_back(Data::terms.command_defend);
	commands.push_back(Data::terms.command_item);

	command_window.reset(new Window_Command(commands, 76));
	command_window->SetHeight(80);
	command_window->SetX(320 - 76);
	command_window->SetY(160);
}


void Scene_Battle_Rpg2k::CreateBattleMessageWindow() {
	message_window.reset(new Window_BattleMessage(0, 160, 320, 80));

	message_window->SetZ(300);
}

void Scene_Battle_Rpg2k::RefreshCommandWindow() {
	std::string skill_name = active_actor->GetSkillName();
	command_window->SetItemText(1,
		skill_name.empty() ? Data::terms.command_skill : skill_name);
}

void Scene_Battle_Rpg2k::SetState(Scene_Battle::State new_state) {
	previous_state = state;
	state = new_state;
	if (state == State_SelectActor && auto_battle)
		state = State_AutoBattle;

	options_window->SetActive(false);
	status_window->SetActive(false);
	command_window->SetActive(false);
	item_window->SetActive(false);
	skill_window->SetActive(false);
	target_window->SetActive(false);
	message_window->SetActive(false);

	switch (state) {
	case State_Start:
		message_window->SetActive(true);
		break;
	case State_SelectOption:
		options_window->SetActive(true);
		break;
	case State_SelectActor:
		status_window->SetActive(true);
		break;
	case State_AutoBattle:
		break;
	case State_SelectCommand:
		command_window->SetActive(true);
		RefreshCommandWindow();
		break;
	case State_SelectEnemyTarget:
		break;
	case State_SelectAllyTarget:
		status_window->SetActive(true);
		break;
	case State_SelectItem:
		item_window->SetActive(true);
		//item_window->SetActor(Game_Battle::GetActiveActor());
		item_window->Refresh();
		break;
	case State_SelectSkill:
		skill_window->SetActive(true);
		skill_window->SetActor(active_actor->GetId());
		skill_window->SetIndex(0);
		break;
	case State_AllyAction:
	case State_EnemyAction:
	case State_Victory:
	case State_Defeat:
		break;
	}

	options_window->SetVisible(false);
	status_window->SetVisible(false);
	command_window->SetVisible(false);
	item_window->SetVisible(false);
	skill_window->SetVisible(false);
	help_window->SetVisible(false);
	target_window->SetVisible(false);
	message_window->SetVisible(false);

	switch (state) {
	case State_Start:
		message_window->SetVisible(true);
		break;
	case State_SelectOption:
		options_window->SetVisible(true);
		status_window->SetVisible(true);
		status_window->SetX(76);
		status_window->SetIndex(-1);
		break;
	case State_SelectActor:
		SelectNextActor();
		break;
	case State_AutoBattle:
		// no-op
		break;
	case State_SelectCommand:
		status_window->SetVisible(true);
		command_window->SetVisible(true);
		status_window->SetX(0);
		break;
	case State_SelectEnemyTarget:
		status_window->SetVisible(true);
		command_window->SetVisible(true);
		target_window->SetActive(true);
		target_window->SetVisible(true);
		break;
	case State_SelectAllyTarget:
		status_window->SetVisible(true);
		status_window->SetX(0);
		command_window->SetVisible(true);
		break;
	case State_AllyAction:
	case State_EnemyAction:
	case State_Battle:
		message_window->SetVisible(true);
		break;
	case State_SelectItem:
		item_window->SetVisible(true);
		item_window->SetHelpWindow(help_window.get());
		help_window->SetVisible(true);
		break;
	case State_SelectSkill:
		skill_window->SetVisible(true);
		skill_window->SetHelpWindow(help_window.get());
		help_window->SetVisible(true);
		break;
	case State_Victory:
	case State_Defeat:
		message_window->SetVisible(true);
		break;
	}
}

void Scene_Battle_Rpg2k::ProcessActions() {
	switch (state) {
	case State_Start:
		if (DisplayMonstersInMessageWindow()) {
			message_window->SetMessageMode(Window_BattleMessage::Mode_Normal);
			SetState(State_SelectOption);
		}
		break;
	case State_SelectActor:
	case State_AutoBattle:
		Game_Battle::Update();

		CheckWin();
		CheckLose();
		CheckAbort();
		CheckFlee();

		if (help_window->GetVisible() && message_timer > 0) {
			message_timer--;
			if (message_timer <= 0)
				help_window->SetVisible(false);
		}

		while (Game_Battle::NextActiveEnemy())
			EnemyAction();

		break;
	case State_Battle:
		message_window->SetMessageMode(Window_BattleMessage::Mode_Action);
		if (!battle_actions.empty()) {
			if (battle_actions.front().second->GetSource()->IsDead()) {
				// No zombies allowed ;)
				battle_actions.pop_front();
			}
			else if (battle_actions.front().second->Execute()) {
				battle_actions.pop_front();
				message_window->SetMessageMode(Window_BattleMessage::Mode_Normal);
				if (CheckWin() ||
					CheckLose() ||
					CheckAbort() ||
					CheckFlee()) {
						return;
				}
			}
		} else {
			message_window->SetMessageMode(Window_BattleMessage::Mode_Normal);
			NextTurn();
		}
		break;
	case State_AllyAction:
	case State_EnemyAction:
	default:
		break;
	}
}

void Scene_Battle_Rpg2k::ProcessInput() {
	if (Input::IsTriggered(Input::DECISION)) {
		switch (state) {
		case State_Start:
			// no-op
			break;
		case State_SelectOption:
			OptionSelected();
			break;
		case State_SelectActor:
			SetState(State_SelectCommand);
			SelectNextActor();
			break;
		case State_AutoBattle:
			// no-op
			break;
		case State_SelectCommand:
			CommandSelected();
			break;
		case State_SelectEnemyTarget:
			EnemySelected();
			break;
		case State_SelectAllyTarget:
			//TargetDone();
			break;
		case State_SelectItem:
			ItemSelected();
			break;
		case State_SelectSkill:
			SkillSelected();
			break;
		case State_AllyAction:
		case State_EnemyAction:
			break;
		case State_Victory:
		case State_Defeat:
			Scene::Pop();
			break;
		}
	}

	if (Input::IsTriggered(Input::CANCEL)) {
		Game_System::SePlay(Data::system.cancel_se);
		switch (state) {
		case State_Start:
		case State_SelectOption:
			// no-op
			break;
		case State_SelectActor:
		case State_AutoBattle:
			SetState(State_SelectOption);
			break;
		case State_SelectCommand:
			--actor_index;
			SelectPreviousActor();
			break;
		case State_SelectEnemyTarget:
		case State_SelectItem:
		case State_SelectSkill:
			SetState(State_SelectCommand);
			break;
		case State_SelectAllyTarget:
			SetState(State_SelectItem);
			break;
		case State_AllyAction:
		case State_EnemyAction:
			// no-op
			break;
		case State_Victory:
		case State_Defeat:
			Scene::Pop();
			break;
		}
	}

	/*if (state == State_SelectEnemyTarget && Game_Battle::HaveTargetEnemy()) {
		if (Input::IsRepeated(Input::DOWN))
			Game_Battle::TargetNextEnemy();
		if (Input::IsRepeated(Input::UP))
			Game_Battle::TargetPreviousEnemy();
		Game_Battle::ChooseEnemy();
	}

	if (state == State_SelectAllyTarget && Game_Battle::HaveTargetAlly()) {
		if (Input::IsRepeated(Input::DOWN))
			Game_Battle::TargetNextAlly();
		if (Input::IsRepeated(Input::UP))
			Game_Battle::TargetPreviousAlly();
	}*/
}

void Scene_Battle_Rpg2k::NextTurn() {
	++turn;
	actor_index = 0;
	SetState(State_SelectOption);
}

void Scene_Battle_Rpg2k::OptionSelected() {
	Game_System::SePlay(Data::system.decision_se);

	switch (options_window->GetIndex()) {
	case 0: // Battle
		CreateBattleTargetWindow();
		auto_battle = false;
		SetState(State_SelectActor);
		break;
	case 1: // Auto Battle
		auto_battle = true;
		Output::Post("Auto Battle not implemented yet.\nSorry :)");
		//SetState(State_SelectActor);
		break;
	case 2: // Escape
		//Escape();
		break;
	}
}

void Scene_Battle_Rpg2k::CommandSelected() {
	Game_System::SePlay(Data::system.decision_se);

	switch (command_window->GetIndex()) {
		case 0: // Attack
			AttackSelected();
			break;
		case 1: // Skill
			SetState(State_SelectSkill);
			break;
		case 2: // Defense
			DefendSelected();
			break;
		case 3: // Item
			SetState(State_SelectItem);
			break;
		default:
			// no-op
			break;
	}
}

void Scene_Battle_Rpg2k::AttackSelected() {
	Game_System::SePlay(Data::system.decision_se);

	SetState(State_SelectEnemyTarget);
}
	
void Scene_Battle_Rpg2k::DefendSelected() {
	Game_System::SePlay(Data::system.decision_se);
}

void Scene_Battle_Rpg2k::ItemSelected() {
	Game_System::SePlay(Data::system.decision_se);
}
	
void Scene_Battle_Rpg2k::SkillSelected() {
	RPG::Skill* skill = skill_window->GetSkill();

	if (!skill || !active_actor->IsSkillUsable(skill->ID)) {
		Game_System::SePlay(Data::system.buzzer_se);
		return;
	}

	Game_System::SePlay(Data::system.decision_se);

	switch (skill->type) {
	case RPG::Skill::Type_teleport:
	case RPG::Skill::Type_escape:
	case RPG::Skill::Type_switch:
		//BeginSkill();
		return;
	case RPG::Skill::Type_normal:
	default:
		break;
	}

	switch (skill->scope) {
		case RPG::Skill::Scope_enemy:
			//Game_Battle::SetTargetEnemy(0);
			SetState(State_SelectEnemyTarget);
			break;
		case RPG::Skill::Scope_ally:
			//Game_Battle::TargetActiveAlly();
			SetState(State_SelectAllyTarget);
			break;
		case RPG::Skill::Scope_enemies: {
			BattlerActionPair battler_action(active_actor, EASYRPG_MAKE_SHARED<Game_BattleAction::PartyTargetAction>(active_actor, &Game_EnemyParty()));
			battle_actions.push_back(battler_action);
			break;
		}
		case RPG::Skill::Scope_self:
			break;
		case RPG::Skill::Scope_party: {
			BattlerActionPair battler_action(active_actor, EASYRPG_MAKE_SHARED<Game_BattleAction::PartyTargetAction>(active_actor, &Game_Party()));
			battle_actions.push_back(battler_action);
			break;
		}
	}
}

void Scene_Battle_Rpg2k::EnemySelected() {
	Game_Enemy* target = static_cast<Game_Enemy*>(Game_EnemyParty().GetAliveEnemies()[target_window->GetIndex()]);

	switch (previous_state) {
		case State_SelectCommand:
		{
			BattlerActionPair battler_action(active_actor, EASYRPG_MAKE_SHARED<Game_BattleAction::AttackSingleNormal>(active_actor, target));
			battle_actions.push_back(battler_action);
			break;
		}
		case State_SelectSkill:
		{
			BattlerActionPair battler_action(active_actor, EASYRPG_MAKE_SHARED<Game_BattleAction::AttackSingleSkill>(active_actor, target, skill_window->GetSkill()));
			battle_actions.push_back(battler_action);
			break;
		}
		case State_SelectItem:
		{
			// Todo
			//BattlerActionPair battler_action(active_actor, EASYRPG_MAKE_SHARED<Game_BattleAction::AttackSingleItem>(active_actor, target));
			//battle_actions.push_back(battler_action);
			break;
		}
		default:
			assert("Invalid previous state for enemy selection" && false);
	}

	SetState(State_SelectActor);
}

void Scene_Battle_Rpg2k::SelectNextActor() {
	std::vector<Game_Actor*> allies = Game_Party().GetActors();

	if ((size_t)actor_index == allies.size()) {
		SetState(State_Battle);
		CreateEnemyActions();
		CreateExecutionOrder();
		return;
	}

	active_actor = allies[actor_index];
	status_window->SetIndex(actor_index);
	actor_index++;

	if (active_actor->GetAutoBattle()) {
		// ToDo Automatic stuff
		SelectNextActor();
		return;
	}

	SetState(Scene_Battle::State_SelectCommand);
}

void Scene_Battle_Rpg2k::SelectPreviousActor() {
	std::vector<Game_Actor*> allies = Game_Party().GetActors();

	if (allies[0] == active_actor) {
		SetState(State_SelectOption);
		actor_index = 0;
		return;
	}
	
	actor_index--;
	battle_actions.pop_front();
	active_actor = allies[actor_index];	

	if (active_actor->GetAutoBattle()) {
		SelectPreviousActor();
		battle_actions.pop_front();
		return;
	}

	SetState(State_SelectActor);
}

static bool BattlerSort(BattlerActionPair first, BattlerActionPair second) {
	return first.first->GetAgi() > second.first->GetAgi();
}

void Scene_Battle_Rpg2k::CreateExecutionOrder() {
	std::sort(battle_actions.begin(), battle_actions.end(), BattlerSort);
}

void Scene_Battle_Rpg2k::CreateEnemyActions() {
	std::vector<Game_Enemy*> alive_enemies = Game_EnemyParty().GetAliveEnemies();
	std::vector<Game_Enemy*>::const_iterator it;
	for (it = alive_enemies.begin(); it != alive_enemies.end(); ++it) {
		battle_actions.push_back(BattlerActionPair(*it, EASYRPG_MAKE_SHARED<Game_BattleAction::AttackSingleNormal>(*it, Game_Party().GetRandomAliveBattler())));
	}
}

bool Scene_Battle_Rpg2k::DisplayMonstersInMessageWindow() {
	static bool first = true;

	if (!first) {
		if (!Game_Message::message_waiting) {
			return true;
		}
	}

	message_window->SetMessageMode(Window_BattleMessage::Mode_EnemyEncounter);

	first = false;

	const boost::ptr_vector<Game_Enemy>& enemies = Game_EnemyParty().GetEnemies();
	for (boost::ptr_vector<Game_Enemy>::const_iterator it = enemies.begin();
		it != enemies.end(); ++it) {
		Game_Message::texts.push_back(it->GetName() + Data::terms.encounter);
	}
	Game_Message::message_waiting = true;

	return false;
}


bool Scene_Battle_Rpg2k::CheckWin() {
	if (!Game_EnemyParty().IsAnyAlive()) {
		Game_Temp::battle_result = Game_Temp::BattleVictory;
		SetState(State_Victory);

		Game_Message::texts.push_back(Data::terms.victory);

		std::stringstream ss;
		ss << Game_EnemyParty().GetExp() << Data::terms.exp_received;
		Game_Message::texts.push_back(ss.str());

		ss.str("");
		ss << Data::terms.gold_recieved_a << " " << Game_EnemyParty().GetMoney() << Data::terms.gold << Data::terms.gold_recieved_b;
		Game_Message::texts.push_back(ss.str());

		Game_System::BgmPlay(Data::system.battle_end_music);

		return true;
	}

	return false;
}

bool Scene_Battle_Rpg2k::CheckLose() {
	if (!Game_Party().IsAnyAlive()) {
		Game_Temp::battle_result = Game_Temp::BattleDefeat;
		SetState(State_Defeat);
		Game_Message::texts.push_back(Data::terms.defeat);

		return true;
	}

	return false;
}

bool Scene_Battle_Rpg2k::CheckAbort() {
	/*if (!Game_Battle::terminate)
		return;
	Game_Temp::battle_result = Game_Temp::BattleAbort;
	Scene::Pop();*/

	return false;
}

bool Scene_Battle_Rpg2k::CheckFlee() {
	/*if (!Game_Battle::allies_flee)
		return;
	Game_Battle::allies_flee = false;
	Game_Temp::battle_result = Game_Temp::BattleEscape;
	Scene::Pop();*/

	return false;
}
