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
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 YAM Official Support Site :  http://www.yam.ch/
 YAM OpenSource project    :  http://sourceforge.net/projects/yamos/

 $Id$

***************************************************************************/

#ifndef CLASSES_CLASSES_H
#define CLASSES_CLASSES_H

 /* This file is automatically generated with GenClasses - PLEASE DO NOT EDIT!!! */

#include <clib/alib_protos.h>
#include <libraries/mui.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include "ClassesExtra.h"

#define inittags(msg)   (((struct opSet *)msg)->ops_AttrList)
#define GETDATA         struct Data *data = (struct Data *)INST_DATA(cl,obj)
#define NUMBEROFCLASSES 4

extern struct MUI_CustomClass *YAMClasses[NUMBEROFCLASSES];
Object *YAM_NewObject( STRPTR class, ULONG tag, ... );
long YAM_SetupClasses( void );
void YAM_CleanupClasses( void );

/******** Class: YAM ********/

#define MUIC_YAM "YAM_YAM"
#define YAMObject YAM_NewObject(MUIC_YAM
#define MUIA_YAM_EMailCacheName                       0xcccf4d01


ULONG YAMGetSize( void );
ULONG m_YAM_OM_NEW              (struct IClass *cl, Object *obj, Msg msg);
ULONG m_YAM_OM_DISPOSE          (struct IClass *cl, Object *obj, Msg msg);

/******** Class: Searchwindow ********/

#define MUIC_Searchwindow "YAM_Searchwindow"
#define SearchwindowObject YAM_NewObject(MUIC_Searchwindow
#define MUIM_Searchwindow_Open                        0xc94b4801
#define MUIM_Searchwindow_Close                       0xdeffbd01
#define MUIM_Searchwindow_Search                      0xa0f31201
#define MUIM_Searchwindow_Next                        0xaeff7101

struct MUIP_Searchwindow_Open
{
  ULONG methodID;
  Object *texteditor;
};

struct MUIP_Searchwindow_Close
{
  ULONG methodID;
};

struct MUIP_Searchwindow_Search
{
  ULONG methodID;
  ULONG top;
};

struct MUIP_Searchwindow_Next
{
  ULONG methodID;
};


ULONG SearchwindowGetSize( void );
ULONG m_Searchwindow_OM_NEW              (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Searchwindow_Open                (struct IClass *cl, Object *obj, struct MUIP_Searchwindow_Open *msg);
ULONG m_Searchwindow_Close               (struct IClass *cl, Object *obj, struct MUIP_Searchwindow_Close *msg);
ULONG m_Searchwindow_Search              (struct IClass *cl, Object *obj, struct MUIP_Searchwindow_Search *msg);
ULONG m_Searchwindow_Next                (struct IClass *cl, Object *obj, struct MUIP_Searchwindow_Next *msg);

/******** Class: Recipientstring ********/

#define MUIC_Recipientstring "YAM_Recipientstring"
#define RecipientstringObject YAM_NewObject(MUIC_Recipientstring
#define MUIM_Recipientstring_Resolve                  0x83ff7c01
#define MUIM_Recipientstring_AddRecipient             0xbeff8701
#define MUIM_Recipientstring_RecipientStart           0x9bdfdc01
#define MUIM_Recipientstring_CurrentRecipient         0x99df1501
#define MUIA_Recipientstring_ResolveOnCR              0xf6f7e401
#define MUIA_Recipientstring_MultipleRecipients       0x9b5f5501
#define MUIA_Recipientstring_FromString               0xf1f78601
#define MUIA_Recipientstring_ReplyToString            0xeb6b2a01
#define MUIA_Recipientstring_Popup                    0xd3d3d001

struct MUIP_Recipientstring_Resolve
{
  ULONG methodID;
  ULONG flags;
};

struct MUIP_Recipientstring_AddRecipient
{
  ULONG methodID;
  STRPTR address;
};

struct MUIP_Recipientstring_RecipientStart
{
  ULONG methodID;
};

struct MUIP_Recipientstring_CurrentRecipient
{
  ULONG methodID;
};


ULONG RecipientstringGetSize( void );
ULONG m_Recipientstring_OM_NEW              (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_OM_DISPOSE          (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_OM_GET              (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_OM_SET              (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_Setup          (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_Show           (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_GoActive       (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_GoInactive     (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_DragQuery      (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_DragDrop       (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_Popstring_Open (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_MUIM_HandleEvent    (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Recipientstring_Resolve             (struct IClass *cl, Object *obj, struct MUIP_Recipientstring_Resolve *msg);
ULONG m_Recipientstring_AddRecipient        (struct IClass *cl, Object *obj, struct MUIP_Recipientstring_AddRecipient *msg);
ULONG m_Recipientstring_RecipientStart      (struct IClass *cl, Object *obj, struct MUIP_Recipientstring_RecipientStart *msg);
ULONG m_Recipientstring_CurrentRecipient    (struct IClass *cl, Object *obj, struct MUIP_Recipientstring_CurrentRecipient *msg);

/* Exported text */

#define MUIF_Recipientstring_Resolve_NoFullName  (1 << 0) // do not resolve with fullname "Mister X <misterx@mister.com>"
#define MUIF_Recipientstring_Resolve_NoValid     (1 << 1) // do not resolve already valid string like "misterx@mister.com"

/******** Class: Addrmatchlist ********/

#define MUIC_Addrmatchlist "YAM_Addrmatchlist"
#define AddrmatchlistObject YAM_NewObject(MUIC_Addrmatchlist
#define MUIM_Addrmatchlist_ChangeWindow               0x82c38101
#define MUIM_Addrmatchlist_Event                      0xc3571401
#define MUIM_Addrmatchlist_Open                       0xd9fbf301
#define MUIA_Addrmatchlist_String                     0xc3676601

struct MUIP_Addrmatchlist_ChangeWindow
{
  ULONG methodID;
};

struct MUIP_Addrmatchlist_Event
{
  ULONG methodID;
  struct IntuiMessage *imsg;
};

struct MUIP_Addrmatchlist_Open
{
  ULONG methodID;
  STRPTR str;
};


ULONG AddrmatchlistGetSize( void );
ULONG m_Addrmatchlist_OM_NEW              (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Addrmatchlist_OM_SET              (struct IClass *cl, Object *obj, Msg msg);
ULONG m_Addrmatchlist_ChangeWindow        (struct IClass *cl, Object *obj, struct MUIP_Addrmatchlist_ChangeWindow *msg);
ULONG m_Addrmatchlist_Event               (struct IClass *cl, Object *obj, struct MUIP_Addrmatchlist_Event *msg);
ULONG m_Addrmatchlist_Open                (struct IClass *cl, Object *obj, struct MUIP_Addrmatchlist_Open *msg);


#endif /* CLASSES_CLASSES_H */

