#pragma once

#include "UObject/ObjectMacros.h"

#pragma region CompilationConstants
//__________________________________________________
#if WITH_EDITOR

#define TRUE_IF_EDITOR  true

#define FALSE_IF_EDITOR false

#else

	#define TRUE_IF_EDITOR  false

	#define FALSE_IF_EDITOR true

#endif //WITH_EDITOR
//__________________________________________________
#pragma endregion CompilationConstants



#pragma region Gameplay
//__________________________________________________

#pragma region AttributeComponent
// === //
#define PERCENTAGE_EMPTY 0.f
#define PERCENTAGE_FULL 100.f

// === //
#pragma endregion AttributeComponent


//__________________________________________________
#pragma endregion Gameplay


#pragma region GameplayTags
//__________________________________________________

#pragma region 3C
// === //
#define TAG_HURTBOX "Hurtbox"
#define TAG_HITBOX "Hitbox"
#define TAG_CABLESTART "CableStart"
#define TAG_CABLEEND "CableEnd"
// === //
#pragma endregion 3C

#pragma region Characters
// === //
#define TAG_PLAYER "Player"
#define TAG_ENEMY "Enemy"
// === //
#pragma endregion Characters

#pragma region Gpe
// === //
#define TAG_SLICEABLE "Sliceable"
#define TAG_FIXED "Fixed"
// === //
#pragma endregion Gpe

//__________________________________________________
#pragma endregion GameplayTags




#pragma region Input Action Name
//__________________________________________________

#define INPUT_UI_SELECT "UiSelect"
#define INPUT_UI_BACK "UiBack"

#define INPUT_UI_UP "UiUp"
#define INPUT_UI_DOWN "UiDown"
#define INPUT_UI_LEFT "UiLeft"
#define INPUT_UI_RIGHT "UiRight"

#define INPUT_UI_LEFTPAD_HORIZONTAL "UiLeftThumbstickHorizontal"
#define INPUT_UI_LEFTPAD_VERTICAL "UiLeftThumbstickVertical"

#define INPUT_UI_RIGHTPAD_HORIZONTAL "UiRightThumbstickHorizontal"
#define INPUT_UI_RIGHTPAD_VERTICAL "UiRightThumbstickVertical"

//__________________________________________________
#pragma endregion Input Action Name