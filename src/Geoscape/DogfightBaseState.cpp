/*
 * Copyright 2010-2015 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "DogfightBaseState.h"
#include "DogfightState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Resource/ResourcePack.h"
#include "../Interface/ImageButton.h"
#include "../Interface/Text.h"
#include "Globe.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Craft.h"
#include "../Ruleset/RuleCraft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Ruleset/RuleCraftWeapon.h"
#include "../Savegame/Ufo.h"
#include "../Ruleset/RuleUfo.h"
#include "../Engine/Music.h"
#include "../Engine/RNG.h"
#include "../Engine/Sound.h"
#include "../Savegame/Base.h"
#include "../Savegame/CraftWeaponProjectile.h"
#include "../Savegame/Country.h"
#include "../Ruleset/RuleCountry.h"
#include "../Savegame/Region.h"
#include "../Ruleset/RuleRegion.h"
#include "../Savegame/AlienMission.h"
#include <cstdlib>

namespace OpenXcom
{

/**
 * Initializes all the elements in the Dogfight window.
 * @param game Pointer to the core game.
 * @param globe Pointer to the Geoscape globe.
 * @param craft Pointer to the craft intercepting.
 * @param ufo Pointer to the UFO being intercepted.
 */
DogfightBaseState::DogfightBaseState(Globe *globe, Base *base, Ufo *ufo) : DogfightState(globe, base->getBaseCraft(), ufo, "INTERWIN2.DAT"), _base(base), _ufoAttacked(false)
{
	_btnAggressive->setVisible(false);
	_base->getBaseCraft()->setDestination(ufo);

	_currentDist = _targetDist = int(base->getDistance(ufo) * 2.0 * 60.0 * (180.0 / M_PI));
	std::wostringstream sd;
	sd << _currentDist / 2;
	_txtDistance->setText(sd.str());
}

/**
 * Cleans up the dogfight state.
 */
DogfightBaseState::~DogfightBaseState()
{
	_base->setRechargeDefense(_ufoAttacked);
}

/**
 * Updates all the elements in the dogfight, including ufo movement,
 * weapons fire, projectile movement, ufo escape conditions,
 * craft and ufo destruction conditions, and retaliation mission generation, as applicable.
 */
void DogfightBaseState::update()
{
	bool finalRun = false;
	// Check if target's status hasn't been changed when window minimized.
	if (_minimized && _ufo->getStatus() != Ufo::FLYING)
	{
		endDogfight();
		return;
	}

	if (!_minimized)
	{
		animate();
		int escapeCounter = _ufo->getEscapeCountdown();
		if (_ufo->getStatus() == Ufo::FLYING)
		{
			if (escapeCounter > 0 && !_ufo->getInterceptionProcessed())
			{
				escapeCounter--;
				_ufo->setEscapeCountdown(escapeCounter);
				_ufo->setInterceptionProcessed(true);
			}
			// Check if UFO is breaking off.
			if (escapeCounter == 0)
			{
				_ufoBreakingOff = true;
				_targetDist = 800;
				finalRun = true;
				// TODO: set status
				//setStatus("STR_UFO_OUTRUNNING_INTERCEPTOR");
				_ufo->setSpeed(_ufo->getRules()->getMaxSpeed());
			}
		}
	}
	else
	{
		_currentDist = _targetDist = int(_base->getDistance(_ufo) * 2.0 * 60.0 * (180.0 / M_PI));
	}

	bool projectileInFlight = false;
	if (!_minimized)
	{
		int distanceChange = 0;

		// Update distance
		if (!_ufoBreakingOff)
		{
			if (_currentDist < _targetDist && !_ufo->isCrashed())
			{
				distanceChange = 4;
				if (_currentDist + distanceChange >_targetDist)
				{
					distanceChange = _targetDist - _currentDist;
				}
			}
			else if (_currentDist > _targetDist && !_ufo->isCrashed())
			{
				distanceChange = -2;
			}

			// don't let the interceptor mystically push or pull its fired projectiles
			for (std::vector<CraftWeaponProjectile*>::iterator it = _projectiles.begin(); it != _projectiles.end(); ++it)
			{
				if ((*it)->getGlobalType() != CWPGT_BEAM && (*it)->getDirection() == D_UP) (*it)->setPosition((*it)->getPosition() + distanceChange);
			}
		}
		else
		{
			distanceChange = 4;

			// UFOs can try to outrun our missiles, don't adjust projectile positions here
			// If UFOs ever fire anything but beams, those positions need to be adjust here though.
		}

		_currentDist += distanceChange; 

		std::wostringstream ss;
		ss << _currentDist / 2;
		_txtDistance->setText(ss.str());

		// Move projectiles and check for hits.
		for (std::vector<CraftWeaponProjectile*>::iterator it = _projectiles.begin(); it != _projectiles.end(); ++it)
		{
			CraftWeaponProjectile *p = (*it);
			p->move();
			// Projectiles fired by interceptor.
			if (p->getDirection() == D_UP)
			{
				// Projectile reached the UFO - determine if it's been hit.
				if (((p->getPosition() >= _currentDist) || (p->getGlobalType() == CWPGT_BEAM && p->toBeRemoved())) && !_ufo->isCrashed() && !p->getMissed())
				{
					// UFO hit.
					if (RNG::percent((p->getAccuracy() * (100 + 300 / (5 - _ufoSize)) + 100) / 200))
					{
						// Formula delivered by Volutar
						int damage = RNG::generate(p->getDamage() / 2, p->getDamage());
						_ufo->setDamage(_ufo->getDamage() + damage);
						if (_ufo->isCrashed())
						{
							_ufo->setShotDownByCraftId(_craft->getUniqueId());
							_ufoBreakingOff = false;
							_ufo->setSpeed(0);
						}
						if (_ufo->getHitFrame() == 0)
						{
							_animatingHit = true;
							_ufo->setHitFrame(3);
						}

						setStatus("STR_UFO_HIT");
						_game->getResourcePack()->getSound("GEO.CAT", ResourcePack::UFO_HIT)->play();
						p->remove();
					}
					// Missed.
					else
					{
						if (p->getGlobalType() == CWPGT_BEAM)
						{
							p->remove();
						}
						else
						{
							p->setMissed(true);
						}
					}
				}
				// Check if projectile passed it's maximum range.
				if (p->getGlobalType() == CWPGT_MISSILE)
				{
					if (p->getPosition() / 8 >= p->getRange())
					{
						p->remove();
					}
					else if (!_ufo->isCrashed())
					{
						projectileInFlight = true;
					}
				}
			}
		}
		
		// Remove projectiles that hit or missed their target.
		for (std::vector<CraftWeaponProjectile*>::iterator it = _projectiles.begin(); it != _projectiles.end();)
		{
			if ((*it)->toBeRemoved() == true || ((*it)->getMissed() == true && (*it)->getPosition() <= 0))
			{
				delete *it;
				it = _projectiles.erase(it);
			}
			else
			{
				++it;
			}
		}

		// Handle weapons and craft distance.
		for (unsigned int i = 0; i < _craft->getRules()->getWeapons(); ++i)
		{
			CraftWeapon *w = _craft->getWeapons()->at(i);
			if (w == 0)
			{
				continue;
			}
			int wTimer;
			if (i == 0)
			{
				wTimer = _w1FireCountdown;
			}
			else
			{
				wTimer = _w2FireCountdown;
			}

			// Handle weapon firing
			if (wTimer == 0 && _currentDist <= w->getRules()->getRange() * 8 && w->getAmmo() > 0 && _mode != _btnStandoff 
				&& _mode != _btnDisengage && !_ufo->isCrashed())
			{
				_ufoAttacked = true;
				if (i == 0)
				{
					fireWeapon1();
				}
				else
				{
					fireWeapon2();
				}
			}
			else if (wTimer > 0)
			{
				if (i == 0)
				{
					_w1FireCountdown--;
				}
				else
				{
					_w2FireCountdown--;
				}
			}
		}
	}

	// Check when battle is over.
	if (_end == true && (((_currentDist > 640 || _minimized) && (_mode == _btnDisengage || _ufoBreakingOff == true)) || (_timeout == 0 && _ufo->isCrashed())))
	{
		if (_ufoBreakingOff)
		{
			_ufo->move();
		}
		endDogfight();
	}

	if (_currentDist > 640 && _ufoBreakingOff)
	{
		finalRun = true;
	}

	// End dogfight if craft is destroyed.
	if (!_end)
	{
		// End dogfight if UFO is crashed or destroyed.
		if (_ufo->isCrashed())
		{
			AlienMission *mission = _ufo->getMission();
			mission->ufoShotDown(*_ufo);
			// Check for retaliation trigger.
			if (!RNG::percent(4 * (24 - (int)(_game->getSavedGame()->getDifficulty()))))
			{
				// Spawn retaliation mission.
				std::string targetRegion;
				if (RNG::percent(50 - 6 * (int)(_game->getSavedGame()->getDifficulty())))
				{
					// Attack on UFO's mission region
					targetRegion = _ufo->getMission()->getRegion();
				}
				else
				{
					// Try to find and attack the originating base.
					targetRegion = _game->getSavedGame()->locateRegion(*_craft->getBase())->getRules()->getType();
					// TODO: If the base is removed, the mission is canceled.
				}
				// Difference from original: No retaliation until final UFO lands (Original: Is spawned).
				if (!_game->getSavedGame()->findAlienMission(targetRegion, OBJECTIVE_RETALIATION))
				{
					const RuleAlienMission &rule = *_game->getRuleset()->getAlienMission("STR_ALIEN_RETALIATION");
					AlienMission *mission = new AlienMission(rule);
					mission->setId(_game->getSavedGame()->getId("ALIEN_MISSIONS"));
					mission->setRegion(targetRegion, *_game->getRuleset());
					mission->setRace(_ufo->getAlienRace());
					mission->start();
					_game->getSavedGame()->getAlienMissions().push_back(mission);
				}
			}

			if (_ufo->isDestroyed())
			{
				if (_ufo->getShotDownByCraftId() == _craft->getUniqueId())
				{
					for (std::vector<Country*>::iterator country = _game->getSavedGame()->getCountries()->begin(); country != _game->getSavedGame()->getCountries()->end(); ++country)
					{
						if ((*country)->getRules()->insideCountry(_ufo->getLongitude(), _ufo->getLatitude()))
						{
							(*country)->addActivityXcom(_ufo->getRules()->getScore()*2);
							break;
						}
					}
					for (std::vector<Region*>::iterator region = _game->getSavedGame()->getRegions()->begin(); region != _game->getSavedGame()->getRegions()->end(); ++region)
					{
						if ((*region)->getRules()->insideRegion(_ufo->getLongitude(), _ufo->getLatitude()))
						{
							(*region)->addActivityXcom(_ufo->getRules()->getScore()*2);
							break;
						}
					}
					setStatus("STR_UFO_DESTROYED");
					_game->getResourcePack()->getSound("GEO.CAT", ResourcePack::UFO_EXPLODE)->play(); //11
				}
				_destroyUfo = true;
			}
			else
			{
				if (_ufo->getShotDownByCraftId() == _craft->getUniqueId())
				{
					setStatus("STR_UFO_CRASH_LANDS");
					_game->getResourcePack()->getSound("GEO.CAT", ResourcePack::UFO_CRASH)->play(); //10
					for (std::vector<Country*>::iterator country = _game->getSavedGame()->getCountries()->begin(); country != _game->getSavedGame()->getCountries()->end(); ++country)
					{
						if ((*country)->getRules()->insideCountry(_ufo->getLongitude(), _ufo->getLatitude()))
						{
							(*country)->addActivityXcom(_ufo->getRules()->getScore());
							break;
						}
					}
					for (std::vector<Region*>::iterator region = _game->getSavedGame()->getRegions()->begin(); region != _game->getSavedGame()->getRegions()->end(); ++region)
					{
						if ((*region)->getRules()->insideRegion(_ufo->getLongitude(), _ufo->getLatitude()))
						{
							(*region)->addActivityXcom(_ufo->getRules()->getScore());
							break;
						}
					}
				}
				if (!_globe->insideLand(_ufo->getLongitude(), _ufo->getLatitude()))
				{
					_ufo->setStatus(Ufo::DESTROYED);
					_destroyUfo = true;
				}
				else
				{
					_ufo->setSecondsRemaining(RNG::generate(24, 96)*3600);
					_ufo->setAltitude("STR_GROUND");
					if (_ufo->getCrashId() == 0)
					{
						_ufo->setCrashId(_game->getSavedGame()->getId("STR_CRASH_SITE"));
					}
				}
			}
			_timeout += 30;
			if (_ufo->getShotDownByCraftId() != _craft->getUniqueId())
			{
				_timeout += 50;
				_ufo->setHitFrame(3);
			}
			finalRun = true;

			if (_ufo->getStatus() == Ufo::LANDED)
			{
				_timeout += 30;
				finalRun = true;
				_ufo->setShootingAt(0);
			}
		}
	}

	if (!projectileInFlight && finalRun)
	{
		_end = true;
	}
}

/**
 * Minimizes the dogfight window.
 * @param action Pointer to an action.
 */
void DogfightBaseState::btnMinimizeClick(Action *)
{
	if (!_ufo->isCrashed() && !_ufoBreakingOff)
	{
		if (!_ufoAttacked)
		{
			setMinimized(true);
			_window->setVisible(false);
			_preview->setVisible(false);
			_btnStandoff->setVisible(false);
			_btnCautious->setVisible(false);
			_btnStandard->setVisible(false);
			_btnAggressive->setVisible(false);
			_btnDisengage->setVisible(false);
			_btnUfo->setVisible(false);
			_btnMinimize->setVisible(false);
			_battle->setVisible(false);
			_weapon1->setVisible(false);
			_range1->setVisible(false);
			_weapon2->setVisible(false);
			_range2->setVisible(false);
			_damage->setVisible(false);
			_txtAmmo1->setVisible(false);
			_txtAmmo2->setVisible(false);
			_txtDistance->setVisible(false);
			_preview->setVisible(false);
			_txtStatus->setVisible(false);
			_btnMinimizedIcon->setVisible(true);
			_txtInterceptionNumber->setVisible(true);
		}
		else
		{
			setStatus("STR_CANT_MINIMIZE_DURING_BATTLE");
		}
	}
}

/**
 * Switches to Standoff mode (maximum range).
 * @param action Pointer to an action.
 */
void DogfightBaseState::btnStandoffPress(Action *)
{
	if (!_ufo->isCrashed() && !_ufoBreakingOff)
	{
		_end = false;
		setStatus("STR_STANDOFF");
//		_targetDist = STANDOFF_DIST;
	}
}

/**
 * Cautious attack button (disable base defense).
 * @param action Pointer to an action.
 */
void DogfightBaseState::btnCautiousPress(Action *action)
{
	_base->setActiveDefense(false);
	DogfightState::btnDisengagePress(action);
	DogfightState::endDogfight();
}

/**
 * Switches to Standard mode (minimum weapon range).
 * @param action Pointer to an action.
 */
void DogfightBaseState::btnStandardPress(Action *)
{
	if (!_ufo->isCrashed() && !_ufoBreakingOff)
	{
		_end = false;
		setStatus("STR_STANDARD_ATTACK");
		if (_craft->getRules()->getWeapons() > 0 && _craft->getWeapons()->at(0) != 0)
		{
			_w1FireInterval = _craft->getWeapons()->at(0)->getRules()->getStandardReload();
		}
		if (_craft->getRules()->getWeapons() > 1 && _craft->getWeapons()->at(1) != 0)
		{
			_w2FireInterval = _craft->getWeapons()->at(1)->getRules()->getStandardReload();
		}
//		maximumDistance();
	}
}

/**
 * Hides the front view of the UFO.
 * @param action Pointer to an action.
 */
void DogfightBaseState::previewClick(Action *action)
{
	DogfightState::previewClick(action);
	_btnAggressive->setVisible(false);
}

/**
 * Maximizes the interception window.
 * @param action Pointer to an action.
 */
void DogfightBaseState::btnMinimizedIconClick(Action *action)
{
	DogfightState::btnMinimizedIconClick(action);
	_btnAggressive->setVisible(false);
}

}
