/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 1995-2000 by Marcel Beck <mbeck@yam.ch>
 Copyright (C) 2000-2006 by YAM Open Source Team

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 YAM Official Support Site :  http://www.yam.ch
 YAM OpenSource project    :  http://sourceforge.net/projects/yamos/

 $Id$

 Superclass:  MUIC_Window
 Description: Window for reading emails

***************************************************************************/

#include "ReadWindow_cl.h"

#include "YAM_mime.h"
#include "BayesFilter.h"

#include "Debug.h"

/* EXPORT
#define MUIV_ReadWindow_ToolbarItems 17
*/

/* CLASSDATA
struct Data
{
  Object *MI_MESSAGE;
  Object *MI_EDIT;
  Object *MI_MOVE;
  Object *MI_DELETE;
  Object *MI_DETACH;
  Object *MI_CROP;
  Object *MI_SETMARKED;
  Object *MI_TOSPAM;
  Object *MI_TOHAM;
  Object *MI_CHSUBJ;
  Object *MI_NAVIG;
  Object *MI_WRAPH;
  Object *MI_TSTYLE;
  Object *MI_FFONT;
  Object *MI_EXTKEY;
  Object *MI_CHKSIG;
  Object *MI_SAVEDEC;
  Object *MI_REPLY;
  Object *MI_BOUNCE;
  Object *MI_NEXTTHREAD;
  Object *MI_PREVTHREAD;
  Object *windowToolbar;
  Object *statusIconGroup;
  Object *readMailGroup;
  struct MUIP_Toolbar_Description toolbarDesc[MUIV_ReadWindow_ToolbarItems];

  char  title[SIZE_SUBJECT+1];
  int   lastDirection;
  int   windowNumber;
};
*/

// menu item IDs
enum
{
  RMEN_EDIT=501,RMEN_MOVE,RMEN_COPY,RMEN_DELETE,RMEN_PRINT,RMEN_SAVE,RMEN_DISPLAY,RMEN_DETACH,
  RMEN_CROP,RMEN_NEW,RMEN_REPLY,RMEN_FORWARD,RMEN_BOUNCE,RMEN_SAVEADDR,RMEN_SETUNREAD,RMEN_SETMARKED,
  RMEN_SETSPAM,RMEN_SETHAM,RMEN_CHSUBJ,RMEN_PREV,RMEN_NEXT,RMEN_URPREV,RMEN_URNEXT,RMEN_PREVTH,
  RMEN_NEXTTH,RMEN_EXTKEY,RMEN_CHKSIG,RMEN_SAVEDEC,RMEN_HNONE,RMEN_HSHORT,RMEN_HFULL,RMEN_SNONE,
  RMEN_SDATA,RMEN_SFULL,RMEN_WRAPH,RMEN_TSTYLE,RMEN_FFONT,RMEN_SIMAGE
};

/* Private Functions */
/// SelectMessage()
//  Activates a message in the main window's message listview
INLINE LONG SelectMessage(struct Mail *mail)
{
  LONG pos = MUIV_NList_GetPos_Start;

  // make sure the folder of the mail is currently the
  // active one.
  MA_ChangeFolder(mail->Folder, TRUE);

  // get the position of the mail in the main mail listview
  DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetPos, mail, &pos);

  // if it is currently viewable we go and set it as the
  // active mail
  if(pos != MUIV_NList_GetPos_End)
    set(G->MA->GUI.PG_MAILLIST, MUIA_NList_Active, pos);

  // return the position to the caller
  return pos;
}
///
/// AddRemoveSpamMenu
//  Dynamically add/remove the spam menu entries to the menu according to the configuration
static void AddRemoveSpamMenu(struct Data *data, struct Mail *mail)
{
  ENTER();

  if(C->SpamFilterEnabled)
  {
    BOOL isSpamMail = mail && !isVirtualMail(mail) && hasStatusSpam(mail);
    BOOL isHamMail  = mail && !isVirtualMail(mail) && hasStatusHam(mail);

    // for each entry check if it exists and if it is part of the menu
    // if not, create a new entry and add it to the current layout
    if(data->MI_TOHAM == NULL || isChildOfFamily(data->MI_MESSAGE, data->MI_TOHAM) == FALSE)
    {
      if((data->MI_TOHAM =  MakeMenuitem(GetStr(MSG_RE_SETNOTSPAM), RMEN_SETHAM)) != NULL)
      {
        set(data->MI_TOHAM, MUIA_Menuitem_Enabled, !isHamMail);
        DoMethod(data->MI_MESSAGE, MUIM_Family_Insert, data->MI_TOHAM, data->MI_SETMARKED);
      }
    }

    if(data->MI_TOSPAM == NULL || isChildOfFamily(data->MI_MESSAGE, data->MI_TOSPAM) == FALSE)
    {
      if((data->MI_TOSPAM = MakeMenuitem(GetStr(MSG_RE_SETSPAM), RMEN_SETSPAM)) != NULL)
      {
        set(data->MI_TOHAM, MUIA_Menuitem_Enabled, !isSpamMail);
        DoMethod(data->MI_MESSAGE, MUIM_Family_Insert, data->MI_TOSPAM, data->MI_SETMARKED);
      }
    }
  }
  else
  {
    // for each entry check if it exists and if it is part of the menu
    // if yes, then remove the entry and dispose it
    if(data->MI_TOSPAM != NULL && isChildOfFamily(data->MI_MESSAGE, data->MI_TOSPAM))
    {
      DoMethod(data->MI_MESSAGE, MUIM_Family_Remove, data->MI_TOSPAM);
      MUI_DisposeObject(data->MI_TOSPAM);
      data->MI_TOSPAM = NULL;
    }
    if(data->MI_TOHAM != NULL && isChildOfFamily(data->MI_MESSAGE, data->MI_TOHAM))
    {
      DoMethod(data->MI_MESSAGE, MUIM_Family_Remove, data->MI_TOHAM);
      MUI_DisposeObject(data->MI_TOHAM);
      data->MI_TOHAM = NULL;
    }
  }

  LEAVE();
}
///

/* Hooks */
/// CloseReadWindowHook()
//  Hook that will be called as soon as a read window is closed
HOOKPROTONHNO(CloseReadWindowFunc, void, struct ReadMailData **arg)
{
  struct ReadMailData *rmData = *arg;

  ENTER();

  // only if this is not a close operation because the application
  // is getting iconified we really cleanup our readmail data
  if(rmData == G->ActiveRexxRMData ||
     xget(G->App, MUIA_Application_Iconified) == FALSE)
  {
    // check if this rmData is the current active Rexx background
    // processing one and if so set the ptr to NULL to signal the rexx
    // commands that their active window was closed
    if(rmData == G->ActiveRexxRMData)
      G->ActiveRexxRMData = NULL;

    // calls the CleanupReadMailData to clean everything else up
    CleanupReadMailData(rmData, TRUE);
  }

  LEAVE();
}
MakeStaticHook(CloseReadWindowHook, CloseReadWindowFunc);
///

/* Overloaded Methods */
/// OVERLOAD(OM_NEW)
OVERLOAD(OM_NEW)
{
  ULONG i;
  struct Data *data;
  struct Data *tmpData;

  // Our static Toolbar description field
  static const struct NewToolbarEntry tb_butt[MUIV_ReadWindow_ToolbarItems] =
  {
    { MSG_RE_TBPrev,    MSG_HELP_RE_BT_PREVIOUS },
    { MSG_RE_TBNext,    MSG_HELP_RE_BT_NEXT     },
    { MSG_RE_TBPrevTh,  MSG_HELP_RE_BT_QUESTION },
    { MSG_RE_TBNextTh,  MSG_HELP_RE_BT_ANSWER   },
    { MSG_Space,        NULL                    },
    { MSG_RE_TBDisplay, MSG_HELP_RE_BT_DISPLAY  },
    { MSG_RE_TBSave,    MSG_HELP_RE_BT_EXPORT   },
    { MSG_RE_TBPrint,   MSG_HELP_RE_BT_PRINT    },
    { MSG_Space,        NULL                    },
    { MSG_RE_TBDelete,  MSG_HELP_RE_BT_DELETE   },
    { MSG_RE_TBMove,    MSG_HELP_RE_BT_MOVE     },
    { MSG_RE_TBReply,   MSG_HELP_RE_BT_REPLY    },
    { MSG_RE_TBForward, MSG_HELP_RE_BT_FORWARD  },
    { MSG_Space,        NULL                    },
    { MSG_RE_TBSPAM,    MSG_HELP_RE_BT_SPAM     },
    { MSG_RE_TBNOTSPAM, MSG_HELP_RE_BT_NOTSPAM  },
    { NULL,             NULL                    }
  };

  // generate a temporarly struct Data to which we store our data and
  // copy it later on
  if(!(data = tmpData = calloc(1, sizeof(struct Data))))
    return 0;

  for(i=0; i < MUIV_ReadWindow_ToolbarItems; i++)
  {
    SetupToolbar(&(data->toolbarDesc[i]),
                 tb_butt[i].label ?
                    (tb_butt[i].label == MSG_Space ? "" : GetStr(tb_butt[i].label))
                    : NULL,
                 tb_butt[i].help ? GetStr(tb_butt[i].help) : NULL,
                 0);
  }

  // before we create all objects of this new read window we have to
  // check which number we can set for this window. Therefore we search in our
  // current ReadMailData list and check which number we can give this window
  i=0;
  do
  {
    struct MinNode *curNode = G->readMailDataList.mlh_Head;
    
    for(; curNode->mln_Succ; curNode = curNode->mln_Succ)
    {
      struct ReadMailData *rmData = (struct ReadMailData *)curNode;
      
      if(rmData->readWindow &&
         xget(rmData->readWindow, MUIA_ReadWindow_Num) == i)
      {
        break;
      }
    }

    // if the curNode successor is NULL we traversed through the whole
    // list without finding the proposed ID, so we can choose it as
    // our readWindow ID
    if(curNode->mln_Succ == NULL)
    {
      D(DBF_GUI, "Free window number %d found.", i);
      data->windowNumber = i;

      break;
    }

    i++;
  }
  while(1);

  if((obj = DoSuperNew(cl, obj,

    MUIA_Window_Title,   "",
    MUIA_HelpNode,       "RE_W",
    MUIA_Window_ID,     MAKE_ID('R','D','W',data->windowNumber),
    MUIA_Window_Menustrip, MenustripObject,
      MenuChild, data->MI_MESSAGE = MenuObject, MUIA_Menu_Title, GetStr(MSG_Message),
        MenuChild, data->MI_EDIT = Menuitem(GetStr(MSG_MA_MEdit), "E", TRUE, FALSE, RMEN_EDIT),
        MenuChild, data->MI_MOVE = Menuitem(GetStr(MSG_MA_MMove), "M", TRUE, FALSE, RMEN_MOVE),
        MenuChild, Menuitem(GetStr(MSG_MA_MCopy), "Y", TRUE, FALSE, RMEN_COPY),
        MenuChild, data->MI_DELETE = Menuitem(GetStr(MSG_MA_MDelete),  "Del", TRUE, TRUE,  RMEN_DELETE),
        MenuChild, MenuBarLabel,
        MenuChild, Menuitem(GetStr(MSG_Print),       "P",     TRUE, FALSE, RMEN_PRINT),
        MenuChild, Menuitem(GetStr(MSG_MA_Save),    "S",     TRUE, FALSE, RMEN_SAVE),
        MenuChild, MenuitemObject, MUIA_Menuitem_Title, GetStr(MSG_Attachments),
          MenuChild, Menuitem(GetStr(MSG_RE_MDisplay),"D",  TRUE,  FALSE, RMEN_DISPLAY),
          MenuChild, data->MI_DETACH = Menuitem(GetStr(MSG_RE_SaveAll),  "A",  TRUE, FALSE, RMEN_DETACH),
          MenuChild, data->MI_CROP =    Menuitem(GetStr(MSG_MA_Crop),    "O",  TRUE, FALSE, RMEN_CROP),
        End,
        MenuChild, MenuBarLabel,
        MenuChild, Menuitem(GetStr(MSG_New),         "N", TRUE, FALSE, RMEN_NEW),
        MenuChild, data->MI_REPLY = Menuitem(GetStr(MSG_MA_MReply),   "R", TRUE, FALSE, RMEN_REPLY),
        MenuChild, Menuitem(GetStr(MSG_MA_MForward), "W", TRUE, FALSE, RMEN_FORWARD),
        MenuChild, data->MI_BOUNCE = Menuitem(GetStr(MSG_MA_MBounce),   "B", TRUE, FALSE, RMEN_BOUNCE),
        MenuChild, MenuBarLabel,
        MenuChild, Menuitem(GetStr(MSG_MA_MGetAddress), "J", TRUE, FALSE, RMEN_SAVEADDR),
        MenuChild, Menuitem(GetStr(MSG_RE_SetUnread),   "U", TRUE, FALSE, RMEN_SETUNREAD),
        MenuChild, data->MI_SETMARKED = Menuitem(GetStr(MSG_RE_SETMARKED),    ",", TRUE, FALSE, RMEN_SETMARKED),
        MenuChild, data->MI_CHSUBJ = Menuitem(GetStr(MSG_MA_ChangeSubj), NULL, TRUE, FALSE, RMEN_CHSUBJ),
      End,
      MenuChild, data->MI_NAVIG = MenuObject, MUIA_Menu_Title, GetStr(MSG_RE_Navigation),
        MenuChild, Menuitem(GetStr(MSG_RE_MNext),    "right", TRUE, TRUE, RMEN_NEXT),
        MenuChild, Menuitem(GetStr(MSG_RE_MPrev),     "left",  TRUE, TRUE, RMEN_PREV),
        MenuChild, Menuitem(GetStr(MSG_RE_MURNext),  "shift right", TRUE, TRUE, RMEN_URNEXT),
        MenuChild, Menuitem(GetStr(MSG_RE_MURPrev),  "shift left",  TRUE, TRUE, RMEN_URPREV),
        MenuChild, data->MI_NEXTTHREAD = Menuitem(GetStr(MSG_RE_MNextTh), ">", TRUE, FALSE, RMEN_NEXTTH),
        MenuChild, data->MI_PREVTHREAD = Menuitem(GetStr(MSG_RE_MPrevTh), "<", TRUE, FALSE, RMEN_PREVTH),
      End,
      MenuChild, MenuObject, MUIA_Menu_Title, "PGP",
        MenuChild, data->MI_EXTKEY = Menuitem(GetStr(MSG_RE_ExtractKey), "X", TRUE, FALSE, RMEN_EXTKEY),
        MenuChild, data->MI_CHKSIG = Menuitem(GetStr(MSG_RE_SigCheck), "K", TRUE, FALSE, RMEN_CHKSIG),
        MenuChild, data->MI_SAVEDEC = Menuitem(GetStr(MSG_RE_SaveDecrypted), "V", TRUE, FALSE, RMEN_SAVEDEC),
      End,
      MenuChild, MenuObject, MUIA_Menu_Title, GetStr(MSG_MA_Settings),
        MenuChild, MenuitemCheck(GetStr(MSG_RE_NoHeaders),    "0", TRUE, C->ShowHeader==HM_NOHEADER,    FALSE, 0x06, RMEN_HNONE),
        MenuChild, MenuitemCheck(GetStr(MSG_RE_ShortHeaders), "1", TRUE, C->ShowHeader==HM_SHORTHEADER, FALSE, 0x05, RMEN_HSHORT),
        MenuChild, MenuitemCheck(GetStr(MSG_RE_FullHeaders),  "2", TRUE, C->ShowHeader==HM_FULLHEADER,  FALSE, 0x03, RMEN_HFULL),
        MenuChild, MenuBarLabel,
        MenuChild, MenuitemCheck(GetStr(MSG_RE_NoSInfo),      "3", TRUE, C->ShowSenderInfo==SIM_OFF,    FALSE, 0xE0, RMEN_SNONE),
        MenuChild, MenuitemCheck(GetStr(MSG_RE_SInfo),        "4", TRUE, C->ShowSenderInfo==SIM_DATA,   FALSE, 0xD0, RMEN_SDATA),
        MenuChild, MenuitemCheck(GetStr(MSG_RE_SInfoImage),   "5", TRUE, C->ShowSenderInfo==SIM_ALL,    FALSE, 0x90, RMEN_SFULL),
        MenuChild, MenuitemCheck(GetStr(MSG_RE_SImageOnly),   "6", TRUE, C->ShowSenderInfo==SIM_IMAGE,  FALSE, 0x70, RMEN_SIMAGE),
        MenuChild, MenuBarLabel,
        MenuChild, data->MI_WRAPH  = MenuitemCheck(GetStr(MSG_RE_WrapHeader), "H", TRUE, C->WrapHeader,    TRUE, 0, RMEN_WRAPH),
        MenuChild, data->MI_TSTYLE = MenuitemCheck(GetStr(MSG_RE_Textstyles), "T", TRUE, C->UseTextstyles, TRUE, 0, RMEN_TSTYLE),
        MenuChild, data->MI_FFONT  = MenuitemCheck(GetStr(MSG_RE_FixedFont),  "F", TRUE, C->FixedFontEdit, TRUE, 0, RMEN_FFONT),
      End,
    End,
    WindowContents, VGroup,
      Child, hasHideToolBarFlag(C->HideGUIElements) ?
        (RectangleObject, MUIA_ShowMe, FALSE, End) :
        (HGroup, GroupSpacing(2),
          Child, HGroupV,
            Child, data->windowToolbar = ToolbarObject,
              MUIA_HelpNode, "RE_B",
              MUIA_Toolbar_ImageType,        MUIV_Toolbar_ImageType_File,
              MUIA_Toolbar_ImageNormal,      "PROGDIR:Icons/Read.toolbar",
              MUIA_Toolbar_ImageGhost,       "PROGDIR:Icons/Read_G.toolbar",
              MUIA_Toolbar_ImageSelect,      "PROGDIR:Icons/Read_S.toolbar",
              MUIA_Toolbar_Description,      &data->toolbarDesc[0],
              MUIA_Toolbar_Reusable,        TRUE,
              MUIA_Toolbar_ParseUnderscore,  TRUE,
              MUIA_Font,                     MUIV_Font_Tiny,
              MUIA_ShortHelp,               TRUE,
            End,
            Child, HSpace(0),
          End,
          Child, RectangleObject,
            MUIA_Rectangle_VBar, TRUE,
            MUIA_FixWidth,        3,
          End,
          Child, data->statusIconGroup = StatusIconGroupObject,
          End,
        End),
        Child, VGroup,
          Child, data->readMailGroup = ReadMailGroupObject,
            MUIA_ReadMailGroup_HGVertWeight, G->Weights[10],
            MUIA_ReadMailGroup_TGVertWeight, G->Weights[11],
          End,
        End,
      End,

    TAG_MORE, (ULONG)inittags(msg))))
  {
    struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);

    if(rmData == NULL ||
       (data = (struct Data *)INST_DATA(cl,obj)) == NULL)
    {
      return 0;
    }

    // copy back the data stored in our temporarly struct Data
    memcpy(data, tmpData, sizeof(struct Data));

    // place this newly created window to the readMailData structure aswell
    rmData->readWindow = obj;

    // set the MUIA_UserData attribute to our readMailData struct
    // as we might need it later on
    set(obj, MUIA_UserData, rmData);

    // Add the window to the application object
    DoMethod(G->App, OM_ADDMEMBER, obj);

    // set some Notifies and stuff
    set(obj, MUIA_Window_DefaultObject, data->readMailGroup);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_EDIT,      obj, 3, MUIM_ReadWindow_NewMail, NEW_EDIT, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_MOVE,      obj, 1, MUIM_ReadWindow_MoveMailRequest);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_COPY,      obj, 1, MUIM_ReadWindow_CopyMailRequest);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_DELETE,    obj, 2, MUIM_ReadWindow_DeleteMailRequest, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_PRINT,     data->readMailGroup, 1, MUIM_ReadMailGroup_PrintMailRequest);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SAVE,      data->readMailGroup, 1, MUIM_ReadMailGroup_SaveMailRequest);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_DISPLAY,   data->readMailGroup, 1, MUIM_ReadMailGroup_DisplayMailRequest);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_DETACH,    data->readMailGroup, 1, MUIM_ReadMailGroup_SaveAllAttachments);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_CROP,      data->readMailGroup, 1, MUIM_ReadMailGroup_CropAttachmentsRequest);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_NEW,       obj, 3,  MUIM_ReadWindow_NewMail, NEW_NEW, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_REPLY,     obj, 3, MUIM_ReadWindow_NewMail, NEW_REPLY, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_FORWARD,   obj, 3, MUIM_ReadWindow_NewMail, NEW_FORWARD, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_BOUNCE,    obj, 3, MUIM_ReadWindow_NewMail, NEW_BOUNCE, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SAVEADDR,  obj, 1, MUIM_ReadWindow_GrabSenderAddress);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SETUNREAD, obj, 1, MUIM_ReadWindow_SetMailToUnread);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SETMARKED, obj, 1, MUIM_ReadWindow_SetMailToMarked);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SETSPAM,   obj, 2, MUIM_ReadWindow_ClassifyMessage, BC_SPAM);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SETHAM,    obj, 2, MUIM_ReadWindow_ClassifyMessage, BC_HAM);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_CHSUBJ,    obj, 1, MUIM_ReadWindow_ChangeSubjectRequest);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_PREV,      obj, 3, MUIM_ReadWindow_SwitchMail, -1, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_NEXT,      obj, 3, MUIM_ReadWindow_SwitchMail, +1, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_URPREV,    obj, 3, MUIM_ReadWindow_SwitchMail, -1, IEQUALIFIER_LSHIFT);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_URNEXT,    obj, 3, MUIM_ReadWindow_SwitchMail, +1, IEQUALIFIER_LSHIFT);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_PREVTH,    obj, 2, MUIM_ReadWindow_FollowThread, -1);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_NEXTTH,    obj, 2, MUIM_ReadWindow_FollowThread, +1);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_EXTKEY,    data->readMailGroup, 1, MUIM_ReadMailGroup_ExtractPGPKey);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_CHKSIG,    data->readMailGroup, 2, MUIM_ReadMailGroup_CheckPGPSignature, TRUE);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SAVEDEC,   data->readMailGroup, 1, MUIM_ReadMailGroup_SaveDecryptedMail);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_HNONE,     obj, 2, MUIM_ReadWindow_ChangeHeaderMode, HM_NOHEADER);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_HSHORT,    obj, 2, MUIM_ReadWindow_ChangeHeaderMode, HM_SHORTHEADER);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_HFULL,     obj, 2, MUIM_ReadWindow_ChangeHeaderMode, HM_FULLHEADER);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SNONE,     obj, 2, MUIM_ReadWindow_ChangeSenderInfoMode, SIM_OFF);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SDATA,     obj, 2, MUIM_ReadWindow_ChangeSenderInfoMode, SIM_DATA);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SFULL,     obj, 2, MUIM_ReadWindow_ChangeSenderInfoMode, SIM_ALL);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_SIMAGE,    obj, 2, MUIM_ReadWindow_ChangeSenderInfoMode, SIM_IMAGE);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_WRAPH,     obj, 1, MUIM_ReadWindow_StyleOptionsChanged);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_TSTYLE,    obj, 1, MUIM_ReadWindow_StyleOptionsChanged);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, RMEN_FFONT,     obj, 1, MUIM_ReadWindow_StyleOptionsChanged);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 0, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 3, MUIM_ReadWindow_SwitchMail, -1, MUIV_Toolbar_Qualifier);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 1, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 3, MUIM_ReadWindow_SwitchMail, +1, MUIV_Toolbar_Qualifier);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 2, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 2, MUIM_ReadWindow_FollowThread, -1);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 3, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 2, MUIM_ReadWindow_FollowThread, +1);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 5, MUIV_Toolbar_Notify_Pressed, FALSE, data->readMailGroup, 1, MUIM_ReadMailGroup_DisplayMailRequest);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 6, MUIV_Toolbar_Notify_Pressed, FALSE, data->readMailGroup, 1, MUIM_ReadMailGroup_SaveMailRequest);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 7, MUIV_Toolbar_Notify_Pressed, FALSE, data->readMailGroup, 1, MUIM_ReadMailGroup_PrintMailRequest);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify, 9, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 2, MUIM_ReadWindow_DeleteMailRequest, MUIV_Toolbar_Qualifier);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify,10, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 1, MUIM_ReadWindow_MoveMailRequest);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify,11, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 3, MUIM_ReadWindow_NewMail, NEW_REPLY, MUIV_Toolbar_Qualifier);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify,12, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 3, MUIM_ReadWindow_NewMail, NEW_FORWARD, MUIV_Toolbar_Qualifier);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify,14, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 2, MUIM_ReadWindow_ClassifyMessage, BC_SPAM);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Notify,15, MUIV_Toolbar_Notify_Pressed, FALSE, obj, 2, MUIM_ReadWindow_ClassifyMessage, BC_HAM);
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-capslock del",                 obj, 2, MUIM_ReadWindow_DeleteMailRequest, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-capslock shift del",            obj, 2, MUIM_ReadWindow_DeleteMailRequest, IEQUALIFIER_LSHIFT);
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-repeat -capslock space",       data->readMailGroup, 2, MUIM_TextEditor_ARexxCmd, "Next Page");
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-repeat -capslock backspace",   data->readMailGroup, 2, MUIM_TextEditor_ARexxCmd, "Previous Page");
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-repeat -capslock left",         obj, 3, MUIM_ReadWindow_SwitchMail, -1, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-repeat -capslock right",       obj, 3, MUIM_ReadWindow_SwitchMail, +1, 0);
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-repeat -capslock shift left",   obj, 3, MUIM_ReadWindow_SwitchMail, -1, IEQUALIFIER_LSHIFT);
    DoMethod(obj, MUIM_Notify, MUIA_Window_InputEvent, "-repeat -capslock shift right",  obj, 3, MUIM_ReadWindow_SwitchMail, +1, IEQUALIFIER_LSHIFT);

    AddRemoveSpamMenu(data, NULL);

    // before we continue we make sure we connect a notify to the new window
    // so that we get informed if the window is closed and therefore can be
    // disposed
    // However, please note that because we do kill the window upon closing it
    // we have to use MUIM_Application_PushMethod instead of calling the CloseReadWindowHook
    // directly
    DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
                  MUIV_Notify_Application, 6,
                    MUIM_Application_PushMethod, G->App, 3, MUIM_CallHook, &CloseReadWindowHook, rmData);
  }

  // free the temporary mem we allocated before
  free(tmpData);

  return (ULONG)obj;
}

///
/// OVERLOAD(OM_GET)
OVERLOAD(OM_GET)
{
  GETDATA;
  ULONG *store = ((struct opGet *)msg)->opg_Storage;

  switch(((struct opGet *)msg)->opg_AttrID)
  {
    ATTR(ReadMailData) : *store = (ULONG)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData); return TRUE;
    ATTR(Num)           : *store = data->windowNumber; return TRUE;
  }

  return DoSuperMethodA(cl, obj, msg);
}
///
/// OVERLOAD(OM_SET)
OVERLOAD(OM_SET)
{
  struct TagItem *tags = inittags(msg), *tag;
  
  while((tag = NextTagItem(&tags)))
  {
    switch(tag->ti_Tag)
    {
      // we also catch foreign attributes
      case MUIA_Window_CloseRequest:
      {
        // if the window is supposed to be closed and the StatusChangeDelay is
        // active and no embeddedReadPane is active we have to cancel an eventually
        // existing timerequest to set the status of a mail to read.
        if(tag->ti_Data == TRUE &&
           C->StatusChangeDelayOn == TRUE && C->EmbeddedReadPane == FALSE &&
           xget(obj, MUIA_Window_Open) == TRUE)
        {
          TC_Stop(TIO_READSTATUSUPDATE);
        }
      }
      break;
    }
  }

  return DoSuperMethodA(cl, obj, msg);
}
///
/// OVERLOAD(MUIM_Window_Snapshot)
OVERLOAD(MUIM_Window_Snapshot)
{
  GETDATA;

  // on a snapshot request we save the weights of all our objects here.
  G->Weights[10] = xget(data->readMailGroup, MUIA_ReadMailGroup_HGVertWeight);
  G->Weights[11] = xget(data->readMailGroup, MUIA_ReadMailGroup_TGVertWeight);

  // make sure the layout is saved
  SaveLayout(TRUE);

  return DoSuperMethodA(cl, obj, msg);
}

///

/* Public Methods */
/// DECLARE(ReadMail)
DECLARE(ReadMail) // struct Mail *mail
{
  GETDATA;
  struct Mail *mail = msg->mail;
  struct Folder *folder = mail->Folder;
  BOOL isRealMail   = !isVirtualMail(mail);
  BOOL isSentMail   = isRealMail && isSentMailFolder(folder);
  BOOL isSpamMail   = !isRealMail || !C->SpamFilterEnabled || hasStatusSpam(mail);
  BOOL isHamMail    = !isRealMail || !C->SpamFilterEnabled || hasStatusHam(mail);
  BOOL inSpamFolder = isRealMail && isSpamFolder(folder);
  BOOL result = FALSE;
  BOOL initialCall = data->title[0] == '\0'; // TRUE if this is the first call
  BOOL prevMailAvailable = FALSE;
  BOOL nextMailAvailable = FALSE;

  ENTER();

  D(DBF_GUI, "setting up readWindow for reading a mail");

  // check the status of the next/prev thread nagivation
  if(isRealMail)
  {
    if(AllFolderLoaded())
    {
      prevMailAvailable = RE_GetThread(mail, FALSE, FALSE, obj) != NULL;
      nextMailAvailable = RE_GetThread(mail, TRUE, FALSE, obj) != NULL;
    }
    else
    {
      prevMailAvailable = TRUE;
      nextMailAvailable = TRUE;
    }
  }

  // enable/disable some menuitems in advance
  set(data->MI_EDIT,      MUIA_Menuitem_Enabled, isSentMail && !inSpamFolder);
  set(data->MI_MOVE,      MUIA_Menuitem_Enabled, isRealMail);
  set(data->MI_DELETE,    MUIA_Menuitem_Enabled, isRealMail);
  set(data->MI_CROP,      MUIA_Menuitem_Enabled, isRealMail);
  set(data->MI_CHSUBJ,    MUIA_Menuitem_Enabled, isRealMail && !inSpamFolder);
  set(data->MI_NAVIG,     MUIA_Menuitem_Enabled, isRealMail);
  set(data->MI_REPLY,     MUIA_Menuitem_Enabled, !isSentMail && !inSpamFolder);
  set(data->MI_BOUNCE,    MUIA_Menuitem_Enabled, !isSentMail);
  set(data->MI_NEXTTHREAD,MUIA_Menuitem_Enabled, nextMailAvailable);
  set(data->MI_PREVTHREAD,MUIA_Menuitem_Enabled, prevMailAvailable);

  if(C->SpamFilterEnabled)
  {
    if(data->MI_TOSPAM != NULL)
      set(data->MI_TOSPAM,    MUIA_Menuitem_Enabled, isRealMail && !isSpamMail);
    if(data->MI_TOHAM != NULL)
      set(data->MI_TOHAM,     MUIA_Menuitem_Enabled, isRealMail && !isHamMail);
  }

  if(data->windowToolbar)
  {
    LONG pos = MUIV_NList_GetPos_Start;

    // query the position of the mail in the current listview
    DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetPos, mail, &pos);

    // now set some items of the toolbar ghosted/enabled
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set,  0, MUIV_Toolbar_Set_Ghosted, isRealMail ? pos == 0 : TRUE);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set,  1, MUIV_Toolbar_Set_Ghosted, isRealMail ? pos == (folder->Total-1) : TRUE);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set,  2, MUIV_Toolbar_Set_Ghosted, !prevMailAvailable);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set,  3, MUIV_Toolbar_Set_Ghosted, !nextMailAvailable);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set,  9, MUIV_Toolbar_Set_Ghosted, !isRealMail);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 10, MUIV_Toolbar_Set_Ghosted, !isRealMail);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 11, MUIV_Toolbar_Set_Ghosted, isSentMail || inSpamFolder);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 12, MUIV_Toolbar_Set_Ghosted, inSpamFolder);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 14, MUIV_Toolbar_Set_Ghosted, isSpamMail);
    DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 15, MUIV_Toolbar_Set_Ghosted, isHamMail);
  }

  // Update the status groups
  DoMethod(data->statusIconGroup, MUIM_StatusIconGroup_Update, mail);

  // now we read in the mail in our read mail group
  if(DoMethod(data->readMailGroup, MUIM_ReadMailGroup_ReadMail, mail,
                                   initialCall == FALSE ? MUIF_ReadMailGroup_ReadMail_StatusChangeDelay : MUIF_NONE))
  {
    size_t titleLen;
    struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);

    // if the title of the window is empty, we can assume that no previous mail was
    // displayed in this readwindow, so we can set the mailTextObject of the readmailgroup
    // as the active object so that the user can browse through the mailtext immediatley after
    // opening the window
    if(initialCall)
      DoMethod(data->readMailGroup, MUIM_ReadMailGroup_ActivateMailText);

    // set the title of the readWindow now
    if(C->MultipleWindows == TRUE ||
       rmData == G->ActiveRexxRMData)
    {
      titleLen = snprintf(data->title, sizeof(data->title), "[%d] %s %s: ", data->windowNumber,
                                                            isSentMail ? GetStr(MSG_To) : GetStr(MSG_From),
                                                            isSentMail ? AddrName(mail->To) : AddrName(mail->From));
    }
    else
    {
      titleLen = snprintf(data->title, sizeof(data->title), "%s %s: ",
                                                            isSentMail ? GetStr(MSG_To) : GetStr(MSG_From),
                                                            isSentMail ? AddrName(mail->To) : AddrName(mail->From));
    }

    if(strlen(mail->Subject)+titleLen > sizeof(data->title)-1)
    {
      if(titleLen < sizeof(data->title)-4)
      {
        strlcat(data->title, mail->Subject, sizeof(data->title)-titleLen-4);
        strlcat(data->title, "...", sizeof(data->title)); // signals that the string was cut.
      }
      else
        strlcat(&data->title[sizeof(data->title)-5], "...", 4);
    }
    else
      strlcat(data->title, mail->Subject, sizeof(data->title));

    set(obj, MUIA_Window_Title, data->title);

    // enable some Menuitems depending on the read mail
    set(data->MI_EXTKEY, MUIA_Menuitem_Enabled, rmData->hasPGPKey);
    set(data->MI_CHKSIG, MUIA_Menuitem_Enabled, hasPGPSOldFlag(rmData) ||
                                                hasPGPSMimeFlag(rmData));
    set(data->MI_SAVEDEC, MUIA_Menuitem_Enabled, isRealMail &&
                                                 (hasPGPEMimeFlag(rmData) || hasPGPEOldFlag(rmData)));

    // everything worked fine
    result = TRUE;
  }

  RETURN(result);
  return result;
}

///
/// DECLARE(NewMail)
DECLARE(NewMail) // enum NewMode mode, ULONG qualifier
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;
  enum NewMode mode = msg->mode;
  int flags = 0;

  ENTER();

  // check for qualifier keys
  if(mode == NEW_FORWARD)
  {
    if(hasFlag(msg->qualifier, (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)))
      mode = NEW_BOUNCE;
    else if(isFlagSet(msg->qualifier, IEQUALIFIER_CONTROL))
      SET_FLAG(flags, NEWF_FWD_NOATTACH);
  }
  else if(mode == NEW_REPLY)
  {
    if(hasFlag(msg->qualifier, (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)))
      SET_FLAG(flags, NEWF_REP_PRIVATE);
  
    if(hasFlag(msg->qualifier, (IEQUALIFIER_LALT|IEQUALIFIER_RALT)))
      SET_FLAG(flags, NEWF_REP_MLIST);
  
    if(isFlagSet(msg->qualifier, IEQUALIFIER_CONTROL))
      SET_FLAG(flags, NEWF_REP_NOQUOTE);
   
  }

  // then create a new mail depending on the current mode
  if(MailExists(mail, NULL))
  {
    // create some fake mail list
    struct Mail *mlist[3];
    mlist[0] = (struct Mail*)1;
    mlist[1] = NULL;
    mlist[2] = mail;

    switch(mode)
    {
      case NEW_NEW:     MA_NewNew(mail, flags);     break;
      case NEW_EDIT:    MA_NewEdit(mail, flags);    break;
      case NEW_BOUNCE:  MA_NewBounce(mail, flags);  break;
      case NEW_FORWARD: MA_NewForward(mlist, flags);break;
      case NEW_REPLY:   MA_NewReply(mlist, flags);  break;

      default:
       // nothing
      break;
    }
  }

  RETURN(0);
  return 0;
}

///
/// DECLARE(MoveMailRequest)
DECLARE(MoveMailRequest)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;
  struct Folder *srcfolder = mail->Folder;
  BOOL closeAfter = FALSE;

  if(MailExists(mail, srcfolder))
  {
    struct Folder *dstfolder = FolderRequest(GetStr(MSG_MA_MoveMsg),
                                             GetStr(MSG_MA_MoveMsgReq),
                                             GetStr(MSG_MA_MoveGad),
                                             GetStr(MSG_Cancel), srcfolder, obj);

    if(dstfolder)
    {
      int pos = SelectMessage(mail); // select the message in the folder and return position
      int entries;

      // depending on the last move direction we
      // set it back
      if(data->lastDirection == -1)
      {
        if(pos-1 >= 0)
          set(G->MA->GUI.PG_MAILLIST, MUIA_NList_Active, --pos);
        else
          closeAfter = TRUE;
      }

      // move the mail to the selected destination folder
      MA_MoveCopy(mail, srcfolder, dstfolder, FALSE, FALSE);

      // erase the old pointer as this has been free()ed by MA_MoveCopy()
      rmData->mail = NULL;

      // if there are still mails in the current folder we make sure
      // it is displayed in this window now or close it
      if(closeAfter == FALSE &&
         (entries = xget(G->MA->GUI.PG_MAILLIST, MUIA_NList_Entries)) >= pos+1)
      {
        DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetEntry, pos, &mail);
        if(mail)
          DoMethod(obj, MUIM_ReadWindow_ReadMail, mail);
        else
          closeAfter = TRUE;
      }
      else
        closeAfter = TRUE;

      // make sure the read window is closed in case there is no further
      // mail for deletion in this direction
      if(closeAfter)
        DoMethod(G->App, MUIM_Application_PushMethod, G->App, 3, MUIM_CallHook, &CloseReadWindowHook, rmData);

      AppendLogNormal(22, GetStr(MSG_LOG_Moving), 1, srcfolder->Name, dstfolder->Name);
    }
  }

  return 0;
}

///
/// DECLARE(CopyMailRequest)
DECLARE(CopyMailRequest)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;
  struct Folder *srcfolder = mail->Folder;

  if(MailExists(mail, srcfolder))
  {
    struct Folder *dstfolder = FolderRequest(GetStr(MSG_MA_CopyMsg),
                                             GetStr(MSG_MA_MoveMsgReq),
                                             GetStr(MSG_MA_CopyGad),
                                             GetStr(MSG_Cancel), NULL, obj);
    if(dstfolder)
    {
      // if there is no source folder this is a virtual mail that we
      // export to the destination folder
      if(srcfolder)
      {
        MA_MoveCopy(mail, srcfolder, dstfolder, TRUE, FALSE);
        
        AppendLogNormal(24, GetStr(MSG_LOG_Copying), 1, srcfolder->Name, dstfolder->Name);
      }
      else if(RE_Export(rmData, rmData->readFile,
                MA_NewMailFile(dstfolder, mail->MailFile), "", 0, FALSE, FALSE, IntMimeTypeArray[MT_ME_EMAIL].ContentType))
      {
        struct Mail *newmail = AddMailToList(mail, dstfolder);

        if(dstfolder == FO_GetCurrentFolder())
          DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_InsertSingle, newmail, MUIV_NList_Insert_Sorted);

        setStatusToRead(newmail); // OLD status
      }
    }
  }

  return 0;
}

///
/// DECLARE(DeleteMailRequest)
DECLARE(DeleteMailRequest) // ULONG qualifier
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;
  struct Folder *folder = mail->Folder;
  struct Folder *delfolder = FO_GetFolderByType(FT_TRASH, NULL);
  BOOL delatonce = hasFlag(msg->qualifier, (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT));
  BOOL closeAfter = FALSE;

  if(MailExists(mail, folder))
  {
    int pos = SelectMessage(mail); // select the message in the folder and return position
    int entries;

    // depending on the last move direction we
    // set it back
    if(data->lastDirection == -1)
    {
      if(pos-1 >= 0)
        set(G->MA->GUI.PG_MAILLIST, MUIA_NList_Active, --pos);
      else
        closeAfter = TRUE;
    }

    // delete the mail
    MA_DeleteSingle(mail, delatonce, FALSE, FALSE);

    // erase the old pointer as this has been free()ed by MA_DeleteSingle()
    rmData->mail = NULL;

    // if there are still mails in the current folder we make sure
    // it is displayed in this window now or close it
    if(closeAfter == FALSE &&
       (entries = xget(G->MA->GUI.PG_MAILLIST, MUIA_NList_Entries)) >= pos+1)
    {
      DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetEntry, pos, &mail);
      if(mail)
        DoMethod(obj, MUIM_ReadWindow_ReadMail, mail);
      else
        closeAfter = TRUE;
    }
    else
      closeAfter = TRUE;

    // make sure the read window is closed in case there is no further
    // mail for deletion in this direction
    if(closeAfter)
      DoMethod(G->App, MUIM_Application_PushMethod, G->App, 3, MUIM_CallHook, &CloseReadWindowHook, rmData);

    if(delatonce || isSpamFolder(folder))
      AppendLogNormal(20, GetStr(MSG_LOG_Deleting), 1, folder->Name);
    else
      AppendLogNormal(22, GetStr(MSG_LOG_Moving), 1, folder->Name, delfolder->Name);
  }

  return 0;
}

///
/// DECLARE(ClassifyMessage)
DECLARE(ClassifyMessage) // enum BayesClassification class
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;
  struct Folder *folder = mail->Folder;
  struct Folder *spamfolder = FO_GetFolderByType(FT_SPAM, NULL);
  enum BayesClassification class = msg->class;
  BOOL closeAfter = FALSE;

  if(MailExists(mail, folder) && spamfolder != NULL)
  {
    if(!hasStatusSpam(mail) && class == BC_SPAM)
    {
      int pos = SelectMessage(mail); // select the message in the folder and return position
      int entries;

      // depending on the last move direction we
      // set it back
      if(data->lastDirection == -1)
      {
        if(pos-1 >= 0)
          set(G->MA->GUI.PG_MAILLIST, MUIA_NList_Active, --pos);
        else
          closeAfter = TRUE;
      }

      // mark the mail as user spam
      AppendLogVerbose(90, GetStr(MSG_LOG_MAILISSPAM), AddrName(mail->From), mail->Subject);
      BayesFilterSetClassification(mail, BC_SPAM);
      setStatusToUserSpam(mail);

      // move the mail
      MA_MoveCopy(mail, folder, spamfolder, FALSE, FALSE);

      if(folder == spamfolder)
      {
        // update the toolbar
        DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 14, MUIV_Toolbar_Set_Ghosted, TRUE);
        DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 15, MUIV_Toolbar_Set_Ghosted, FALSE);
      }

      // erase the old pointer as this has been free()ed by MA_MoveCopy()
      rmData->mail = NULL;

      // if there are still mails in the current folder we make sure
      // it is displayed in this window now or close it
      if(closeAfter == FALSE &&
         (entries = xget(G->MA->GUI.PG_MAILLIST, MUIA_NList_Entries)) >= pos+1)
      {
        DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetEntry, pos, &mail);
        if(mail)
          DoMethod(obj, MUIM_ReadWindow_ReadMail, mail);
        else
          closeAfter = TRUE;
      }
      else
        closeAfter = TRUE;
    }
    else if(!hasStatusHam(mail) && class == BC_HAM)
    {
      // mark the mail as ham
      AppendLogVerbose(90, GetStr(MSG_LOG_MAILISNOTSPAM), AddrName(mail->From), mail->Subject);
      BayesFilterSetClassification(mail, BC_HAM);
      setStatusToHam(mail);

      // update the toolbar
      DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 14, MUIV_Toolbar_Set_Ghosted, FALSE);
      DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 15, MUIV_Toolbar_Set_Ghosted, TRUE);
    }
  }

  // make sure the read window is closed in case there is no further
  // mail for deletion in this direction
  if(closeAfter)
    DoMethod(G->App, MUIM_Application_PushMethod, G->App, 3, MUIM_CallHook, &CloseReadWindowHook, rmData);

  return 0;
}

///
/// DECLARE(GrabSenderAddress)
//  Stores sender address of current message in the address book
DECLARE(GrabSenderAddress)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;

  if(MailExists(mail, mail->Folder))
  {
    struct Mail *mlist[3];
    mlist[0] = (struct Mail *)1;
    mlist[1] = NULL;
    mlist[2] = mail;
    MA_GetAddress(mlist);
  }

  return 0;
}

///
/// DECLARE(SetMailToUnread)
//  Sets the status of the current mail to unread
DECLARE(SetMailToUnread)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;

  setStatusToUnread(mail);

  DoMethod(data->statusIconGroup, MUIM_StatusIconGroup_Update, mail);
  DisplayStatistics(NULL, TRUE);

  return 0;
}

///
/// DECLARE(SetMailToMarked)
//  Sets the flags of the current mail to marked
DECLARE(SetMailToMarked)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;

  setStatusToMarked(mail);

  DoMethod(data->statusIconGroup, MUIM_StatusIconGroup_Update, mail);
  DisplayStatistics(NULL, TRUE);

  return 0;
}

///
/// DECLARE(ChangeSubjectRequest)
//  Changes the subject of the current message
DECLARE(ChangeSubjectRequest)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;
  struct Folder *folder = mail->Folder;

  if(MailExists(mail, folder))
  {
    char subj[SIZE_SUBJECT];

    strlcpy(subj, mail->Subject, sizeof(subj));
    
    if(StringRequest(subj, SIZE_SUBJECT,
                     GetStr(MSG_MA_ChangeSubj),
                     GetStr(MSG_MA_ChangeSubjReq),
                     GetStr(MSG_Okay), NULL, GetStr(MSG_Cancel), FALSE, obj))
    {
      MA_ChangeSubject(mail, subj);

      if(DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_MainMailListGroup_RedrawMail, mail))
      {
        MA_ChangeSelected(TRUE);
        DisplayStatistics(mail->Folder, TRUE);
      }
      
      // update this window
      DoMethod(obj, MUIM_ReadWindow_ReadMail, mail);
    }
  }

  return 0;
}

///
/// DECLARE(SwitchMail)
//  Goes to next or previous (new) message in list
DECLARE(SwitchMail) // LONG direction, ULONG qualifier
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;
  struct Folder *folder = mail->Folder;
  LONG direction = msg->direction;
  LONG act = MUIV_NList_GetPos_Start;
  BOOL onlynew = hasFlag(msg->qualifier, (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT));
  BOOL found = FALSE;

  // save the direction we are going to process now
  data->lastDirection = direction;

  // we have to make sure that the folder the next/prev mail will
  // be showed from is active, that`s why we call ChangeFolder with TRUE.
  MA_ChangeFolder(folder, TRUE);

  // after changing the folder we have to get the MailInfo (Position etc.)
  DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetPos, mail, &act);

  D(DBF_GUI, "act: %d - direction: %d", act, direction);

  if(act != MUIV_NList_GetPos_End)
  {
    for(act += direction; act >= 0; act += direction)
    {
      DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetEntry, act, &mail);
      if(!mail)
        break;

      if(!onlynew ||
        (hasStatusNew(mail) || !hasStatusRead(mail)))
      {
         set(G->MA->GUI.PG_MAILLIST, MUIA_NList_Active, act);
         DoMethod(obj, MUIM_ReadWindow_ReadMail, mail);

         // this is a valid break and not break because of an error
         found = TRUE;
         break;
      }
    }
  }

  // check if there are following/previous folders with unread
  // mails and change to there if the user wants
  if(!found && onlynew)
  {
    if(C->AskJumpUnread)
    {
      struct Folder **flist;

      if((flist = FO_CreateList()))
      {
        int i;

        // look for the current folder in the array
        for(i = 1; i <= (int)*flist; i++)
        {
          if(flist[i] == folder)
            break;
        }

        // look for first folder with at least one unread mail
        // and if found read that mail
        for(i += direction; i <= (int)*flist && i >= 1; i += direction)
        {
          if(!isGroupFolder(flist[i]) && flist[i]->Unread > 0)
          {
            if(!MUI_Request(G->App, obj, 0, GetStr(MSG_MA_ConfirmReq),
                                            GetStr(MSG_YesNoReq),
                                            GetStr(MSG_RE_MoveNextFolderReq), flist[i]->Name))
            {
              break;
            }

            MA_ChangeFolder(flist[i], TRUE);
            DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &mail);
            if(!mail)
              break;
            
            DoMethod(obj, MUIM_ReadWindow_ReadMail, mail);

            // this is a valid break and not break because of an error
            found = TRUE;
            break;
          }
        }

        // beep if no folder with unread mails was found
        if(i > (int)*flist || i < 1)
          DisplayBeep(_screen(obj));

        free(flist);
      }
    }
    else
      DisplayBeep(_screen(obj));
  }

  // if we didn't find any next/previous mail (mail == NULL) then
  // we can close the window accordingly. This signals a user that he/she
  // reached the end of the mail list
  if(found == FALSE)
    DoMethod(G->App, MUIM_Application_PushMethod, G->App, 3, MUIM_CallHook, &CloseReadWindowHook, rmData);

  return 0;
}

///
/// DECLARE(FollowThread)
//  Follows a thread in either direction
DECLARE(FollowThread) // LONG direction
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  struct Mail *mail = rmData->mail;

  // depending on the direction we get the Question or Answer to the current Message
  struct Mail *fmail = RE_GetThread(mail, msg->direction <= 0 ? FALSE : TRUE, TRUE, obj);

  if(fmail)
  {
    LONG pos = MUIV_NList_GetPos_Start;

    // we have to make sure that the folder where the message will be showed
    // from is active and ready to display the mail
    MA_ChangeFolder(fmail->Folder, TRUE);

    // get the position of the mail in the currently active listview
    DoMethod(G->MA->GUI.PG_MAILLIST, MUIM_NList_GetPos, fmail, &pos);

    // if the mail is displayed we make it the active one
    if(pos != MUIV_NList_GetPos_End)
      set(G->MA->GUI.PG_MAILLIST, MUIA_NList_Active, pos);
    
    DoMethod(obj, MUIM_ReadWindow_ReadMail, fmail);
  }
  else
  {
    // set the correct toolbar image and menuitem ghosted
    if(msg->direction <= 0)
    {
      DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 2, MUIV_Toolbar_Set_Ghosted, TRUE);
      set(data->MI_PREVTHREAD, MUIA_Menuitem_Enabled, FALSE);
    }
    else
    {
      DoMethod(data->windowToolbar, MUIM_Toolbar_Set, 3, MUIV_Toolbar_Set_Ghosted, TRUE);
      set(data->MI_NEXTTHREAD, MUIA_Menuitem_Enabled, FALSE);
    }

    DisplayBeep(_screen(obj));
  }

  return 0;
}

///
/// DECLARE(ChangeHeaderMode)
//  Changes display options (header)
DECLARE(ChangeHeaderMode) // enum HeaderMode hmode
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);

  // change the header display mode
  rmData->headerMode = msg->hmode;

  // issue an update of the readMailGroup
  DoMethod(data->readMailGroup, MUIM_ReadMailGroup_UpdateHeaderDisplay, MUIF_ReadMailGroup_ReadMail_UpdateOnly);

  return 0;
}

///
/// DECLARE(ChangeSenderInfoMode)
//  Changes display options (sender info)
DECLARE(ChangeSenderInfoMode) // enum SInfoMode simode
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);

  // change the sender info mode
  rmData->senderInfoMode = msg->simode;

  // issue an update of the readMailGroup
  DoMethod(data->readMailGroup, MUIM_ReadMailGroup_UpdateHeaderDisplay, MUIF_ReadMailGroup_ReadMail_UpdateOnly);

  return 0;
}

///
/// DECLARE(StyleOptionsChanged)
DECLARE(StyleOptionsChanged)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);
  BOOL updateHeader = FALSE;
  BOOL updateText = FALSE;
  BOOL tmp;

  ENTER();

  // check wrapHeaders diff
  if((tmp = xget(data->MI_WRAPH, MUIA_Menuitem_Checked)) != rmData->wrapHeaders)
  {
    rmData->wrapHeaders = tmp;
    updateHeader = TRUE;
  }

  // check useTextstyles diff
  if((tmp = xget(data->MI_TSTYLE, MUIA_Menuitem_Checked)) != rmData->useTextstyles)
  {
    rmData->useTextstyles = tmp;
    updateText = TRUE;
  }

  // check useFixedFont diff
  if((tmp = xget(data->MI_FFONT, MUIA_Menuitem_Checked)) != rmData->useFixedFont)
  {
    rmData->useFixedFont = tmp;
    updateText = TRUE;
  }

  // issue an update of the readMailGroup's components
  if(updateHeader && updateText)
  {
    DoMethod(data->readMailGroup, MUIM_ReadMailGroup_ReadMail, rmData->mail,
                                  MUIF_ReadMailGroup_ReadMail_UpdateOnly);
  }
  else if(updateText)
  {
    DoMethod(data->readMailGroup, MUIM_ReadMailGroup_ReadMail, rmData->mail,
                                  (MUIF_ReadMailGroup_ReadMail_UpdateOnly |
                                   MUIF_ReadMailGroup_ReadMail_UpdateTextOnly));
  }
  else if(updateHeader)
  {
    DoMethod(data->readMailGroup, MUIM_ReadMailGroup_UpdateHeaderDisplay,
                                  MUIF_ReadMailGroup_ReadMail_UpdateOnly);
  }

  RETURN(0);
  return 0;
}
///
/// DECLARE(StatusIconRefresh)
DECLARE(StatusIconRefresh)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);

  // Update the statusIconGroup
  if(rmData->mail)
    DoMethod(data->statusIconGroup, MUIM_StatusIconGroup_Update, rmData->mail);

  return 0;
}
///
/// DECLARE(AddRemoveSpamMenu)
DECLARE(AddRemoveSpamMenu)
{
  GETDATA;
  struct ReadMailData *rmData = (struct ReadMailData *)xget(data->readMailGroup, MUIA_ReadMailGroup_ReadMailData);

  AddRemoveSpamMenu(data, rmData->mail);

  return 0;
}
///
