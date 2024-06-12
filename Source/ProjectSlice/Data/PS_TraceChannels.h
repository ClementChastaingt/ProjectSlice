#pragma once

/**
 * File where to define ECC_GameTraceChannels
 */

#pragma region GameTraceChannel
//__________________________________________________
#define ECC_Player ECC_GameTraceChannel1
#define ECC_Enemy ECC_GameTraceChannel2
#define ECC_EnemyGhost ECC_GameTraceChannel3
#define ECC_Hitbox ECC_GameTraceChannel4
#define ECC_Hurtbox ECC_GameTraceChannel5
#define ECC_DefaultIgnore ECC_GameTraceChannel6
#define ECC_DefaultOverlap ECC_GameTraceChannel7
#define ECC_GPE ECC_GameTraceChannel8
#define ECC_Landscape ECC_GameTraceChannel9
#define ECC_Vegetation ECC_GameTraceChannel10
#define ECC_Slice ECC_GameTraceChannel11
#define ECC_Rope ECC_GameTraceChannel12
#define ECC_Parkour ECC_GameTraceChannel13


// #define ECC_GameTraceChannel16 ECC_GameTraceChannel14
// #define ECC_GameTraceChannel17 ECC_GameTraceChannel15
// #define ECC_GameTraceChannel18 ECC_GameTraceChannel16
// #define ECC_GameTraceChannel16 ECC_GameTraceChannel17
// #define ECC_GameTraceChannel16 ECC_GameTraceChannel18
// 18 is the max
//__________________________________________________
#pragma endregion GameTraceChannel

#pragma region ProfilePreset
//__________________________________________________

#define Profile_NoCollision "NoCollision"
#define Profile_BlockAll "BlockAll"
#define Profile_OverlapAll "OverlapAll"
#define Profile_PhysicActor "PhysicsActor"
#define Profile_Trigger "Trigger"
#define Profile_PlayerCapsule "PlayerCapsule"
#define Profile_EnemyCapsule "EnemyCapsule"
#define Profile_Hitbox "Hitbox"
#define Profile_Hurtbox "Hurtbox"
#define Profile_Landscape "Landscape"
#define Profile_Vegetation "Vegetation"
#define Profile_OverlapOnlyPlayer "OverlapOnlyPlayer"
#define Profile_OverlapOnlyEnemy "OverlapOnlyEnemy"
#define Profile_GPE "GPE"
#define Profile_Projectile "Projectile"
#define Profile_Rope "Rope"

//__________________________________________________
#pragma endregion ProfilePreset

#pragma region ObjectTypeQuery
//__________________________________________________
#define EOTQ_WorldStatic EObjectTypeQuery::ObjectTypeQuery1
#define EOTQ_WorldDynamic EObjectTypeQuery::ObjectTypeQuery2
#define EOTQ_Pawn EObjectTypeQuery::ObjectTypeQuery3
#define EOTQ_PhysicsBody EObjectTypeQuery::ObjectTypeQuery4
#define EOTQ_Vehicle EObjectTypeQuery::ObjectTypeQuery5
#define EOTQ_Destructible EObjectTypeQuery::ObjectTypeQuery6
// TODO: check if below are correct (will change with ECC definition order)
#define EOTQ_Player EObjectTypeQuery::ObjectTypeQuery7
#define EOTQ_Enemy EObjectTypeQuery::ObjectTypeQuery8
#define EOTQ_EnemyGhost EObjectTypeQuery::ObjectTypeQuery9
#define EOTQ_Hitbox EObjectTypeQuery::ObjectTypeQuery10
#define EOTQ_Hurtbox EObjectTypeQuery::ObjectTypeQuery11
#define EOTQ_GPE EObjectTypeQuery::ObjectTypeQuery12
#define EOTQ_Landscape EObjectTypeQuery::ObjectTypeQuery13
#define EOTQ_Vegetation EObjectTypeQuery::ObjectTypeQuery14

//__________________________________________________
#pragma endregion ObjectTypeQuery
