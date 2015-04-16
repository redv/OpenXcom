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
#ifndef OPENXCOM_DOGFIGHTBASESTATE_H
#define OPENXCOM_DOGFIGHTBASESTATE_H

#include "DogfightState.h"
#include <vector>
#include <string>

namespace OpenXcom
{

class ImageButton;
class Text;
class Surface;
class InteractiveSurface;
class Timer;
class Globe;
class Craft;
class Ufo;
class CraftWeaponProjectile;

/**
 * Shows a dogfight between a player base and an UFO.
 */
class DogfightBaseState : public DogfightState
{
private:
	Base *_base;
	bool _ufoAttacked;

public:
	/// Creates the Dogfight state.
	DogfightBaseState(Globe *globe, Base *base, Ufo *ufo);
	/// Cleans up the Dogfight state.
	~DogfightBaseState();
	/// Moves the craft.
	void update();
	/// Handler for clicking the Minimize button.
	void btnMinimizeClick(Action *action);
	/// Handler for pressing the Standoff button.
	void btnStandoffPress(Action *action);
	/// Handler for pressing the Cautious Attack button (disable base defense).
	void btnCautiousPress(Action *action);
	/// Handler for pressing the Standard Attack button.
	void btnStandardPress(Action *action);
	/// Handler for clicking the Preview graphic.
	void previewClick(Action *action);
	/// Handler for clicking the minimized interception window icon.
	void btnMinimizedIconClick(Action *action);
};

}

#endif
