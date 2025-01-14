//
// CClientUse.cpp
// Copyright Menace Software (www.menasoft.com).
//
#include "graysvr.h"	// predef header.
#include "cclient.h"

bool CClient::Cmd_Use_Item( CItem * pItem, bool fTestTouch )
{
	// Assume we can see the target.
	// called from Event_DoubleClick

	if ( pItem == NULL )
		return false;

	if ( fTestTouch )
	{
		// CanTouch handles priv level compares for chars
		if ( ! m_pChar->CanUse( pItem, false ))
		{
			if ( ! m_pChar->CanTouch( pItem ))
			{
				SysMessage(( m_pChar->IsStatFlag( STATF_DEAD )) ?
					"Your ghostly hand passes through the object." :
					"You can't reach that." );
				return false;
			}

			SysMessage( "You can't use this where it is." );
			return false;
		}
	}

	CItemBase * pItemDef = pItem->Item_GetDef();

	if ( pItemDef->IsTypeEquippable() && ! pItem->IsItemEquipped() && pItemDef->GetEquipLayer() )
	{
		if ( pItem->IsType(IT_LIGHT_OUT) && pItem->IsItemInContainer())
		{
mustequipthis:
			if ( ! m_pChar->CanMove( pItem ) ||
				! m_pChar->ItemEquip( pItem ))
			{
				SysMessage( "The item should be equipped to use." );
				return false;
			}
		}
		else if ( ! pItem->IsType(IT_LIGHT_OUT) &&
			! pItem->IsType(IT_LIGHT_LIT) &&
			! pItem->IsType(IT_SPELLBOOK))
		{
			goto mustequipthis;
		}
	}

	SetTargMode();
	m_Targ_UID = pItem->GetUID();	// probably already set anyhow.
	m_tmUseItem.m_pParent = pItem->GetParent();	// Cheat Verify.

	if ( pItem->OnTrigger( ITRIG_DCLICK, m_pChar ) == TRIGRET_RET_TRUE )
	{
		return true;
	}

	// Use types of items. (specific to client)
	switch ( pItem->GetType() )
	{

	case IT_TRACKER:
		{
			DIR_TYPE dir = (DIR_TYPE) ( DIR_QTY + 1 );	// invalid value.
			if ( ! m_pChar->Skill_Tracking( pItem->m_uidLink, dir ))
			{
				if ( pItem->m_uidLink.IsValidUID())
				{
					SysMessage( "You cannot locate your target" );
				}
				m_Targ_UID = pItem->GetUID();
				addTarget( CLIMODE_TARG_LINK, "Who do you want to attune to ?" );
			}
		}
		return true;

	case IT_TRACK_ITEM:		// 109 - track a id or type of item.
	case IT_TRACK_CHAR:		// 110 = track a char or range of char id's
		// Track a type of item or creature.
		{
			// Look in the local area for this item or char.


		}
		break;

	case IT_SHAFT:
	case IT_FEATHER:
		Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_bolts" ));
		return true;

	case IT_FISH_POLE:	// Just be near water ?
		addTarget( CLIMODE_TARG_USE_ITEM, "Where would you like to fish?", true );
		return true;

	case IT_DEED:
		addTargetDeed( pItem );
		return true;

	case IT_METRONOME:
		pItem->OnTick();
		return true;

	case IT_EQ_BANK_BOX:
	case IT_EQ_VENDOR_BOX:
		g_Log.Event( LOGL_WARN|LOGM_CHEAT, 
			"%x:Cheater '%s' is using 3rd party tools to open bank box\n", 
			m_Socket.GetSocket(), (LPCTSTR) GetAccount()->GetName());
		return false;

	case IT_CONTAINER_LOCKED:
	case IT_SHIP_HOLD_LOCK:
		if ( ! m_pChar->GetPackSafe()->ContentFindKeyFor( pItem ))
		{
			SysMessage( "This item is locked.");
			if ( ! IsPriv( PRIV_GM ))
				return false;
		}

	case IT_CORPSE:
	case IT_SHIP_HOLD:
	case IT_CONTAINER:
	case IT_TRASH_CAN:
		{
			CItemContainer * pPack = dynamic_cast <CItemContainer *>(pItem);
			if ( ! m_pChar->Skill_Snoop_Check( pPack ))
			{
				if ( ! addContainerSetup( pPack ))
					return false;
			}
		}
		return true;

	case IT_GAME_BOARD:
		if ( ! pItem->IsTopLevel())
		{
			SysMessage( "Can't open game board in a container" );
			return false;
		}
		{
			CItemContainer* pBoard = dynamic_cast <CItemContainer *>(pItem);
			ASSERT(pBoard);
			pBoard->Game_Create();
			addContainerSetup( pBoard );
		}
		return true;

	case IT_BBOARD:
		addBulletinBoard( dynamic_cast<CItemContainer *>(pItem));
		return true;

	case IT_SIGN_GUMP:
		// Things like grave stones and sign plaques.
		// Need custom gumps.
		{
		GUMP_TYPE gumpid = pItemDef->m_ttContainer.m_gumpid;
		if ( ! gumpid )
		{
			return false;
		}
		addGumpTextDisp( pItem, gumpid,	pItem->GetName(),
			( pItem->IsIndividualName()) ? pItem->GetName() : NULL );
		}
		return true;

	case IT_BOOK:
	case IT_MESSAGE:
	case IT_EQ_NPC_SCRIPT:
		if ( ! addBookOpen( pItem ))
		{
			SysMessage( "The book apears to be ruined!" );
		}
		return true;

	case IT_STONE_GUILD:
	case IT_STONE_TOWN:
	case IT_STONE_ROOM:
		// Guild and town stones.
		{
			CItemStone * pStone = dynamic_cast<CItemStone*>(pItem);
			if ( pStone == NULL )
				break;
			pStone->Use_Item( this );
		}
		return true;

	case IT_ADVANCE_GATE:
		// Upgrade the player to the skills of the selected NPC script.
		m_pChar->Use_AdvanceGate( pItem );
		return true;

	case IT_POTION:
		if ( ! m_pChar->CanMove( pItem ))	// locked down decoration.
		{
			SysMessage( "You can't move the item." );
			return false;
		}
		if ( RES_GET_INDEX(pItem->m_itPotion.m_Type) == SPELL_Poison )
		{
			// ask them what they want to use the poison on ?
			// Poisoning or poison self ?
			addTarget( CLIMODE_TARG_USE_ITEM, "What do you want to use this on?", false, true );
			return true;
		}
		else if ( RES_GET_INDEX(pItem->m_itPotion.m_Type) == SPELL_Explosion )
		{
			// Throw explode potion.
			if ( ! m_pChar->ItemPickup( pItem, 1 ))
				return true;
			m_tmUseItem.m_pParent = pItem->GetParent();	// Cheat Verify FIX.

			addTarget( CLIMODE_TARG_USE_ITEM, "Where do you want to throw the potion?", true, true );
			// Put the potion in our hand as well. (and set it's timer)
			pItem->m_itPotion.m_tick = 4;	// countdown to explode purple.
			pItem->SetTimeout( TICK_PER_SEC );
			pItem->m_uidLink = m_pChar->GetUID();
			return true;
		}
		if ( m_pChar->Use_Drink( pItem ))
			return true;
		break;

	case IT_ANIM_ACTIVE:
		SysMessage( "The item is in use" );
		return false;

	case IT_CLOCK:
		addObjMessage( m_pChar->GetTopSector()->GetLocalGameTime(), pItem );
		return true;

	case IT_SPAWN_CHAR:
		SysMessage( "You negate the spawn" );
		pItem->Spawn_KillChildren();
		return true;

	case IT_SPAWN_ITEM:
		SysMessage( "You trigger the spawn." );
		pItem->Spawn_OnTick( true );
		return true;

	case IT_SHRINE:
		if ( m_pChar->OnSpellEffect( SPELL_Resurrection, m_pChar, 1000, pItem ))
			return true;
		SysMessage( "You have a feeling of holiness" );
		return true;

	case IT_SHIP_TILLER:
		// dclick on tiller man.
		pItem->Speak( "Arrg stop that.", 0, TALKMODE_SAY, FONT_NORMAL );
		return true;

		// A menu or such other action ( not instantanious)

	case IT_WAND:
	case IT_SCROLL:	// activate the scroll.
		return Cmd_Skill_Magery( (SPELL_TYPE)RES_GET_INDEX(pItem->m_itWeapon.m_spell), pItem );

	case IT_RUNE:
		// name the rune.
		if ( ! m_pChar->CanMove( pItem, true ))
		{
			return false;
		}
		addPromptConsole( CLIMODE_PROMPT_NAME_RUNE, "What is the new name of the rune ?" );
		return true;

	case IT_CARPENTRY:
		// Carpentry type tool
		Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_carpentry" ));
		return true;

		// Solve for the combination of this item with another.
	case IT_FORGE:
		addTarget( CLIMODE_TARG_USE_ITEM, "Select ore to smelt." );
		return true;
	case IT_ORE:
		// just use the nearest forge.
		return m_pChar->Skill_Mining_Smelt( pItem, NULL );
	case IT_INGOT:
		return Cmd_Skill_Smith( pItem );

	case IT_KEY:
	case IT_KEYRING:
		if ( pItem->GetTopLevelObj() != m_pChar && ! m_pChar->IsPriv(PRIV_GM))
		{
			SysMessage( "The key must be on your person" );
			return false;
		}
		addTarget( CLIMODE_TARG_USE_ITEM, "Select item to use the key on.", false, true );
		return true;

	case IT_BANDAGE:		// SKILL_HEALING, or SKILL_VETERINARY
		addTarget( CLIMODE_TARG_USE_ITEM, "What do you want to use this on?", false, false );
		return true;

	case IT_BANDAGE_BLOOD:	// Clean the bandages.
	case IT_COTTON:			// use on a spinning wheel.
	case IT_WOOL:			// use on a spinning wheel.
	case IT_YARN:			// Use this on a loom.
	case IT_THREAD: 		// Use this on a loom.
	case IT_COMM_CRYSTAL:
	case IT_SCISSORS:
	case IT_LOCKPICK:		// Use on a locked thing.
	case IT_CARPENTRY_CHOP:
	case IT_WEAPON_MACE_STAFF:
	case IT_WEAPON_MACE_SMITH:	// Can be used for smithing ?
	case IT_WEAPON_MACE_SHARP:	// war axe can be used to cut/chop trees.
	case IT_WEAPON_SWORD:		// 23 =
	case IT_WEAPON_FENCE:		// 24 = can't be used to chop trees.
	case IT_WEAPON_AXE:
		addTarget( CLIMODE_TARG_USE_ITEM, "What do you want to use this on?", false, true );
		return true;

	case IT_MEAT_RAW:
	case IT_FOOD_RAW:
		addTarget( CLIMODE_TARG_USE_ITEM, "What do you want to cook this on?" );
		return true;
	case IT_FISH:
		SysMessage( "Use a knife to cut this up" );
		return true;
	case IT_TELESCOPE:
		// Big telescope.
		SysMessage( "Wow you can see the sky!" );
		return true;
	case IT_MAP:
	case IT_MAP_BLANK:
		if ( ! pItem->m_itMap.m_right && ! pItem->m_itMap.m_bottom )
		{
			Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_cartography" ));
		}
		else if ( ! IsPriv(PRIV_GM) && pItem->GetTopLevelObj() != m_pChar )	// must be on your person.
		{
			SysMessage( "You must possess the map to get a good look at it." );
		}
		else
		{
			addMap( dynamic_cast <CItemMap*>( pItem ));
		}
		return true;

	case IT_CANNON_BALL:
		{
			CGString sTmp;
			sTmp.Format( "What do you want to use the %s on?", (LPCTSTR) pItem->GetName());
			addTarget( CLIMODE_TARG_USE_ITEM, sTmp );
		}
		return true;

	case IT_CANNON_MUZZLE:
		// Make sure the cannon is loaded.
		if ( ! m_pChar->CanUse( pItem, false ))
			return( false );
		if ( ! ( pItem->m_itCannon.m_Load & 1 ))
		{
			addTarget( CLIMODE_TARG_USE_ITEM, "The cannon needs powder" );
			return true;
		}
		if ( ! ( pItem->m_itCannon.m_Load & 2 ))
		{
			addTarget( CLIMODE_TARG_USE_ITEM, "The cannon needs shot" );
			return true;
		}
		addTarget( CLIMODE_TARG_USE_ITEM, "Armed and ready. What is the target?", false, true );
		return true;

	case IT_CRYSTAL_BALL:
		// Gaze into the crystal ball.

		return true;

	case IT_WEAPON_MACE_CROOK:
		addTarget( CLIMODE_TARG_USE_ITEM, "What would you like to herd?", false, true );
		return true;

	case IT_SEED:
	case IT_PITCHER_EMPTY:
		{ // not a crime.
			CGString sTmp;
			sTmp.Format( "Where do you want to use the %s?", (LPCTSTR) pItem->GetName());
			addTarget( CLIMODE_TARG_USE_ITEM, sTmp, true );
		}
		return true;

	case IT_WEAPON_MACE_PICK:
		{	// Mine at the location. (possible crime?)
			CGString sTmp;
			sTmp.Format( "Where do you want to use the %s?", (LPCTSTR) pItem->GetName());
			addTarget( CLIMODE_TARG_USE_ITEM, sTmp, true, true );
		}
		return true;

	case IT_SPELLBOOK:
		addSpellbookOpen( pItem );
		return true;

	case IT_HAIR_DYE:
		if (!m_pChar->LayerFind( LAYER_BEARD ) && !m_pChar->LayerFind( LAYER_HAIR ))
		{
			SysMessage("You have no hair to dye!");
			return true;
		}
		Dialog_Setup( CLIMODE_DIALOG_HAIR_DYE, g_Cfg.ResourceGetIDType( RES_DIALOG, "d_HAIR_DYE" ), m_pChar );
		return true;
	case IT_DYE:
		addTarget( CLIMODE_TARG_USE_ITEM, "Which dye vat will you use this on?" );
		return true;
	case IT_DYE_VAT:
		addTarget( CLIMODE_TARG_USE_ITEM, "Select the object to use this on.", false, true );
		return true;

	case IT_MORTAR:
		// If we have a mortar then do alchemy.
		addTarget( CLIMODE_TARG_USE_ITEM, "What reagent you like to make a potion out of?" );
		return true;
	case IT_POTION_EMPTY:
		if ( ! m_pChar->ContentFind(RESOURCE_ID(RES_TYPEDEF,IT_MORTAR)))
		{
			SysMessage( "You have no mortar and pestle." );
			return( false );
		}
		addTarget( CLIMODE_TARG_USE_ITEM, "What reagent you like to make a potion out of?" );
		return true;
	case IT_REAGENT:
		// Make a potion with this. (The best type we can)
		if ( ! m_pChar->ContentFind( RESOURCE_ID(RES_TYPEDEF,IT_MORTAR)))
		{
			SysMessage( "You have no mortar and pestle." );
			return false;
		}
		Cmd_Skill_Alchemy( pItem );
		return true;

		// Put up menus to better decide what to do.

	case IT_TINKER_TOOLS:	// Tinker tools.
		Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_tinker" ));
		return true;
	case IT_SEWING_KIT:	// IT_SEWING_KIT Sew with materials we have on hand.
		{
			CGString sTmp;
			sTmp.Format( "What do you want to use the %s on?", (LPCTSTR) pItem->GetName());
			addTarget( CLIMODE_TARG_USE_ITEM, sTmp );
		}
		return true;

	case IT_SCROLL_BLANK:
		Cmd_Skill_Inscription();
		return true;

	default:
		// An NPC could use it this way.
		if ( m_pChar->Use_Item( pItem ))
			return( true );
		break;
	}

	SysMessage( "You can't think of a way to use that item.");
	return( false );
}

void CClient::Cmd_EditItem( CObjBase * pObj, int iSelect )
{
	// ARGS:
	//   iSelect == -1 = setup.
	//   m_Targ_Text = what are we doing to it ?
	//
	if ( pObj == NULL )
		return;

	CContainer * pContainer = dynamic_cast <CContainer *> (pObj);
	if ( pContainer == NULL )
	{
		addGumpDialogProps( pObj->GetUID());
		return;
	}

	if ( iSelect == 0 )
		return;	// cancelled.

	if ( iSelect > 0 )
	{
		// We selected an item.
		if ( iSelect >= COUNTOF(m_tmMenu.m_Item))
			return;

		if ( m_Targ_Text.IsEmpty())
		{
			addGumpDialogProps( m_tmMenu.m_Item[iSelect] );
		}
		else
		{
			OnTarg_Obj_Set( CGrayUID( m_tmMenu.m_Item[iSelect] ).ObjFind() );
		}
		return;
	}

	CMenuItem item[MAX_MENU_ITEMS];	// Most as we want to display at one time.
	item[0].m_sText.Format( "Contents of %s", (LPCTSTR) pObj->GetName());

	int count=0;
	for ( CItem * pItem = pContainer->GetContentHead(); pItem != NULL; pItem = pItem->GetNext())
	{
		if ( count >= COUNTOF( item ))
			break;
		m_tmMenu.m_Item[++count] = pItem->GetUID();
		item[count].m_sText = pItem->GetName();
		ITEMID_TYPE idi = pItem->GetDispID();
		item[count].m_id = idi;
	}

	addItemMenu( CLIMODE_MENU_EDIT, item, count, pObj );
}

bool CClient::Cmd_CreateItem( ITEMID_TYPE id, bool fStatic )
{
	// make an item near by.
	m_tmAdd.m_id = id;
	m_tmAdd.m_fStatic = fStatic;
	return addTargetItems( CLIMODE_TARG_ADDITEM, m_tmAdd.m_id );
}

bool CClient::Cmd_CreateChar( CREID_TYPE id, SPELL_TYPE iSpell, bool fPet )
{
	// make a creature near by. (GM or script used only)
	// "ADDNPC"
	// spell = SPELL_Summon

	ASSERT(m_pChar);
	m_tmSkillMagery.m_Spell = iSpell;	// m_atMagery.m_Spell
	m_tmSkillMagery.m_SummonID = id;				// m_atMagery.m_SummonID
	m_tmSkillMagery.m_fSummonPet = fPet;
	if ( ! m_Targ_PrvUID.IsValidUID())
	{
		m_Targ_PrvUID = m_pChar->GetUID();	// what id this was already a scroll.
	}

	const CSpellDef * pSpellDef = g_Cfg.GetSpellDef( iSpell );
	ASSERT( pSpellDef );

	return addTargetChars( CLIMODE_TARG_SKILL_MAGERY, id, pSpellDef->IsSpellType( SPELLFLAG_HARM ));
}

bool CClient::Cmd_Skill_Menu( RESOURCE_ID_BASE rid, int iSelect )
{
	// Build the skill menu for the curent active skill.
	// Only list the things we have skill and ingrediants to make.
	//
	// ARGS:
	//	m_Targ_UID = the object used to get us here.
	//  rid = which menu ?
	//	iSelect = -2 = Just a test of the whole menu.,
	//	iSelect = -1 = 1st setup.
	//	iSelect = 0 = cancel
	//	iSelect = x = execute the selection.
	//
	// RETURN: false = fail/cancel the skill.
	//   m_tmMenu.m_Item = the menu entries.

	ASSERT(m_pChar);
	if ( iSelect == 0 || rid.GetResType() != RES_SKILLMENU )
		return( false );

	int iDifficulty = 0;

	// Find section.
	CResourceLock s;
	if ( ! g_Cfg.ResourceLock( s, rid ))
	{
		return false;
	}

	// Get title line
	if ( ! s.ReadKey())
		return( false );

	CMenuItem item[ min( COUNTOF( m_tmMenu.m_Item ), MAX_MENU_ITEMS ) ];
	if ( iSelect < 0 )
	{
		item[0].m_sText = s.GetKey();
		if ( iSelect == -1 )
		{
			m_tmMenu.m_ResourceID = rid;
		}
	}

	bool fSkip = false;	// skip this if we lack resources or skill.
	bool fSuccess = false;
	int iOnCount = 0;
	int iShowCount = 0;
	bool fShowMenu = false;	// are we showing a menu?

	while ( s.ReadKeyParse())
	{
		if ( s.IsKeyHead( "ON", 2 ))
		{
			// a new option to look at.
			fSkip = false;
			iOnCount ++;

			if ( iSelect < 0 )	// building up the list.
			{
				if ( iSelect < -1 && iShowCount >= 1 )		// just a test. so we are done.
				{
					return( true );
				}
				iShowCount ++;
				if ( ! item[iShowCount].ParseLine( s.GetArgRaw(), NULL, m_pChar ))
				{
					// remove if the item is invalid.
					iShowCount --;
					fSkip = true;
					continue;
				}
				if ( iSelect == -1 )
				{
					m_tmMenu.m_Item[iShowCount] = iOnCount;
				}
				if ( iShowCount >= COUNTOF( item )-1 )
					break;
			}
			else
			{
				if ( iOnCount > iSelect )	// we are done.
					break;
			}
			continue;
		}

		if ( fSkip ) // we have decided we cant do this option.
			continue;
		if ( iSelect > 0 && iOnCount != iSelect ) // only interested in the selected option
			continue;

		// Check for a skill / non-consumables required.
		if ( s.IsKey("TEST"))
		{
			CResourceQtyArray skills( s.GetArgStr());
			if ( ! skills.IsResourceMatchAll(m_pChar))
			{
				iShowCount--;
				fSkip = true;
			}
			continue;
		}
		if ( s.IsKey("TESTIF"))
		{
			m_pChar->ParseText( s.GetArgRaw(), m_pChar );
			if ( ! s.GetArgVal())
			{
				iShowCount--;
				fSkip = true;
			}
			continue;
		}

		// select to execute any entries here ?
		if ( iOnCount == iSelect )
		{
			m_pChar->m_Act_Targ = m_Targ_UID;

			// Execute command from script
			TRIGRET_TYPE tRet = m_pChar->OnTriggerRun( s, TRIGRUN_SINGLE_EXEC, m_pChar );
			if ( tRet == TRIGRET_RET_TRUE )
			{
				return( false );
			}

			fSuccess = true;	// we are good. but continue til the end
		}
		else
		{
			ASSERT( iSelect < 0 );

			if ( s.IsKey("SKILLMENU"))
			{
				// Test if there is anything in this skillmenu we can do.
				if ( ! Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, s.GetArgStr()), iSelect-1 ))
				{
					iShowCount--;
					fSkip = true;
				}
				else
				{
					fShowMenu = true;
					ASSERT( ! fSkip );
				}
				continue;
			}
			if ( s.IsKey("MAKEITEM"))
			{
				// test if i can make this item using m_Targ_UID.
				// There should ALWAYS be a valid id here.
				if ( ! m_pChar->Skill_MakeItem( (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ITEMDEF, s.GetArgStr()),
					m_Targ_UID, SKTRIG_SELECT ))
				{
					iShowCount--;
					fSkip = true;
				}
				continue;
			}
		}
	}

	if ( iSelect < -1 )		// just a test.
	{
		return( iShowCount ? true : false );
	}
	if ( iSelect > 0 )	// seems our resources disappeared.
	{
		if ( ! fSuccess )
		{
			SysMessage( "You can't make that." );
		}
		return( fSuccess );
	}


	if ( ! iShowCount )
	{
		SysMessage( "You can't make anything with what you have." );
		return( false );
	}
	if ( iShowCount == 1 && fShowMenu )
	{
		// If there is just one menu then select it.
		return( Cmd_Skill_Menu( rid, m_tmMenu.m_Item[1] ));
	}
	addItemMenu( CLIMODE_MENU_SKILL, item, iShowCount );
	return( true );
}

bool CClient::Cmd_Skill_Magery( SPELL_TYPE iSpell, CObjBase * pSrc )
{
	// start casting a spell. prompt for target.
	// pSrc = you the char.
	// pSrc = magic object is source ?

	static const TCHAR sm_Txt_Summon[] = "Where would you like to summon the creature ?";

	// Do we have the regs ? etc.
	ASSERT(m_pChar);
	if ( ! m_pChar->Spell_CanCast( iSpell, true, pSrc, true ))
		return false;

	const CSpellDef * pSpellDef = g_Cfg.GetSpellDef( iSpell );
	ASSERT(pSpellDef);

	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "%x:Cast Spell %d='%s'\n", m_Socket.GetSocket(), iSpell, pSpellDef->GetName()));
	}

	SetTargMode();
	m_tmSkillMagery.m_Spell = iSpell;	// m_atMagery.m_Spell
	m_Targ_UID = m_pChar->GetUID();	// default target.
	m_Targ_PrvUID = pSrc->GetUID();	// source of the spell.

	LPCTSTR pPrompt = "Select Target";
	switch ( iSpell )
	{
	case SPELL_Recall:
		pPrompt = "Select rune to recall from.";
		break;
	case SPELL_Blade_Spirit:
		pPrompt = sm_Txt_Summon;
		break;
	case SPELL_Summon:
		return Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_summon" ));
	case SPELL_Mark:
		pPrompt = "Select rune to mark.";
		break;
	case SPELL_Gate_Travel:	// gate travel
		pPrompt = "Select rune to gate from.";
		break;
	case SPELL_Polymorph:
		// polymorph creature menu.
		if ( IsPriv(PRIV_GM))
		{
			pPrompt = "Select creature to polymorph.";
			break;
		}
		return Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_polymorph" ));

	case SPELL_Earthquake:
		// cast immediately with no targetting.
		m_pChar->m_atMagery.m_Spell = SPELL_Earthquake;
		m_pChar->m_Act_Targ = m_Targ_UID;
		m_pChar->m_Act_TargPrv = m_Targ_PrvUID;
		m_pChar->m_Act_p = m_pChar->GetTopPoint();
		return( m_pChar->Skill_Start( SKILL_MAGERY ));

	case SPELL_Resurrection:
		pPrompt = "Select ghost to resurrect.";
		break;
	case SPELL_Vortex:
	case SPELL_Air_Elem:
	case SPELL_Daemon:
	case SPELL_Earth_Elem:
	case SPELL_Fire_Elem:
	case SPELL_Water_Elem:
		pPrompt = sm_Txt_Summon;
		break;

		// Necro spells
	case SPELL_Summon_Undead: // Summon an undead
		pPrompt = sm_Txt_Summon;
		break;
	case SPELL_Animate_Dead: // Corpse to zombie
		pPrompt = "Choose a corpse";
		break;
	case SPELL_Bone_Armor: // Skeleton corpse to bone armor
		pPrompt = "Chose a skeleton";
		break;
	}

	addTarget( CLIMODE_TARG_SKILL_MAGERY, pPrompt,
		! pSpellDef->IsSpellType( SPELLFLAG_TARG_OBJ | SPELLFLAG_TARG_CHAR ),
		pSpellDef->IsSpellType( SPELLFLAG_HARM ));
	return( true );
}

bool CClient::Cmd_Skill_Tracking( int track_sel, bool fExec )
{
	// look around for stuff.

	ASSERT(m_pChar);
	if ( track_sel < 0 )
	{
		// Initial pre-track setup.
		if ( m_pChar->m_pArea->IsFlag( REGION_FLAG_SHIP ) &&
			! m_pChar->ContentFind( RESOURCE_ID(RES_TYPEDEF,IT_SPY_GLASS)) )
		{
			SysMessage( "You need a Spyglass to use tracking here." );
			return( false );
		}

		// Tacking (unlike other skills) is used during menu setup.
		m_pChar->Skill_Fail( true );	// Kill previous skill.

		CMenuItem item[6];
		item[0].m_sText = "Tracking";

		item[1].m_id = ITEMID_TRACK_HORSE;
		item[1].m_sText = "Animals";
		item[2].m_id = ITEMID_TRACK_OGRE;
		item[2].m_sText = "Monsters";
		item[3].m_id = ITEMID_TRACK_MAN;
		item[3].m_sText = "Humans";
		item[4].m_id = ITEMID_TRACK_MAN_NAKED;
		item[4].m_sText = "Players";
		item[5].m_id = ITEMID_TRACK_WISP;
		item[5].m_sText = "Anything that moves";

		m_tmMenu.m_Item[0] = 0;
		addItemMenu( CLIMODE_MENU_SKILL_TRACK_SETUP, item, 5 );
		return( true );
	}

	if ( track_sel ) // Not Cancelled
	{
		ASSERT( ((WORD)track_sel) < COUNTOF( m_tmMenu.m_Item ));
		if ( fExec )
		{
			// Tracking menu got us here. Start tracking the selected creature.
			m_pChar->SetTimeout( 1*TICK_PER_SEC );
			m_pChar->m_Act_Targ = m_tmMenu.m_Item[track_sel]; // selected UID
			m_pChar->Skill_Start( SKILL_TRACKING );
			return true;
		}

		bool fGM = IsPriv(PRIV_GM);

		static const NPCBRAIN_TYPE sm_Track_Brain[] =
		{
			NPCBRAIN_QTY,	// not used here.
			NPCBRAIN_ANIMAL,
			NPCBRAIN_MONSTER,
			NPCBRAIN_HUMAN,
			NPCBRAIN_NONE,	// players
			NPCBRAIN_QTY,	// anything.
		};

		if ( track_sel >= COUNTOF(sm_Track_Brain))
			track_sel = COUNTOF(sm_Track_Brain)-1;
		NPCBRAIN_TYPE track_type = sm_Track_Brain[ track_sel ];

		CMenuItem item[ min( MAX_MENU_ITEMS, COUNTOF( m_tmMenu.m_Item )) ];
		int count = 0;

		item[0].m_sText = "Tracking";
		m_tmMenu.m_Item[0] = track_sel;

		CWorldSearch AreaChars( m_pChar->GetTopPoint(), m_pChar->Skill_GetBase(SKILL_TRACKING)/20 + 10 );
		while (true)
		{
			CChar * pChar = AreaChars.GetChar();
			if ( pChar == NULL )
				break;
			if ( m_pChar == pChar )
				continue;
			if ( ! m_pChar->CanDisturb( pChar ))
				continue;

			CCharBase * pCharDef = pChar->Char_GetDef();
			NPCBRAIN_TYPE basic_type = pChar->GetCreatureType();
			if ( track_type != basic_type && track_type != NPCBRAIN_QTY )
			{
				if ( track_type != NPCBRAIN_NONE )		// no match.
				{
					continue;	
				}

				// players
				if ( ! pChar->m_pPlayer )
					continue;
				if ( ! fGM && basic_type != NPCBRAIN_HUMAN )	// can't track polymorphed person.
					continue;
			}
			
			if ( ! fGM && ! pCharDef->Can( CAN_C_WALK ))	// never moves or just swims.
				continue;

			count ++;
			item[count].m_id = pCharDef->m_trackID;
			item[count].m_sText = pChar->GetName();
			m_tmMenu.m_Item[count] = pChar->GetUID();
			if ( count >= COUNTOF( item )-1 )
				break;
		}

		if ( count )
		{
			// Some credit for trying.
			m_pChar->Skill_UseQuick( SKILL_TRACKING, 20 + Calc_GetRandVal( 30 ));
			addItemMenu( CLIMODE_MENU_SKILL_TRACK, item, count );
			return( true );
		}
		else
		{
			// Some credit for trying.
			m_pChar->Skill_UseQuick( SKILL_TRACKING, 10 + Calc_GetRandVal( 30 ));
		}
	}

	// Tracking failed or was cancelled .

	static LPCTSTR const sm_Track_FailMsg[] =
	{
		"Tracking Cancelled",
		"You see no signs of animals to track.",
		"You see no signs of monsters to track.",
		"You see no signs of humans to track.",
		"You see no signs of players to track.",
		"You see no signs to track."
	};

	SysMessage( sm_Track_FailMsg[track_sel] );
	return( false );
}

bool CClient::Cmd_Skill_Smith( CItem * pIngots )
{
	ASSERT(m_pChar);
	if ( pIngots == NULL || ! pIngots->IsType(IT_INGOT))
	{
		SysMessage( "You need ingots for smithing." );
		return( false );
	}
	ASSERT( m_Targ_UID == pIngots->GetUID());
	if ( pIngots->GetTopLevelObj() != m_pChar )
	{
		SysMessage( "The ingots must be on your person" );
		return( false );
	}

	// must have hammer equipped.
	CItem * pHammer = m_pChar->LayerFind( LAYER_HAND1 );
	if ( pHammer == NULL || ! pHammer->IsType(IT_WEAPON_MACE_SMITH))
	{
		SysMessage( "You must weild a smith hammer of some sort." );
		return( false );
	}

	// Select the blacksmith item type.
	// repair items or make type of items.
	if ( ! g_World.IsItemTypeNear( m_pChar->GetTopPoint(), IT_FORGE, 3 ))
	{
		SysMessage( "You must be near a forge to smith ingots" );
		return( false );
	}

	// Select the blacksmith item type.
	// repair items or make type of items.
	return( Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU,"sm_blacksmith" )));
}

bool CClient::Cmd_Skill_Inscription()
{
	// Select the scroll type to make.
	// iSelect = -1 = 1st setup.
	// iSelect = 0 = cancel
	// iSelect = x = execute the selection.
	// we should already be in inscription skill mode.

	ASSERT(m_pChar);

	CItem * pBlank = m_pChar->ContentFind( RESOURCE_ID(RES_TYPEDEF,IT_SCROLL_BLANK));
	if ( pBlank == NULL )
	{
		SysMessage( "You have no blank scrolls" );
		return( false );
	}

	return Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_inscription" ) );
}

bool CClient::Cmd_Skill_Alchemy( CItem * pReag )
{
	// SKILL_ALCHEMY

	if ( pReag == NULL )
		return( false );

	ASSERT(m_pChar);
	if ( ! m_pChar->CanUse( pReag, true ))
		return( false );

	if ( ! pReag->IsType(IT_REAGENT))
	{
		// That is not a reagent.
		SysMessage( "That is not a reagent." );
		return( false );
	}

	// Find bottles to put potions in.
	if ( ! m_pChar->ContentFind(RESOURCE_ID(RES_TYPEDEF,IT_POTION_EMPTY)))
	{
		SysMessage( "You have no bottles for your potion." );
		return( false );
	}

	m_Targ_UID = pReag->GetUID();

	// Put up a menu to decide formula ?
	return Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_alchemy" ));
}

bool CClient::Cmd_Skill_Cartography( int iLevel )
{
	// select the map type.

	ASSERT(m_pChar);
	if ( m_pChar->Skill_Wait(SKILL_CARTOGRAPHY))
		return( false );

	if ( ! m_pChar->ContentFind( RESOURCE_ID(RES_TYPEDEF,IT_MAP_BLANK)))
	{
		SysMessage( "You have no blank parchment to draw on" );
		return( false );
	}

	return( Cmd_Skill_Menu( g_Cfg.ResourceGetIDType( RES_SKILLMENU, "sm_cartography" )));
}

bool CClient::Cmd_SecureTrade( CChar * pChar, CItem * pItem )
{
	// Begin secure trading with a char. (Make the initial offer)
	ASSERT(m_pChar);

	// Trade window only if another PC.
	if ( ! pChar->IsClient())
	{
		return( pChar->NPC_OnItemGive( m_pChar, pItem ));
	}

	// Is there already a trade window open for this client ?
	CItem * pItemCont = m_pChar->GetContentHead();
	for ( ; pItemCont != NULL; pItemCont = pItemCont->GetNext())
	{
		if ( ! pItemCont->IsType(IT_EQ_TRADE_WINDOW))
			continue; // found it
		CItem * pItemPartner = pItemCont->m_uidLink.ItemFind(); // counterpart trade window.
		if ( pItemPartner == NULL )
			continue;
		CChar * pCharPartner = dynamic_cast <CChar*>( pItemPartner->GetParent());
		if ( pCharPartner != pChar )
			continue;
		CItemContainer * pCont = dynamic_cast <CItemContainer *>( pItemCont );
		ASSERT(pCont);
		pCont->ContentAdd( pItem );
		return( true );
	}

	// Open a new one.
	CItemContainer* pCont1 = dynamic_cast <CItemContainer*> (CItem::CreateBase( ITEMID_Bulletin1 ));
	ASSERT(pCont1);
	pCont1->SetType( IT_EQ_TRADE_WINDOW );
	CItemContainer* pCont2 = dynamic_cast <CItemContainer*> (CItem::CreateBase( ITEMID_Bulletin1 ));
	ASSERT(pCont2);
	pCont2->SetType( IT_EQ_TRADE_WINDOW );

	pCont1->m_itEqTradeWindow.m_fCheck = false;
	pCont1->m_uidLink = pCont2->GetUID();

	pCont2->m_itEqTradeWindow.m_fCheck = false;
	pCont2->m_uidLink = pCont1->GetUID();

	m_pChar->LayerAdd( pCont1, LAYER_SPECIAL );
	pChar->LayerAdd( pCont2, LAYER_SPECIAL );

	CCommand cmd;
	int len = sizeof(cmd.SecureTrade);

	cmd.SecureTrade.m_Cmd = XCMD_SecureTrade;
	cmd.SecureTrade.m_len = len;
	cmd.SecureTrade.m_action = SECURE_TRADE_OPEN;	// init
	cmd.SecureTrade.m_UID = pChar->GetUID();
	cmd.SecureTrade.m_UID1 = pCont1->GetUID();
	cmd.SecureTrade.m_UID2 = pCont2->GetUID();
	cmd.SecureTrade.m_fname = 1;
	strcpy( cmd.SecureTrade.m_charname, pChar->GetName());
	xSendPkt( &cmd, len );

	cmd.SecureTrade.m_UID = m_pChar->GetUID();
	cmd.SecureTrade.m_UID1 = pCont2->GetUID();
	cmd.SecureTrade.m_UID2 = pCont1->GetUID();
	strcpy( cmd.SecureTrade.m_charname, m_pChar->GetName());
	pChar->GetClient()->xSendPkt( &cmd, len );

	CPointMap pt( 30, 30, 9 );
	pCont1->ContentAdd( pItem, pt );
	return( true );
}

