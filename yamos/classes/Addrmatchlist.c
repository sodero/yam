/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 1995-2000 by Marcel Beck <mbeck@yam.ch>
 Copyright (C) 2000-2001 by YAM Open Source Team

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 YAM Official Support Site : http://www.yam.ch
 YAM OpenSource project    : http://sourceforge.net/projects/yamos/

 $Id$

 Superclass:  MUIC_Window
 Description: Popup a list of addresses which match a given substring
***************************************************************************/

#include "Classes.h"

/* ---------------------------------- */
#define DECLARE(method) ULONG m_Addrmatchlist_## method (struct IClass *cl, Object *obj, struct MUIP_Addrmatchlist_## method *msg)
#define OVERLOAD(method) ULONG m_Addrmatchlist_## method (struct IClass *cl, Object *obj, Msg msg)
#define ATTR(attr) case MUIA_Addrmatchlist_## attr
/* ---------------------------------- */

struct Data
{
	Object *Matchlist, *String;
	BOOL Open;
};

ULONG AddrmatchlistGetSize (VOID) { return sizeof(struct Data); }

struct CustomABEntry
{
	LONG MatchField;
	STRPTR MatchString;
	struct ABEntry *MatchEntry;
};

HOOKPROTONH(ConstructFunc, struct CustomABEntry *, APTR pool, struct CustomABEntry *e)
{
	struct CustomABEntry *res;
	if(res = malloc(sizeof(struct CustomABEntry)))
		*res = *e;
	return res;
}
MakeStaticHook(ConstructHook, ConstructFunc);

HOOKPROTONH(DisplayFunc, LONG, STRPTR *array, struct CustomABEntry *e)
{
	static TEXT buf[SIZE_ADDRESS + 4];

	array[0] = e->MatchEntry->Alias;
	array[1] = e->MatchEntry->RealName;
	array[2] = e->MatchEntry->Address;

	sprintf(buf, "\033b%." STR(SIZE_ADDRESS) "s", e->MatchString);
	array[e->MatchField] = buf;

	return 0;
}
MakeStaticHook(DisplayHook, DisplayFunc);

HOOKPROTONH(CompareFunc, LONG, struct CustomABEntry *e2, struct CustomABEntry *e1)
{
	if(e1->MatchField == e2->MatchField)
			return Stricmp(e1->MatchString, e2->MatchString);
	else	return e1->MatchField < e2->MatchField ? -1 : +1;
}
MakeStaticHook(CompareHook, CompareFunc);

OVERLOAD(OM_NEW)
{
	Object *list;
	if(obj = DoSuperNew(cl, obj,
		MUIA_Window_Activate,         FALSE,
		MUIA_Window_Borderless,       TRUE,
		MUIA_Window_CloseGadget,      FALSE,
		MUIA_Window_DepthGadget,      FALSE,
		MUIA_Window_DragBar,          FALSE,
		MUIA_Window_SizeGadget,       FALSE,
		WindowContents, GroupObject,
			InnerSpacing(0, 0),
			Child, list = ListObject,
				InputListFrame,
				MUIA_List_CompareHook,     &CompareHook,
				MUIA_List_ConstructHook,   &ConstructHook,
				MUIA_List_CursorType,      MUIV_List_CursorType_Bar,
				MUIA_List_DestructHook,    &GeneralDesHook,
				MUIA_List_DisplayHook,     &DisplayHook,
				MUIA_List_Format,          ",,",
			End,
		End,
		TAG_MORE, inittags(msg)))
	{
		GETDATA;

		struct TagItem *tags = inittags(msg), *tag;
		while((tag = NextTagItem(&tags)))
		{
			switch(tag->ti_Tag)
			{
				ATTR(String) : data->String = (Object *)tag->ti_Data ; break;
			}
		}

		data->Matchlist = list;

		if(!data->String)
		{
			DB(kprintf("No MUIA_Addrmatchlist_String supplied\n");)
			CoerceMethod(cl, obj, OM_DISPOSE);
			obj = NULL;
		}
	}
	return (ULONG)obj;
}

OVERLOAD(OM_SET)
{
	GETDATA;

	struct TagItem *tags = inittags(msg), *tag;
	while((tag = NextTagItem(&tags)))
	{
		switch(tag->ti_Tag)
		{
			case MUIA_Window_Open:
			{
				if(data->Open != tag->ti_Data && _win(data->String))
					set(_win(data->String), MUIA_Window_DisableKeys, (data->Open = tag->ti_Data) ? 1 << MUIKEY_WINDOW_CLOSE : 0);
			}
			break;
		}
	}
	return DoSuperMethodA(cl, obj, msg);
}

DECLARE(ChangeWindow)
{
	GETDATA;
	ULONG left = xget(_win(data->String), MUIA_Window_LeftEdge) + _left(data->String);
	ULONG top = xget(_win(data->String), MUIA_Window_TopEdge) + _bottom(data->String) + 1;

	if(xget(obj, MUIA_Window_Open))
	{
		struct Window *win = (struct Window *)xget(obj, MUIA_Window_Window);
		ChangeWindowBox(win, left, top, _width(data->String), win->Height);
	}
	else
	{
		SetAttrs(obj,
			MUIA_Window_LeftEdge,   left,
			MUIA_Window_TopEdge,    top,
			MUIA_Window_Width,      _width(data->String),
			TAG_DONE);
	}

	return 0;
}

DECLARE(Event) // struct IntuiMessage *imsg
{
	GETDATA;
	STRPTR res = NULL;
	if(xget(obj, MUIA_Window_Open))
	{
		struct CustomABEntry *entry;
		if(xget(data->Matchlist, MUIA_List_Active) != MUIV_List_Active_Off)
				set(data->Matchlist, MUIA_List_Active, msg->imsg->Code == IECODE_UP ? MUIV_List_Active_Up     : MUIV_List_Active_Down);
		else	set(data->Matchlist, MUIA_List_Active, msg->imsg->Code == IECODE_UP ? MUIV_List_Active_Bottom : MUIV_List_Active_Top);

		DoMethod(data->Matchlist, MUIM_List_GetEntry, xget(data->Matchlist, MUIA_List_Active), &entry);
		res = entry->MatchString;
	}
	return (ULONG)res;
}

VOID FindAllMatches (STRPTR text, Object *list, struct MUI_NListtree_TreeNode *root)
{
	int tl = strlen(text);
	struct MUI_NListtree_TreeNode *tn;
	for(tn = (struct MUI_NListtree_TreeNode *)DoMethod(G->AB->GUI.LV_ADDRESSES, MUIM_NListtree_GetEntry, root, MUIV_NListtree_GetEntry_Position_Head, 0);tn ;tn = (struct MUI_NListtree_TreeNode *)DoMethod(G->AB->GUI.LV_ADDRESSES, MUIM_NListtree_GetEntry, tn, MUIV_NListtree_GetEntry_Position_Next, 0))
	{
		if(tn->tn_Flags & TNF_LIST) /* it's a sublist */
		{
			FindAllMatches(text, list, tn);
		}
		else
		{
			struct ABEntry *entry = (struct ABEntry *)tn->tn_User;
			struct CustomABEntry e = { -1 };

			if(!Strnicmp(entry->Alias, text, tl))				{ e.MatchField = 0; e.MatchString = entry->Alias; }
			else if(!Strnicmp(entry->RealName, text, tl))	{ e.MatchField = 1; e.MatchString = entry->RealName; }
			else if(!Strnicmp(entry->Address, text, tl))		{ e.MatchField = 2; e.MatchString = entry->Address; }

			if(e.MatchField != -1) /* one of the fields matches, so let's insert it in the MUI list */
			{
				e.MatchEntry = entry;
				DoMethod(list, MUIM_List_InsertSingle, &e, MUIV_List_Insert_Sorted);
			}
		}
	}
}

DECLARE(Open) // STRPTR str
{
	GETDATA;
	STRPTR res = NULL;
	LONG entries, state;
	struct CustomABEntry *entry;

	DB(kprintf("Match this: %s\n", msg->str);)

	set(data->Matchlist, MUIA_List_Quiet, TRUE);
	DoMethod(data->Matchlist, MUIM_List_Clear);
	if(msg->str && msg->str[0] != '\0')
		FindAllMatches(msg->str, data->Matchlist, MUIV_NListtree_GetEntry_ListNode_Root);
	set(data->Matchlist, MUIA_List_Quiet, FALSE);

	/* is there more entries in the list and if only one, is it longer than what the user already typed... */
	if((get(data->Matchlist, MUIA_List_Entries, &entries), entries > 0) && (DoMethod(data->Matchlist, MUIM_List_GetEntry, 0, &entry), (entries != 1 || Stricmp(msg->str, entry->MatchString))))
		res = entry->MatchString;

   /* should we open the popup list (if not already shown) */
	if(!res || (get(obj, MUIA_Window_Open, &state), !state))
		set(obj, MUIA_Window_Open, res ? TRUE : FALSE);

	return (ULONG)res;
}
