/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 2000  Marcel Beck <mbeck@yam.ch>

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

***************************************************************************/

#include "YAM.h"

/***************************************************************************
 Utilities
***************************************************************************/

/* local protos */
LOCAL int GetWord(char **rptr, char *wbuf, int max);
LOCAL char *ReflowParagraph(char *start, char *end, int lmax, char *dest);
LOCAL char *RemoveQuoteString(char *start, char *end, char *quot, char *dest);
LOCAL char *InsertQuoteString(char *start, char *quote, FILE *out);
LOCAL void SaveParagraph(char *start, char *end, char *prefix, FILE *out);
LOCAL char *FileToBuffer(char *file);
LOCAL int TZtoMinutes(char *tzone);
LOCAL BOOL GetPackMethod(int xpktype, char **method, int *eff);
LOCAL BOOL CompressMailFile(char *src, char *dst, char *passwd, char *method, int eff);
LOCAL BOOL UncompressMailFile(char *src, char *dst, char *passwd);
LOCAL void AppendToLogfile(int id, char *text, void *a1, void *a2, void *a3, void *a4);
LOCAL char *IdentifyFileDT(char *fname);
SAVEDS ASM void putChar(REG(a0, struct Hook *hook), REG(a1, char c), REG(a2, struct Locale *locale));


/*** Requesters ***/
/// StringRequest
//  Puts up a string requester
int StringRequest(char *string, int size, char *title, char *body, char *yestext, char *alttext, char *notext, BOOL secret, APTR parent)
{
   APTR bt_okay, bt_middle, bt_cancel, wi_sr, st_in;
   int ret_code = -1;

   wi_sr = WindowObject,
      MUIA_Window_Title, title ? title : "YAM",
      MUIA_Window_RefWindow, parent,
      MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
      MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
      MUIA_Window_ID, MAKE_ID('S','R','E','Q'),
      WindowContents, VGroup,
         Child, VGroup,
            GroupFrame,
            MUIA_Background, MUII_GroupBack,
            Child, LLabel(body),
            Child, st_in = secret ? MakePassString("") : MakeString(size, ""),
         End,
         Child, ColGroup(3),
            Child, bt_okay = MakeButton(yestext),
            Child, bt_middle = alttext ? MakeButton(alttext) : HSpace(0),
            Child, bt_cancel = MakeButton(notext),
         End,
      End,
   End;
   setstring(st_in, string);
   set(wi_sr, MUIA_Window_ActiveObject, st_in);
   set(G->App, MUIA_Application_Sleep, TRUE);
   DoMethod(G->App, OM_ADDMEMBER, wi_sr);
   DoMethod(bt_okay  , MUIM_Notify, MUIA_Pressed, FALSE, G->App, 2, MUIM_Application_ReturnID, 1);
   DoMethod(bt_middle, MUIM_Notify, MUIA_Pressed, FALSE, G->App, 2, MUIM_Application_ReturnID, 2);
   DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE, G->App, 2, MUIM_Application_ReturnID, 3);
   DoMethod(st_in, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, G->App, 2, MUIM_Application_ReturnID, 1);
   DoMethod(wi_sr, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, G->App, 2, MUIM_Application_ReturnID, 3);
   if (!SafeOpenWindow(wi_sr)) ret_code = 0;
   else while (ret_code == -1)
   {
      ULONG signals;
      switch (DoMethod(G->App, MUIM_Application_Input, &signals))
      {
         case 1: ret_code = 1; break;
         case 2: ret_code = 2; break;
         case 3: ret_code = 0; break;
      }
      if (ret_code == -1 && signals) Wait(signals);
   }
   if (ret_code > 0) GetMUIString(string, st_in);
   DoMethod(G->App, OM_REMMEMBER, wi_sr);
   set(G->App, MUIA_Application_Sleep, FALSE);
   return ret_code;
}
///
/// FolderRequest
//  Allows user to choose a folder from a list
struct Folder *FolderRequest(char *title, char *body, char *yestext, char *notext, struct Folder *exclude, APTR parent)
{
   static int lastactive;
   struct Folder **flist, *folder = (struct Folder *)-1;
   int act, i;
   char *fname;
   APTR bt_okay, bt_cancel, wi_fr, lv_folder;

   wi_fr = WindowObject,
      MUIA_Window_Title, title ? title : "YAM",
      MUIA_Window_RefWindow, parent,
      MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
      MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
      MUIA_Window_ID, MAKE_ID('F','R','E','Q'),
      WindowContents, VGroup,
         Child, LLabel(body),
         Child, lv_folder = ListviewObject,
            MUIA_CycleChain, 1,
            MUIA_Listview_DoubleClick, TRUE,
            MUIA_Listview_List, ListObject,
               InputListFrame,
               MUIA_List_AutoVisible, TRUE,
            End,
         End,
         Child, ColGroup(3),
            Child, bt_okay = MakeButton(yestext),
            Child, HSpace(0),
            Child, bt_cancel = MakeButton(notext),
         End,
      End,
   End;
   if (wi_fr)
   {
      flist = FO_CreateList();
      for (i = 1; i <= (int)*flist; i++) if (flist[i] != exclude) if (flist[i]->Type != FT_SEPARATOR)
         DoMethod(lv_folder, MUIM_List_InsertSingle, flist[i]->Name, MUIV_List_Insert_Bottom);
      free(flist);
      set(lv_folder, MUIA_List_Active, lastactive);
      set(wi_fr, MUIA_Window_ActiveObject, lv_folder);
      set(G->App, MUIA_Application_Sleep, TRUE);
      DoMethod(G->App, OM_ADDMEMBER, wi_fr);
      DoMethod(bt_okay  , MUIM_Notify, MUIA_Pressed, FALSE, G->App, 2, MUIM_Application_ReturnID, 1);
      DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE, G->App, 2, MUIM_Application_ReturnID, 3);
      DoMethod(lv_folder, MUIM_Notify, MUIA_Listview_DoubleClick, MUIV_EveryTime, G->App, 2, MUIM_Application_ReturnID, 1);
      DoMethod(wi_fr, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, G->App, 2, MUIM_Application_ReturnID, 3);
      if (!SafeOpenWindow(wi_fr)) folder = NULL;
      else while (folder == (struct Folder *)-1)
      {
         ULONG signals, oo = DoMethod(G->App, MUIM_Application_Input, &signals);
         switch (oo)
         {
            case 1:
               get(lv_folder, MUIA_List_Active, &act);
               DoMethod(lv_folder, MUIM_List_GetEntry, act, &fname);
               if (folder = FO_GetFolderByName(fname, NULL)) lastactive = act;
               break;
            case 3: folder = NULL; break;
         }
         if (folder == (struct Folder *)-1 && signals) Wait(signals);
      }
      DoMethod(G->App, OM_REMMEMBER, wi_fr);
      set(G->App, MUIA_Application_Sleep, FALSE);
   }
   return folder;
}
///
/// AttachRequest
//  Allows user to select a message part (attachment) from a list
struct Part *AttachRequest(char *title, char *body, char *yestext, char *notext, int winnum, int mode, APTR parent)
{
   struct Part *retpart = (struct Part *)-1, *part, *prevpart;
   APTR bt_okay, bt_cancel, wi_ar, lv_attach;

   wi_ar = WindowObject,
      MUIA_Window_Title, title ? title : "YAM",
      MUIA_Window_RefWindow, parent,
      MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
      MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
      MUIA_Window_ID, MAKE_ID('A','R','E','Q'),
      WindowContents, VGroup,
         Child, LLabel(body),
         Child, lv_attach = NListviewObject,
            MUIA_CycleChain, 1,
            MUIA_NListview_NList, NListObject,
               InputListFrame,
               MUIA_NList_DoubleClick, TRUE,
               MUIA_NList_MultiSelect, (mode&ATTREQ_MULTI) ? MUIV_NList_MultiSelect_Default : MUIV_NList_MultiSelect_None,
               MUIA_NList_DisplayHook, &RE_LV_AttachDspFuncHook,
               MUIA_NList_Format, "BAR,BAR,",
            End,
         End,
         Child, ColGroup(3),
            Child, bt_okay = MakeButton(yestext),
            Child, HSpace(0),
            Child, bt_cancel = MakeButton(notext),
         End,
      End,
   End;
   if (wi_ar)
   {
      static struct Part spart[2];
      spart[0].Nr = -2; strcpy(spart[0].Name, GetStr(MSG_RE_Original)); spart[0].Size = G->RE[winnum]->Mail.Size; spart[0].Decoded = TRUE;
      spart[1].Nr = -1; strcpy(spart[1].Name, GetStr(MSG_RE_AllTexts)); spart[1].Size = 0;
      DoMethod(lv_attach, MUIM_NList_InsertSingle, &spart[0], MUIV_NList_Insert_Bottom);
      if ((mode&0xF) != ATTREQ_DISP) DoMethod(lv_attach, MUIM_NList_InsertSingle, &spart[1], MUIV_NList_Insert_Bottom);
      for (part = G->RE[winnum]->FirstPart->Next; part; part = part->Next)
         if ((mode&0xF) != ATTREQ_PRINT || part->Printable) DoMethod(lv_attach, MUIM_NList_InsertSingle, part, MUIV_NList_Insert_Bottom);
//      set(lv_attach, MUIA_NList_Active, 1);
      set(wi_ar, MUIA_Window_ActiveObject, lv_attach);
      set(G->App, MUIA_Application_Sleep, TRUE);
      DoMethod(G->App, OM_ADDMEMBER, wi_ar);
      DoMethod(bt_okay  , MUIM_Notify, MUIA_Pressed, FALSE, G->App, 2, MUIM_Application_ReturnID, 1);
      DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE, G->App, 2, MUIM_Application_ReturnID, 3);
      DoMethod(lv_attach, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime, G->App, 2, MUIM_Application_ReturnID, 1);
      DoMethod(wi_ar, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, G->App, 2, MUIM_Application_ReturnID, 3);
      if (!SafeOpenWindow(wi_ar)) retpart = NULL;
      else  while (retpart == (struct Part *)-1)
      {
         ULONG signals;
         int id;
         switch (DoMethod(G->App, MUIM_Application_Input, &signals))
         {
            case 1:
               for (id = MUIV_NList_NextSelected_Start;; prevpart = part)
               {
                  DoMethod(lv_attach, MUIM_NList_NextSelected, &id);
                  if (id == MUIV_NList_NextSelected_End) break;
                  DoMethod(lv_attach, MUIM_NList_GetEntry, id, &part);
                  if (retpart == (struct Part *)-1) retpart = part;
                  else prevpart->NextSelected = part;
               }
               break;
            case 3: retpart = NULL; break;
         }
         if (retpart == (struct Part *)-1 && signals) Wait(signals);
      }
      DoMethod(G->App, OM_REMMEMBER, wi_ar);
      set(G->App, MUIA_Application_Sleep, FALSE);
   }
   return retpart;
}
///
/// InfoWindow
//  Displays a text in an own window
void InfoWindow(char *title, char *body, char *oktext, APTR parent)
{
   APTR bt_okay, wi_iw;

   if (wi_iw = WindowObject,
         MUIA_Window_Title, title,
         MUIA_Window_RefWindow, parent,
         MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
         MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
         WindowContents, VGroup,
            MUIA_Background, MUII_RequesterBack,
            Child, VGroup,
               TextFrame,
               MUIA_Background, MUII_TextBack,
               Child, LLabel(body),
            End,
            Child, HCenter(bt_okay = MakeButton(oktext)),
         End,
      End)
   {
      DoMethod(G->App, OM_ADDMEMBER, wi_iw);
      DoMethod(bt_okay, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 5, MUIM_Application_PushMethod, parent, 2, MUIM_MainWindow_CloseWindow, wi_iw);
      DoMethod(wi_iw  , MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Application, 5, MUIM_Application_PushMethod, parent, 2, MUIM_MainWindow_CloseWindow, wi_iw);
      set(wi_iw, MUIA_Window_DefaultObject, bt_okay);
      set(wi_iw, MUIA_Window_Open, TRUE);
   }
}
///

/*** String related ***/
/// itoa
//  Converts an integer into a string
char *itoa(int val)
{
   static char str[SIZE_SMALL];
   sprintf(str, "%ld", val);
   return str;
}
///
/// MatchNoCase
//  Case insensitive pattern matching
int MatchNoCase(char *string, char *match)
{
   char pattern[SIZE_PATTERN];
   ParsePatternNoCase(match, pattern, SIZE_PATTERN);
   return MatchPatternNoCase(pattern, string);
}
///
/// CompNoCase
//  Case insensitive string comparison
BOOL CompNoCase(char *a, char *b, int l)
{
   for (; l; l--, a++, b++) if (toupper(*a) != toupper(*b)) return FALSE;
   return TRUE;
}
///
/// MatchTT
//  Checks if charset matches a translation table
BOOL MatchTT(char *charset, struct TranslationTable *tt, BOOL in)
{
   if (!tt) return FALSE;
   return (BOOL)MatchNoCase(charset, in ? tt->SourceCharset : tt->DestCharset);
}
////
/// ISpace
//  Checks if character is a white-space
BOOL ISpace(char ch)
{
   return (BOOL)(ch == ' ' || (ch >= 9 && ch <= 13));
}
///
/// isSpace
//  Localized version if isspace()
BOOL isSpace(int c)
{
   UBYTE ch = (UBYTE)c;
   return (BOOL)(G->Locale ? (IsSpace(G->Locale,(ULONG)ch) != 0) : (isspace(ch) != 0));
}
///
/// isGraph
//  Localized version of isgraph()
BOOL isGraph(int c)
{
   UBYTE ch = (UBYTE)c;
   return (BOOL)(G->Locale ? (IsGraph(G->Locale,(ULONG)ch) != 0) : (isgraph(ch) != 0));
}
///
/// isAlNum
//  Localized version of isalnum()
BOOL isAlNum(int c)
{
   UBYTE ch = (UBYTE)c;
   return (BOOL)(G->Locale ? (IsAlNum(G->Locale,(ULONG)ch) != 0) : (isalnum(ch) != 0));
}
///
/// StripUnderscore
//  Removes underscore from button labels
char *StripUnderscore(char *label)
{
   if (strchr(label,'_'))
   {
      static char newlabel[SIZE_DEFAULT];
      char *p = newlabel;
      for (; *label; label++) if (*label != '_') *p++ = *label;
      *p = 0;
      return newlabel;
   }
   else return label;
}
///
/// GetNextLine
//  Reads next line from a multi-line string
char *GetNextLine(char *p1)
{
   static char *begin;
   char *p2;
   if (p1) begin = p1;
   if (p1 = strchr(p2 = begin, '\n')) { *p1 = 0; begin = ++p1; }
   return p2;
}
///
/// TrimStart
//  Strips leading spaces
char *TrimStart(char *s)
{
   while (*s && ISpace(*s)) ++s;
   return s;
}       
///
/// TrimEnd
//  Removes trailing spaces
char *TrimEnd(char *s)
{
   char *e = s+strlen(s)-1;
   while (e >= s && ISpace(*e)) *e-- = 0;
   return s;
}
///
/// Trim
//  Removes leading and trailing spaces
char *Trim(char *s)
{
   char *e = s+strlen(s)-1;
   while (*s && ISpace(*s)) ++s;
   while (e >= s && ISpace(*e)) *e-- = 0;
   return s;
}       
///
/// stccat
//  Safe string concatenation
char *stccat(char *a, char *b, int n)
{
   int m = 1;
   char *p = a;
   while (*p) { p++; m++; }
   while (*b && m < n) { *p++ = *b++; m++; }
   *p = 0;
   return a;
}
///
/// stristr
//  Case insensitive version of strstr()
char *stristr(char *a, char *b)
{
   int l = strlen(b);
   for (; *a; a++) if (CompNoCase(a, b, l)) return a;
   return NULL;
}
///
/// MyStrChr
//  Searches for a character in string, ignoring text in quotes
char *MyStrChr(char *s, int c)
{
   BOOL nested = FALSE;
   for (; *s; s++)
      if (*s == '\"') nested = !nested; 
      else if (*s == (char)c && !nested) return s;
   return NULL;
}
///
/// Index
//  Returns position of a character in a string
int Index(char *str, char chr)
{
   char *t = strchr(str, chr);
   return t ? (int)(t-str) : -1;
}
///
/// AllocStrBuf
//  Allocates a dynamic buffer
char *AllocStrBuf(long initlen)
{
   char *strbuf = AllocMem(initlen+4, MEMF_PUBLIC|MEMF_CLEAR);
   if (strbuf)
   {
      *((long *)strbuf) = initlen;
      return &strbuf[4];
   }
   return NULL;
}
///
/// FreeStrBuf
//  Frees a dynamic buffer
void FreeStrBuf(char *strbuf)
{
   long len;

   if (!strbuf) return;
   len = *((long *)(strbuf-4));
   FreeMem(strbuf-4, len+4);        
}
///
/// StrBufCpy
//  Fills a dynamic buffer
char *StrBufCpy(char *strbuf, char *source)
{
   long oldlen, newlen;
   char *newstrbuf;

   if (!strbuf) strbuf = AllocStrBuf(strlen(source)+1);
   oldlen = *((long *)(strbuf-sizeof(long)));
   newstrbuf = strbuf;
   for (newlen = oldlen; newlen <= strlen(source); newlen += SIZE_DEFAULT);
   if (newlen != oldlen)
   {
      newstrbuf = AllocStrBuf(newlen);
      FreeStrBuf(strbuf);
   }
   strcpy(newstrbuf, source);
   return newstrbuf;
}
///
/// StrBufCat
//  String concatenation using a dynamic buffer
char *StrBufCat(char *strbuf, char *source)
{
   long oldlen, newlen;
   char *newstrbuf;

   if (!strbuf) strbuf = AllocStrBuf(strlen(source)+1);
   oldlen = *((long *)(strbuf-sizeof(long)));
   newstrbuf = strbuf;
   for (newlen = oldlen; newlen <= strlen(strbuf)+strlen(source); newlen += SIZE_DEFAULT);
   if (newlen != oldlen)
   {
      newstrbuf = AllocStrBuf(newlen);
      strcpy(newstrbuf, strbuf);
      FreeStrBuf(strbuf);
   }
   strcat(newstrbuf, source);
   return newstrbuf;
}
///
/// FreeData2D
//  Frees dynamic two-dimensional array
void FreeData2D(struct Data2D *data)
{
   while (data->Used) FreeStrBuf(data->Data[--data->Used]);
   if (data->Allocated) free(data->Data);
   data->Data = NULL; data->Allocated = 0;
}
///
/// AllocData2D
//  Allocates dynamic two-dimensional array
char *AllocData2D(struct Data2D *data, int initsize)
{
   if (data->Used >= data->Allocated)
   {
      data->Allocated += 10;
      if (data->Data) data->Data = realloc(data->Data, data->Allocated*sizeof(char *));
      else            data->Data = malloc(data->Allocated*sizeof(char *));
   }
   return data->Data[data->Used++] = AllocStrBuf(initsize);
}
///
/// AllocCopy
//  Duplicates a memory block
APTR AllocCopy(APTR source, int size)
{
   APTR dest;
   if (dest = malloc(size)) memcpy(dest, source, size);
   return dest;
}
///
/// Decrypt
//  Decrypts passwords
char *Decrypt(char *source)
{
   static char buffer[SIZE_PASSWORD+2];
   char *write = &buffer[SIZE_PASSWORD];

   *write-- = 0;
   while (*source)
   {
      *write-- = ((char)atoi(source))^CRYPTBYTE;
      source += 4;
   }
   return ++write;
}
///
/// Encrypt
//  Encrypts passwords
char *Encrypt(char *source)
{
   static char buffer[4*SIZE_PASSWORD+2];
   char *read = source+strlen(source)-1;

   *buffer = 0;
   while (read >= source)
   {
      unsigned char c = (*read--)^CRYPTBYTE;
      sprintf(&buffer[strlen(buffer)], "%03d ", c);
   }
   return buffer;
}
///

/*** File related ***/
/// GetLine
//  Gets Null terminated line of a text file
char *GetLine(FILE *fh, char *buffer, int bufsize)
{
   char *ptr;
   clear(buffer, bufsize);
   if (!fgets(buffer, bufsize, fh)) return NULL;
   if (ptr = strpbrk(buffer, "\r\n")) *ptr = 0;
   return buffer;
}       
///
/// FileInfo
//  Gets size, protection bits and type of a file/directory
BOOL FileInfo(char *filename, int *size, long *bits, long *type)
{
   struct FileInfoBlock *fib;
   BPTR lock;

   if (lock = Lock((STRPTR)filename,ACCESS_READ))
   {
      fib = AllocDosObject(DOS_FIB, NULL);
      Examine(lock, fib);
      if (size) *size = fib->fib_Size;
      if (bits) *bits = fib->fib_Protection;
      if (type) *type = fib->fib_DirEntryType;
      FreeDosObject(DOS_FIB, fib);
      UnLock(lock);
      return TRUE;
   }
   return FALSE;
}
///
/// FileSize
//  Returns size of a file
int FileSize(char *filename)
{
   int size;
   if (FileInfo(filename, &size, NULL, NULL)) return size; else return -1;
}
///
/// FileProtection
//  Returns protection bits of a file
long FileProtection(char *filename)
{
   long bits;
   if (FileInfo(filename, NULL, &bits, NULL)) return bits; else return -1;
}
///
/// FileType
//  Returns file type (file/directory)
int FileType(char *filename)
{
   long type;
   if (FileInfo(filename, NULL, NULL, &type)) return (type < 0 ? 1 : 2); else return 0;
}
///
/// FileExists
//  Checks if a file exists
BOOL FileExists(char *filename)
{
   return FileInfo(filename, NULL, NULL, NULL);
}
///
/// RenameFile
//  Renames a file and restores the protection bits
BOOL RenameFile(char *oldname, char *newname)
{
   struct FileInfoBlock *fib;
   BPTR lock;
   if (!Rename(oldname, newname)) return FALSE;
   if(fib = AllocDosObject(DOS_FIB,NULL))
	{
	   if (lock = Lock(newname, ACCESS_READ))
   	{
	      Examine(lock, fib); UnLock(lock);
   	   SetProtection(newname, fib->fib_Protection & (~FIBF_ARCHIVE));
	   }
	   FreeDosObject(DOS_FIB,fib);
	} else return FALSE;
   return TRUE;
}
///
/// CopyFile
//  Copies a file
BOOL CopyFile(char *dest, FILE *destfh, char *sour, FILE *sourfh)
{
   BOOL success = FALSE;

   if (sour) sourfh = fopen(sour, "r");
   if (dest) destfh = fopen(dest, "w");
   if (sourfh && destfh)
   {
      char buf[SIZE_LARGE];
      int len;
      success = TRUE;
      while (len = fread(buf, 1, SIZE_LARGE, sourfh))
         if (fwrite(buf, 1, len, destfh) != len) { success = FALSE; break; }
   }
   if (dest && destfh) fclose(destfh);
   if (sour && sourfh) fclose(sourfh);
   return success;
}
///
/// ConvertCRLF
//  Converts line breaks from LF to CRLF or vice versa
BOOL ConvertCRLF(char *in, char *out, BOOL to)
{
   BOOL success = FALSE;
   char buf[SIZE_LINE];
   FILE *infh, *outfh;

   if (infh = fopen(in, "r"))
   {
      if (outfh = fopen(out, "w"))
      {
         while (GetLine(infh, buf, SIZE_LINE)) fprintf(outfh, "%s%s\n", buf, to?"\r":"");
         success = TRUE;
         fclose(outfh);
      }
      fclose(infh);
   }
   return success;
}
///
/// GetWord
//  Word-wrapping algorithm: gets next word
LOCAL int GetWord(char **rptr, char *wbuf, int max)
{
   int c, i = 0;
   static int nonblanks = 0;

   if (!(c = *(*rptr)++)) { *wbuf = 0; return 0; }
   if (isSpace(c))
   {
      while (isSpace(c) && c != '\n')
      {
         if (i < max-1) wbuf[i++] = c;
         c = *(*rptr)++;
      }
      if (c == '\n' || !c) { i = 0; wbuf[i++] = '\n'; }
   }
   else
   {
      while (isGraph(c))
      {
         if (i < max-1) wbuf[i++] = c; else break;
         c = *(*rptr)++;
      }
      nonblanks += i;
      while (isSpace(c) && c != '\n') c = *(*rptr)++;
      if (c == '\n')
      {
         if (nonblanks <= 20) wbuf[i++] = '\n';
         nonblanks = 0;
      }
   }
   if (isGraph(c)) (*rptr)--;
   wbuf[i] = '\0'; return i;
}
///
/// ReflowParagraph
//  Word-wrapping algorithm: process a paragraph
LOCAL char *ReflowParagraph(char *start, char *end, int lmax, char *dest)
{
   int lword, lline = 0;
   char c, word[SIZE_LARGE], *p;
   BOOL dented = FALSE;

   while (lword = GetWord(&start, word, SIZE_LARGE))
   {
      if ((c = word[lword-1]) == '\n')  word[--lword] = '\0';
      if (!lword);
      else if (isSpace(*word))
      {
         if (lline) *dest++ = '\n';
         dented = TRUE; lline = lword;
         for (p = word; *p; p++) *dest++ = *p;
      }
      else
      {
         if (lline == 0 || dented) { for (p = word; *p; p++) *dest++ = *p; lline += lword; dented = FALSE; }
         else
         {
            if (lline+lword < lmax) { *dest++ = ' '; for (p = word; *p; p++) *dest++ = *p; lline += lword+1; }
            else { *dest++ = '\n'; for (p = word; *p; p++) *dest++ = *p; lline = lword; }
         }
      }
      if (c == '\n') { *dest++ = '\n'; lline = 0; }
      if (start > end) break;
   }
   if (lline) *dest++ = '\n';
   *dest-- = 0;
   return dest;
}
///
/// RemoveQuoteString
//  Removes reply prefix
LOCAL char *RemoveQuoteString(char *start, char *end, char *quot, char *dest)
{
   while (start <= end)
   {
      if (!strncmp(start, quot, strlen(quot))) start += strlen(quot);
      while (*start && *start != '\n') *dest++ = *start++;
      if (*start) *dest++ = *start++;
   }
   *dest-- = 0;
   return dest;
}
///
/// InsertQuoteString
//  Inserts reply prefix
LOCAL char *InsertQuoteString(char *start, char *quote, FILE *out)
{
   if ((*start != '\n' || C->QuoteEmptyLines) && strncmp(start, "<sb>", 4) && strncmp(start, "<tsb>", 5))
   {
      int level, i;
      for (i = level = 0; start[i] && start[i] != '\n' && i < 16; i++)
      {
         if (isSpace(start[i]) && (!level || start[i+1] == '>')) continue;
         if (start[i] == '>') level++; else if (!isAlNum(start[i]) || level) break;
      }
      if (level) start = &start[i];
      if (level > 8) level = 8;
      fputs(quote, out); while (level--) fputc('>', out);
      if (*start != ' ') fputc(' ', out);
   }
   return start;
}
///
/// SaveParagraph
//  Writes a paragraph and inserts reply prefixes
LOCAL void SaveParagraph(char *start, char *end, char *prefix, FILE *out)
{
   while (start <= end)
   {
      if (*prefix) start = InsertQuoteString(start, prefix, out);
      while (*start && *start != '\n') fputc(*start++, out);
      if (*start) fputc(*start++, out);
   }
}
///
/// QuoteWordWrap
//  Reformats quoted messages to a new line length
void QuoteWordWrap(char *rptr, int lmax, char *prefix, char *firstprefix, FILE *out)
{
   if (!prefix) prefix = firstprefix;
   while (*rptr)
   {
      char *p, *ps = rptr, *pe, quot[17], c;
      int lsm = 0;
      *quot = 0;
      while (TRUE)
      {
         int ls = 0;
         char *p = rptr;
         while (*rptr && *rptr != '\n') { rptr++; ls++; }
         if (ls && *p == '>' && !*quot)
         {
            int i = 0;
            while ((*p == ' ' || *p == '>') && i < 16) quot[i++] = *p++;
            quot[i] = 0;
            while (i > 1) if (quot[i-2] == ' ' && quot[i-1] == ' ') quot[--i] = 0; else break;
         }
         if (ls > lsm) lsm = ls;
         if (!*rptr) break;
         c = rptr[1];
         if (isSpace(c) || !c || c == '\n') break;
         if (!*quot && c == '>') break;
         if (*quot) if (strncmp(&rptr[1], quot, strlen(quot))) break;
         rptr++;
      }
      pe = rptr;
      if (lsm > lmax)
      {
         if (*quot)
         {
            char *buf = calloc(2*(pe-ps+2),1), newprefix[SIZE_DEFAULT];
            RemoveQuoteString(ps, pe, quot, buf);
            strcpy(newprefix, firstprefix); strcat(newprefix, TrimEnd(quot));
            QuoteWordWrap(buf, lmax-strlen(quot), newprefix, firstprefix, out);
            free(buf);
         }
         else
         {
            char *buf = calloc(2*(pe-ps+2),1);
            p = ReflowParagraph(ps, pe, lmax, buf);
            SaveParagraph(buf, p, prefix, out);
            free(buf);
         }
      }
      else SaveParagraph(ps, pe, prefix, out);
      rptr = pe+1;
   }
}
///
/// SimpleWordWrap
//  Reformats a file to a new line length
void SimpleWordWrap(char *filename, int wrapsize)
{
   BPTR fh;
   if (fh = Open((STRPTR)filename, MODE_OLDFILE))
   {
      char ch;
      int p = 0, lsp = -1, sol = 0;

      while (Read(fh, &ch, 1))
      {
         if (p-sol > wrapsize && lsp >= 0) 
         {
            ch = '\n';
            Seek(fh, lsp-p-1, OFFSET_CURRENT);
            p = lsp;
            Write(fh, &ch, 1);
         }
         if (ISpace(ch)) lsp = p;
         if (ch == '\n') { sol = p+1; lsp = -1; }
         p++;
      }
      Close(fh);
   }
}
///
/// ReqFile
//  Puts up a file requester
int ReqFile(int num, struct Object *win, char *title, int mode, char *drawer, char *file)
{   
   static BOOL init[MAXASL] = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };
   static char *pattern[MAXASL] =  { "#?.addressbook#?", "#?.config#?", NULL, NULL, "#?.(yam|rexx)", "#?.(gif|jpg|jpeg|png|iff|ilbm)", NULL, NULL };
   struct Window *truewin;
   int skip = *file ? 1 : 2;
   char *postext = (mode & 1)==1 ? GetStr(MSG_UT_Save) : GetStr(MSG_UT_Load);

   get(win, MUIA_Window_Window, &truewin);
   if (!init[num]) { init[num] = TRUE; skip = 0; }
   return MUI_AslRequestTags(G->ASLReq[num],
      ASLFR_Window, truewin,
      ASLFR_TitleText, title,
      ASLFR_PositiveText, postext,
      ASLFR_InitialFile, file,
      ASLFR_DoSaveMode, (mode & 1)==1,
      ASLFR_DoMultiSelect, (mode & 2)==2,
      ASLFR_DrawersOnly, (mode & 4)==4,
      ASLFR_DoPatterns, pattern[num] != NULL,
      skip ? TAG_DONE : ASLFR_InitialDrawer, drawer,
      ASLFR_InitialPattern, pattern[num] ? pattern[num] : "#?",
      TAG_DONE);
}
///
/// OpenTempFile
//  Creates or opens a temporary file
struct TempFile *OpenTempFile(char *mode)
{
   static int count = 0;
   struct TempFile *tf;
   if (tf = calloc(1, sizeof(struct TempFile)))
   {
      char buf[SIZE_SMALL];
      sprintf(buf, "YAM.%ld.tmp", ++count);
      strmfp(tf->Filename, C->TempDir, buf);
      if (!mode) return tf;
      if (tf->FP = fopen(tf->Filename, mode)) return tf;
      free(tf);
   }
   return NULL;
}
///
/// CloseTempFile
//  Closes a temporary file
void CloseTempFile(struct TempFile *tf)
{
   if (tf)
   {
      if (tf->FP) fclose(tf->FP);
      DeleteFile(tf->Filename);
      free(tf);
   }
}
///
/// DumpClipboard
//  Exports contents of clipboard unit 0 to a file
#define ID_FTXT   MAKE_ID('F','T','X','T')
#define ID_CHRS   MAKE_ID('C','H','R','S')
BOOL DumpClipboard(FILE *out)
{
   struct IFFHandle *iff;
   struct ContextNode *cn;
   long   error, rlen;
   UBYTE  readbuf[SIZE_DEFAULT];
   BOOL   success = FALSE;

   if (iff = AllocIFF())
   {
      if (iff->iff_Stream = (ULONG)OpenClipboard(PRIMARY_CLIP))
      {
         InitIFFasClip(iff);
         if (!OpenIFF(iff, IFFF_READ))
         {
            if (!StopChunk(iff, ID_FTXT, ID_CHRS)) while (TRUE)
            {
               error = ParseIFF(iff, IFFPARSE_SCAN);
               if (error == IFFERR_EOC) continue; else if (error) break;
               cn = CurrentChunk(iff);
               if (cn) if (cn->cn_Type == ID_FTXT && cn->cn_ID == ID_CHRS)
               {
                  success = TRUE;
                  while ((rlen = ReadChunkBytes(iff, readbuf, SIZE_DEFAULT)) > 0)
                     fwrite(readbuf, 1, rlen, out);
               }
            }
            CloseIFF(iff);
         }
         CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
      }
      FreeIFF(iff);
   }
   return success;
}
///
/// IsFolderDir
//  Checks if a directory is used as a mail folder
BOOL IsFolderDir(char *dir)
{
   char *filename = FilePart(dir);
   int i;
   for (i = 0; i < 4; i++) if (!stricmp(filename, FolderNames[i])) return TRUE;
   return (BOOL)(PFExists(dir, ".fconfig") || PFExists(dir, ".index"));
}
///
/// PFExists
//  Checks if a file exists in the specified directory
BOOL PFExists(char *path, char *file)
{
   char fname[SIZE_PATHFILE];
   strmfp(fname, path, file);
   return (BOOL)(FileSize(fname) >= 0);
}
///
/// DeleteMailDir
//  Recursively deletes a mail directory
void DeleteMailDir(char *dir, BOOL isroot)
{
   BOOL cont, isdir;
   struct FileInfoBlock *fib;
   BPTR lock;
   char fname[SIZE_PATHFILE], *filename, dirname[SIZE_PATHFILE];

   fib = AllocDosObject(DOS_FIB,NULL);
   if (lock = Lock(dir, ACCESS_READ))
   {
      strcpy(dirname, dir);
      Examine(lock, fib);
      cont = (ExNext(lock,fib) && IoErr() != ERROR_NO_MORE_ENTRIES);
      while (cont)
      {
         strmfp(fname, dir, fib->fib_FileName);
         filename = FilePart(fname);
         isdir = fib->fib_DirEntryType > 0;
         cont = (ExNext(lock,fib) && IoErr() != ERROR_NO_MORE_ENTRIES);
         if (isroot)
         {
            if (isdir)
            {
               if (IsFolderDir(fname)) DeleteMailDir(fname, FALSE);
            }
            else
            {
               if (!stricmp(filename, ".config") || !stricmp(filename, ".glossary") || !stricmp(filename, ".addressbook")) DeleteFile(fname);
            }
         }
         else if (!isdir) if (!stricmp(filename, ".fconfig") || !stricmp(filename, ".index") || IsValidMailFile(filename)) DeleteFile(fname);
      }
      UnLock(lock);
      DeleteFile(dirname);
   }
   FreeDosObject(DOS_FIB,fib);
}
///
/// FileToBuffer
//  Reads a complete file into memory
LOCAL char *FileToBuffer(char *file)
{
   char *text;
   int size = FileSize(file);
   FILE *fh;

   if (size >= 0) if (text = calloc(size+1,1))
   {
      if (fh = fopen(file, "r"))
      {
         fread(text, 1, size, fh);
         fclose(fh);
         return text;
      }
      free(text);
   }
   return NULL;
}
///

/*** Mail related ***/
/// MyAddTail
//  Adds a message to a message list
void MyAddTail(struct Mail **list, struct Mail *new)
{
   struct Mail *mail;
   new->Next = NULL;
   if (!*list) { *list = new; return; }
   for (mail = *list; mail->Next; mail = mail->Next);
   mail->Next = new;
}
///
/// MyRemove
//  Removes a message from a message list
void MyRemove(struct Mail **list, struct Mail *rem)
{
   struct Mail *mail;
   if (*list == rem) { *list = rem->Next; return; }
   for (mail = *list; mail->Next; mail = mail->Next)
      if (mail->Next == rem) { mail->Next = rem->Next; return; }
}
///
/// WhichLV
//  Returns pointer to message listview if folder is active
APTR WhichLV(struct Folder *folder)
{
   if (folder == FO_GetCurrentFolder()) return G->MA->GUI.NL_MAILS; else return NULL;
}
///
/// CreateFilename
//  Prepends mail directory to a file name
char *CreateFilename(char *file)
{
   static char buffer[SIZE_PATHFILE];
   strmfp(buffer, G->MA_MailDir, file);
   return buffer;
}
///
/// CreateDirectory
//  Makes a directory
BOOL CreateDirectory(char *dir)
{
   int t = FileType(dir);
   if (t == 2) return TRUE;
   if (t == 0) if (!mkdir(dir)) return TRUE;
   if (G->MA) ER_NewError(GetStr(MSG_ER_CantCreateDir), dir, NULL);
   return FALSE;
}
///
/// GetFolderDir
//  Returns path of a folder directory
char *GetFolderDir(struct Folder *fo)
{
   static char buffer[SIZE_PATH];
   if (strchr(fo->Path, ':')) return fo->Path;
   strmfp(buffer, G->MA_MailDir, fo->Path);
   return buffer;
}
///
/// GetMailFile
//  Returns path of a message file
char *GetMailFile(char *string, struct Folder *folder, struct Mail *mail)
{
   static char buffer[SIZE_PATHFILE];
   if (!folder && mail) folder = mail->Folder;
   if (!string) string = buffer;
   strmfp(string, (int)folder <= 0 ? C->TempDir : GetFolderDir(folder), mail->MailFile);
   return string;
}
///
/// GetMailInfo
//  Returns location of a message
struct MailInfo *GetMailInfo(struct Mail *smail)
{
   static struct MailInfo mi;
   int i;
   
   mi.Display = smail->Folder == FO_GetCurrentFolder();
   mi.Pos = -1;
   mi.FName = GetMailFile(NULL, smail->Folder, smail);
   if (mi.Display) for (i=0; mi.Pos == -1; i++)
   {
      struct Mail *mail;
      DoMethod(G->MA->GUI.NL_MAILS, MUIM_NList_GetEntry, i, &mail);
      if (!mail) break;
      if (mail == smail) mi.Pos = i;
   }
   return &mi;
}
///
/// GetReturnAddress
//  Gets return address of a message
struct Person *GetReturnAddress(struct Mail *mail)
{
   if (mail->ReplyTo.Address[0]) return &mail->ReplyTo;
   return &mail->From;
}
///
/// BuildAddrName
//  Creates "Real Name" <E-mail> string
char *BuildAddrName(char *address, char *name)
{
   static char buffer[SIZE_ADDRESS+SIZE_REALNAME+4];
   char *delim;

   if (*name)
   {
      if (strpbrk(name, ",.()")) delim = "\""; else delim = "";
      sprintf(buffer, "%s%s%s <%s>", delim, name, delim, address);
   }
   else sprintf(buffer, "%s", address);
   return buffer;
}
char *BuildAddrName2(struct Person *pe)
{
   return BuildAddrName(pe->Address, pe->RealName);
}
///
/// ExtractAddress
//  Extracts e-mail address and real name
void ExtractAddress(char *line, struct Person *pe)
{
   char *p = line, *ra[4], *save = malloc(strlen(line)+1);
   BOOL found = FALSE;

   ra[2] = ra[3] = NULL;
   strcpy(save, line);
   pe->Address[0] = pe->RealName[0] = 0;
   while (ISpace(*p)) p++;
   if (ra[0] = MyStrChr(p,'<')) if (ra[1] = MyStrChr(ra[0],'>'))
   {
      *ra[0]++ = 0; *ra[1] = 0;
      for (ra[2] = p, ra[3] = ra[0]-2; ISpace(*ra[3]) && ra[3] >= ra[2]; ra[3]--) *ra[3] = 0;
      found = TRUE;
   }
   if (!found)
   {
      for (ra[1] = ra[0] = p; *ra[1] && *ra[1] != '\t' && *ra[1] != ' ' && *ra[1] != ','; ra[1]++);
      if (ra[2] = MyStrChr(ra[1],'(')) if (ra[3] = MyStrChr(ra[2],')'))
      {
         ra[2]++; *ra[3]-- = 0;
         found = TRUE;
      }
      *ra[1] = 0;
      if (!found) ra[2] = ra[3] = "";
   }
   if (*ra[2] == '\"') ra[2]++;
   if (*ra[3] == '\"' && *(ra[3]-1) != '\\') *ra[3] = 0;
   stccpy(pe->Address , Trim(ra[0]), SIZE_ADDRESS);
   stccpy(pe->RealName, Trim(ra[2]), SIZE_REALNAME);
   strcpy(line, save);
   free(save);
}
///
/// CompressMsgID
//  Creates a compressed version of the message ID
int CompressMsgID(char *msgid)
{
   int i = 0, compr = 0;
   char *ptr = msgid;
   while (*ptr) compr += ++i*123*(*ptr++);
   return compr;
}
///
/// ExpandText
//  Replaces variables with values
char *ExpandText(char *src, struct ExpandTextData *etd)
{
   static char chr[2] = { 0,0 };
   char buf[SIZE_ADDRESS], *p, *p2, *dst = AllocStrBuf(SIZE_DEFAULT);
   struct DateStamp adate;
  
   for (; *src; src++)
      if (*src == '\\')
      {
         src++;
         switch (*src)
         {
            case '\\':  dst = StrBufCat(dst, "\\"); break;
            case 'n':  dst = StrBufCat(dst, "\n"); break;
         }
      }
      else if (*src == '%' && etd)
      {
         if (!etd->OM_Date) etd->OM_Date = DateStamp(&adate);
         src++;
         switch (*src)
         {
            case 'n': dst = StrBufCat(dst, etd->OS_Name); break;
            case 'f': stccpy(buf, etd->OS_Name, SIZE_ADDRESS);
                      if (p = strchr(buf, ',')) p = Trim(++p);
                      else { for (p = buf; *p && *p != ' '; p++); *p = 0; p = buf; }
                      dst = StrBufCat(dst, p); break;
            case 's': dst = StrBufCat(dst, etd->OM_Subject); break;
            case 'e': dst = StrBufCat(dst, etd->OS_Address); break;
            case 'd': dst = StrBufCat(dst, DateStamp2String(etd->OM_Date, DSS_DATE)); break;
            case 't': dst = StrBufCat(dst, DateStamp2String(etd->OM_Date, DSS_TIME)); break;
            case 'w': dst = StrBufCat(dst, DateStamp2String(etd->OM_Date, DSS_WEEKDAY)); break;
            case 'm': dst = StrBufCat(dst, etd->OM_MessageID); break;
            case 'r': dst = StrBufCat(dst, etd->R_Name); break;
            case 'v': strcpy(buf, etd->R_Name);
                      if (p = strchr(buf, ',')) p = Trim(++p);
                      else { for (p = buf; *p && *p != ' '; p++); *p = 0; p = buf; }
                      dst = StrBufCat(dst, p); break;
            case 'a': dst = StrBufCat(dst, etd->R_Address); break;
            case 'i': strcpy(buf, etd->OS_Name);
                      for (p = p2 = &buf[1]; *p; p++)
                         if (*p == ' ' && p[1] && p[1] != ' ') *p2++ = *++p;
                      *p2 = 0;
                      dst = StrBufCat(dst, buf); break;
            case 'j': strcpy(buf, etd->OS_Name);
                      for (p2 = &buf[1], p = &buf[strlen(buf)-1]; p > p2; p--)
                         if (p[-1] == ' ') { *p2++ = *p; break; }
                      *p2 = 0;
                      dst = StrBufCat(dst, buf); break;
            case 'h': if (p = FileToBuffer(etd->HeaderFile))
                      {
                         dst = StrBufCat(dst, p); free(p);
                      }
                      break;
        }
      }
      else
      {
         chr[0] = *src;
         dst = StrBufCat(dst, chr);
      }
   return dst;
}              
///
/// DescribeCT
//  Returns description of a content type
char *DescribeCT(char *ct)
{
   int i;
   for (i = 0; i < MAXCTYPE; i++)
      if (!stricmp(ct, ContType[i])) { ct = GetStr(ContTypeDesc[i]); break; }
   return ct;
}
///
/// GetDateStamp
//  Get number of seconds since 1/1-1978
time_t GetDateStamp(void)
{
	struct DateStamp ds;
	struct DateStamp *rds;

	rds = DateStamp(&ds);
	return (rds->ds_Days*24*60*60 + rds->ds_Minute*60 + rds->ds_Tick/TICKS_PER_SECOND);
}
///
/// DateStamp2String
//  Converts a datestamp to a string
char *DateStamp2String(struct DateStamp *date, int mode)
{
   static char resstr[32];
   char *s, datestr[16], timestr[16], daystr[16];
   struct DateTime dt;
   struct DateStamp dsnow;
   long beat;

   if (!date) date = DateStamp(&dsnow);
   clear(&dt, sizeof(struct DateTime));
   dt.dat_Stamp   = *date;
   dt.dat_Format  = FORMAT_DOS;
   if (mode == DSS_USDATETIME || mode == DSS_UNIXDATE) dt.dat_Format = FORMAT_USA;
   dt.dat_StrDate = (STRPTR)datestr;
   dt.dat_StrTime = (STRPTR)timestr;
   dt.dat_StrDay  = (STRPTR)daystr;
   DateToStr(&dt);
   beat = (((date->ds_Minute-60*C->TimeZone+(C->DaylightSaving?0:60)+1440)%1440)*1000)/1440;
   s = Trim(datestr);
   switch (mode)
   {
      case DSS_UNIXDATE:   sprintf(resstr, "%s %s %02ld %s %ld\n", wdays[dt.dat_Stamp.ds_Days%7], months[atoi(s)-1], atoi(&s[3]), timestr, 1900+atoi(&s[6])); break;
      case DSS_DATETIME:   timestr[5] = 0; sprintf(resstr, "%s %s", s, timestr); break;
      case DSS_DATEBEAT:   sprintf(resstr, "%s @%03ld", s, beat); break;
      case DSS_USDATETIME: sprintf(resstr, "%s %s", s, timestr); break;
      case DSS_WEEKDAY:    strcpy(resstr, daystr); break;
      case DSS_DATE:       strcpy(resstr, s); break;
      case DSS_TIME:       strcpy(resstr, timestr); break;
      case DSS_BEAT:       sprintf(resstr, "@%03ld", beat); break;
   }
   return resstr;
}
///
/// DateStamp2Long
// Converts a datestamp to a numeric value
long DateStamp2Long(struct DateStamp *date)
{
   char *s, datestr[16];
   struct DateStamp dsnow;
   struct DateTime dt;
   int y;

   if (!date) date = DateStamp(&dsnow);
   clear(&dt, sizeof(struct DateTime));
   dt.dat_Stamp   = *date;
   dt.dat_Format  = FORMAT_USA;
   dt.dat_StrDate = (STRPTR)datestr;
   DateToStr(&dt); s = Trim(datestr);
   if ((y = atoi(&s[6])) < 78) y += 100;
   return (100*atoi(&s[3])+atoi(s))*10000+y+1900;
}
///
/// GetTZ
//  Gets the locale time zone
char *GetTZ(void)
{
   static char tzone[SIZE_SMALL];
   if (GetVar("YAM_TZ", tzone, SIZE_SMALL, 0) < 0)
   {
      int tz = C->TimeZone + (C->DaylightSaving ? 1 : 0);
      if (C->TimeZone >= 0) sprintf(tzone, "+%02d00", tz);
      else                 sprintf(tzone, "-%02d00", -tz);
   }
   return tzone;
}
///
/// TZtoMinutes
//  Converts time zone into a numeric offset
LOCAL int TZtoMinutes(char *tzone)
{
   int i, tzcorr = 0;
   static struct TimeZones { char *TZname; int TZcorr; }
   tzones[] = { "sst",-1100, "pst",-800, "mst",-700, "pdt",-700, "cst",-600,
      "mdt",-600, "est",-500, "ast",-500, "edt",-400, "wgt",-300, "wgst",-200,
      "aat",-100, "egt",-100, "egst",0, "gmt",0, "utc",0, "wat",0, "wet",0,
      "bst",100, "cat",100, "cet",100, "met",100, "west",100, "cest",200,
      "met dst",200, "eet",200, "ist",200, "sat",200, "ast",300, "eat",300,
      "eest",300, "idt",300, "msk",300, "adt",400, "msd",300, "gst",400,
      "smt",400, "ict",400, "hkt",800, "wst",800, "jst",900, "kst",1000,
      "nzst",1200, "nzdt",1300, NULL,0 };

   if (tzone[0] == '+') tzcorr = atoi(&tzone[1]);
   else if (tzone[0] == '-') tzcorr = -atoi(&tzone[1]);
   else for (i = 0; tzones[i].TZname; i++)
      if (!strnicmp(tzones[i].TZname, tzone, strlen(tzones[i].TZname)))
      { tzcorr = tzones[i].TZcorr; break; }
   tzcorr = (tzcorr/100)*60 + (tzcorr%100);
   return tzcorr;
}
///
/// ScanDate
//  Converts textual date header into datestamp format
struct DateStamp *ScanDate(char *date)
{
   int count = 0, day = 0, mon = 0, year = 0, hour = 0, min = 0, sec = 0;
   char *tzone = "", *p, tdate[SIZE_SMALL], ttime[SIZE_SMALL];
   static struct DateTime dt;
   struct DateStamp *ds = &dt.dat_Stamp;

   if (p = strchr(date, ',')) p++; else p = date;
   while (*p && ISpace(*p)) p++;
   while (p = strtok(p, " \t"))
   {
      switch (count)
      {
         case 0: if (!isdigit(*p)) return NULL;
                 if ((day = atoi(p)) > 31) return NULL;
                 break;
         case 1: for (mon = 1; mon <= 12; mon++) if (!strnicmp(p, months[mon-1], 3)) break;
                 if (mon > 12) return NULL;
                 break;
         case 2: year = atoi(p);
                 break;
         case 3: if (sscanf(p, "%d:%d:%d", &hour, &min, &sec) == 3) ;
                 else if (sscanf (p, "%d:%d", &hour, &min) == 2) sec = 0;
                 else return NULL;
                 break;
         case 4: while (*p && (ISpace(*p) || *p == '(')) p++;
                 tzone = p;
                 break;
      }
      count++; p = NULL;
   }
   sprintf(tdate, "%02ld-%02ld-%02ld", mon, day, year%100);
   sprintf(ttime, "%02ld:%02ld:%02ld", hour, min, sec);
   dt.dat_Format  = FORMAT_USA;
   dt.dat_Flags   = 0;
   dt.dat_StrDate = (STRPTR)tdate;
   dt.dat_StrTime = (STRPTR)ttime;
   StrToDate(&dt);
   ds->ds_Minute -= TZtoMinutes(tzone);
   ds->ds_Minute += TZtoMinutes(GetTZ());
   while (ds->ds_Minute < 0)  { ds->ds_Minute += 1440; ds->ds_Days--; }
   while (ds->ds_Minute >= 1440) { ds->ds_Minute -= 1440; ds->ds_Days++; }
   return ds;
}
///
/// FormatSize
//  Displays large numbers using group separators
void FormatSize(int size, char *buffer)
{
   char *gs = G->Locale ? (char *)G->Locale->loc_GroupSeparator : ".";
   char *p = &buffer[strlen(buffer)];
   if (size >= 1000000) sprintf(p, "%d%s%03d%s%03d", size/1000000, gs, (size%1000000)/1000, gs, size%1000);
   if (size >= 1000 && size < 1000000) sprintf(p, "%d%s%03d", size/1000, gs, size%1000);
   if (size < 1000) sprintf(p, "%d", size);
}
///
/// MailExists
//  Checks if a message still exists
BOOL MailExists(struct Mail *mailptr, struct Folder *folder)
{
   struct Mail *work;
   if (Virtual(mailptr)) return TRUE;
   if (!folder) folder = mailptr->Folder;
   for (work = folder->Messages; work; work = work->Next) if (work == mailptr) return TRUE;
   return FALSE;
}
///
/// SelectMessage
//  Activates a message in the message listview
int SelectMessage(struct Mail *mail)
{
   struct MailInfo *mi;
   if (mail->Folder != FO_GetCurrentFolder()) MA_ChangeFolder(mail->Folder);
   mi = GetMailInfo(mail);
   if (mi->Pos >= 0) set(G->MA->GUI.NL_MAILS, MUIA_NList_Active, mi->Pos);
   return mi->Pos;
}
///
/// DisplayMailList
//  Lists folder contents in the message listview
void DisplayMailList(struct Folder *fo, APTR lv)
{
   struct Mail *work, **array;

   if (array = (struct Mail **)calloc(fo->Total+1,sizeof(struct Mail *)))
   {
      int i = 0;
      Busy(GetStr(MSG_BusyDisplayingList), "", 0, 0);
      for (work = fo->Messages; work; work = work->Next) array[i++] = work;
      set(lv, MUIA_NList_Quiet, TRUE);
      DoMethod(lv, MUIM_NList_Clear);
      DoMethod(lv, MUIM_NList_Insert, array, fo->Total, MUIV_NList_Insert_Sorted);
      set(lv, MUIA_NList_Quiet, FALSE);
      free(array);
      BusyEnd;
   }
}
///
/// AddMailToList
//  Adds a message to a folder
struct Mail *AddMailToList(struct Mail *mail, struct Folder *folder)
{
   struct Mail *new = malloc(sizeof(struct Mail));
   if (new) 
   {
      *new = *mail;
      new->Folder = folder;
      MyAddTail(&(folder->Messages), new);
      folder->Total++;
      folder->Size += mail->Size;
      if (mail->Status == STATUS_NEW) { folder->New++; folder->Unread++; }
      if (mail->Status == STATUS_UNR) folder->Unread++;
      MA_ExpireIndex(folder);
   }
   return new;
}
///
/// RemoveMailFromList
//  Removes a message from a folder
void RemoveMailFromList(struct Mail *mail)
{
   struct Folder *folder = mail->Folder;
   folder->Total--;
   folder->Size -= mail->Size;
   if (mail->Status == STATUS_NEW) { folder->New--; folder->Unread--; }
   if (mail->Status == STATUS_UNR) folder->Unread--;
   MA_ExpireIndex(folder);
   MyRemove(&(folder->Messages), mail);
   free(mail);
}
///
/// ClearMailList
//  Removes all messages from a folder
void ClearMailList(struct Folder *folder, BOOL resetstats)
{
   struct Mail *work, *next;
   for (work = folder->Messages; work; work = next)
   {
      next = work->Next;
      free(work);
   }
   if (resetstats) folder->Total = folder->New = folder->Unread = folder->Size = 0;
   folder->Messages = NULL;
}
///
/// GetPackMethod
//  Returns packer type and efficiency
LOCAL BOOL GetPackMethod(int xpktype, char **method, int *eff)
{
   if (xpktype == 2) { *method = C->XPKPack; *eff = C->XPKPackEff; return TRUE; }
   if (xpktype == 3) { *method = C->XPKPackEncrypt; *eff = C->XPKPackEncryptEff; return TRUE; }
   return FALSE;
}
///
/// CompressMailFile
//  Shrinks a message file
LOCAL BOOL CompressMailFile(char *src, char *dst, char *passwd, char *method, int eff)
{
   int err;
   if (!XpkBase) return FALSE;
   err = XpkPackTags(XPK_InName, src, XPK_OutName, dst, XPK_Password, passwd, XPK_PackMethod, method, XPK_PackMode, eff, TAG_DONE);
   return (BOOL)!err;
}
///
/// UncompressMailFile
//  Expands a compressed message file
LOCAL BOOL UncompressMailFile(char *src, char *dst, char *passwd)
{
   if (!XpkBase) return FALSE;
   return (BOOL)(!XpkUnpackTags(XPK_InName, src, XPK_OutName, dst, XPK_Password, passwd, TAG_DONE));
}
///
/// TransferMailFile
//  Copies or moves a message file, handles compression
BOOL TransferMailFile(BOOL copyit, struct Mail *mail, struct Folder *dstfolder)
{
   char *pmeth, srcbuf[SIZE_PATHFILE], dstbuf[SIZE_PATHFILE];
   struct Folder *srcfolder = mail->Folder;
   int peff = 0;
   int srcxpk = srcfolder->XPKType, dstxpk = dstfolder->XPKType;
   char *srcpw = srcfolder->Password, *dstpw = dstfolder->Password;
   BOOL one2one, needuncomp, needcomp, done = FALSE, success = FALSE;

   if (!MA_GetIndex(srcfolder)) return FALSE;
   if (!MA_GetIndex(dstfolder)) return FALSE;
   one2one = (srcxpk == dstxpk) && (srcxpk != 3);
   needuncomp = srcxpk > 1;
   needcomp   = dstxpk > 1;
   GetPackMethod(dstxpk, &pmeth, &peff);
   GetMailFile(srcbuf, srcfolder, mail);
   strcpy(dstbuf, MA_NewMailFile(dstfolder, mail->MailFile, atoi(mail->MailFile)));
   if (one2one && !copyit) if (done = RenameFile(srcbuf, dstbuf)) success = TRUE;
   if (!done)
   {
      if (needuncomp)
         if (needcomp)
            if (one2one)
               success = CopyFile(dstbuf, 0, srcbuf, 0);
            else
            {
               struct TempFile *tf = OpenTempFile(NULL);
               if (UncompressMailFile(srcbuf, tf->Filename, srcpw))
                  success = CompressMailFile(tf->Filename, dstbuf, dstpw, pmeth, peff);
               CloseTempFile(tf);
            }
         else
            success = UncompressMailFile(srcbuf, dstbuf, srcpw);
      else
         if (needcomp)
            success = CompressMailFile(srcbuf, dstbuf, dstpw, pmeth, peff);
         else
            success = CopyFile(dstbuf, 0, srcbuf, 0);
      if (success && !copyit) DeleteFile(srcbuf);
      if (success) SetComment(dstbuf, Status[mail->Status]);
   }
   return success;
}
///
/// RepackMailFile
//  (Re/Un)Compresses a message file
BOOL RepackMailFile(struct Mail *mail, int dstxpk, char *passwd)
{
   char *pmeth = NULL, srcbuf[SIZE_PATHFILE], dstbuf[SIZE_PATHFILE];
   struct Folder *folder = mail->Folder;
   int peff = 0, srcxpk = folder->XPKType;
   BOOL success = TRUE;

   MA_GetIndex(folder);
   GetMailFile(srcbuf, folder, mail);
   GetPackMethod(dstxpk, &pmeth, &peff);
   sprintf(dstbuf, "%s.tmp", srcbuf);
   switch (4*srcxpk+dstxpk)
   {
      case  0: case  5: case 10: case 15:
      case  1: case  4:                   return TRUE;
      case  2: case  3: case  6: case  7: if (success = CompressMailFile(srcbuf, dstbuf, passwd, pmeth, peff))
                                          {
                                             DeleteFile(srcbuf);
                                             success = RenameFile(dstbuf, srcbuf);
                                          }
                                          break;
      case  8: case 9:  case 12: case 13: if (success = UncompressMailFile(srcbuf, dstbuf, folder->Password))
                                          {
                                             DeleteFile(srcbuf);
                                             success = RenameFile(dstbuf, srcbuf);
                                          }
                                          break;
      case 11: case 14:                   if (success = UncompressMailFile(srcbuf, dstbuf, folder->Password))
                                          {
                                             success = CompressMailFile(dstbuf, srcbuf, passwd, pmeth, peff);
                                             DeleteFile(dstbuf);
                                          }
                                          break;
   }
   MA_SetMailStatus(mail, mail->Status);
   return success;
}
///
/// DoPack
//  Compresses a file
BOOL DoPack(char *file, char *newfile, struct Folder *folder)
{
   char *pmeth = NULL;
   int peff = 0;

   GetPackMethod(folder->XPKType, &pmeth, &peff);
   if (!CompressMailFile(file, newfile, folder->Password, pmeth, peff)) return FALSE;
   DeleteFile(file);
   return TRUE;
}
///
/// StartUnpack
//  Unpacks a file to a temporary file
char *StartUnpack(char *file, char *newfile, struct Folder *folder)
{
   FILE *fh;
   BOOL xpk = FALSE;
   if (fh = fopen(file, "r"))
   {
      if (fgetc(fh) == 'X') if (fgetc(fh) == 'P') if (fgetc(fh) == 'K') xpk = TRUE;
      fclose(fh);
      if (xpk)
      {
         char nfile[SIZE_FILE];
         stcgfn(nfile, file); sprintf(&nfile[strlen(nfile)], "_%08lx.unp", folder);
         strmfp(newfile, C->TempDir, nfile);
         if (FileSize(newfile) < 0) if (!UncompressMailFile(file, newfile, folder ? folder->Password : "")) return NULL;
      }
      else strcpy(newfile, file);
      return newfile;
   }
   return NULL;
}
///
/// FinishUnpack
//  Deletes temporary unpacked file
void FinishUnpack(char *file)
{
   if (strstr(file, ".unp")) DeleteFile(file);
}
///
/// IsValidMailFile
//  Checks if a message file name is valid
BOOL IsValidMailFile(char *fname)
{
   int l = strlen(fname);
   if (l < 7 || l > 10 || fname[5] != '.') return FALSE;
   while (--l >= 0) if (l != 5 && !isdigit(fname[l])) return FALSE;
   return TRUE;
}
///

/*** Editor related ***/
/// EditorToFile
//  Saves contents of a texteditor object to a file
BOOL EditorToFile(struct Object *editor, char *file, struct TranslationTable *tt)
{
   char *text;
   UBYTE *p;
   FILE *fh;

   if (!(fh = fopen(file, "w"))) return FALSE;
   text = (char *)DoMethod((Object *)editor, MUIM_TextEditor_ExportText);
   if (tt) for (p = text; *p; ++p) *p = tt->Table[*p];
   fputs(text, fh);
   fclose(fh);
   FreeVec(text);
   return TRUE;
}
///
/// FileToEditor
//  Loads a file into a texteditor object
BOOL FileToEditor(char *file, struct Object *editor)
{
   char *text = FileToBuffer(file);
   if (!text) return FALSE;
   set(editor, MUIA_TextEditor_Contents, text);
   free(text);
   return TRUE;
}
///

/*** Hooks ***/
/// GeneralDesFunc
//  General purpose destruction hook
SAVEDS ASM long GeneralDesFunc(REG(a1,void *entry))
{
   free(entry);
   return 0;
}
MakeHook(GeneralDesHook, GeneralDesFunc);
///
/// PO_SetPublicKey
//  Copies public PGP key from list to string gadget
SAVEDS ASM void PO_SetPublicKey(REG(a1,APTR string), REG(a2,APTR pop))
{
   char *var, buf[SIZE_SMALL];

   DoMethod(pop, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &var);
   if (var)
   {
      strcpy(buf, "0x");
      strncat(buf, var, 8);
      setstring(string, buf);
   }
}
MakeHook(PO_SetPublicKeyHook, PO_SetPublicKey);
///
/// PO_ListPublicKeys
//  Lists keys of public PGP keyring in a popup window
SAVEDS ASM long PO_ListPublicKeys(REG(a1,APTR string), REG(a2,APTR pop))
{  
   BOOL secret;
   char buf[SIZE_LARGE], *str, p;
   int retc, keys = 0;
   FILE *fp;

   get(pop, MUIA_UserData, &str); secret = (BOOL)(str != NULL);
   if (G->PGPVersion == 5)
      retc = PGPCommand("pgpk", "-l +language=us", KEEPLOG);
   else
   {
      strcpy(buf, "-kv  ");
      if (secret)
      {
         GetVar("PGPPATH", &buf[4], SIZE_DEFAULT, 0);
         if ((p = buf[strlen(buf)-1]) != ':' && p != '/') strcat(buf, "/");
         strcat(buf, "secring.pgp");
      }
      retc = PGPCommand("pgp", buf, KEEPLOG);
   }
   if (!retc) if (fp = fopen(PGPLOGFILE, "r"))
   {
      get(string, MUIA_String_Contents, &str);
      DoMethod(pop, MUIM_List_Clear);
      while (GetLine(fp, buf, SIZE_LARGE))
      {
         char entry[SIZE_DEFAULT];
         clear(entry, SIZE_DEFAULT);
         if (G->PGPVersion == 5)
         {
            if (!strncmp(buf, "sec", 3) || (!strncmp(&buf[1], "ub", 2) && !secret))
            {
               memcpy(entry, &buf[12], 8);
               while (GetLine(fp, buf, SIZE_LARGE))
                  if (!strncmp(buf, "uid", 3)) { strncat(entry, &buf[4], SIZE_DEFAULT-8); break; }
             }
         }
         else
         {
            if (buf[9] == '/' && buf[23] == '/')
            {
               memcpy(entry, &buf[10], 8);
               strncat(entry, &buf[29], SIZE_DEFAULT-8);
            }
         }
         if (*entry)
         {
            DoMethod(pop, MUIM_List_InsertSingle, entry, MUIV_List_Insert_Bottom);
            if (!strncmp(entry, str, 8)) set(pop, MUIA_List_Active, keys);
            keys++;
         }
      }
      fclose(fp);
      DeleteFile(PGPLOGFILE);
   }
   if (!keys) ER_NewError(GetStr(MSG_ER_NoPublicKeys), "", "");
   return keys > 0;
}
MakeHook(PO_ListPublicKeysHook, PO_ListPublicKeys);
///

/*** MUI related ***/
/// ShortCut
//  Finds keyboard shortcut in text label
char ShortCut(char *label)
{
   char *ptr = strchr(label, '_'), sc;

   if (!ptr) return 0;
   sc = ToLower(*++ptr);
   return sc;
}

/*********** Function isn't used anywhere! -msbethke **************
///
/// RemoveCut
//  Removes shortcut character from text label
LOCAL char *RemoveCut(char *label)
{
   static char lab[SIZE_DEFAULT], *p;

   for (p = lab; *label; label++) if (*label != '_') *p++ = *label;
   *p = 0;
   return lab;
}
*/

///
/// SetHelp
//  Sets bubble help of a MUI object
void SetHelp(APTR object, APTR strnum)
{
   set(object, MUIA_ShortHelp, GetStr(strnum));
}
///
/// MakeCycle
//  Creates a MUI cycle object
Object *MakeCycle(char **labels, char *label)
{
   Object *obj = KeyCycle(labels, ShortCut(label));
   if (obj) set(obj, MUIA_CycleChain, 1);
   return obj;
}
///
/// MakeButton
//  Creates a MUI button
Object *MakeButton(char *txt)
{
   Object *obj = MUI_MakeObject(MUIO_Button,txt);
   if (obj) set(obj, MUIA_CycleChain, 1);
   return obj;
}
///
/// MakeCheck
//  Creates a MUI checkmark object
Object *MakeCheck(char *label)
{
   return 
   ImageObject,
      ImageButtonFrame,
      MUIA_InputMode   , MUIV_InputMode_Toggle,
      MUIA_Image_Spec  , MUII_CheckMark, 
      MUIA_Background  , MUII_ButtonBack, 
      MUIA_ShowSelState, FALSE,
      MUIA_ControlChar , ShortCut(label),
      MUIA_CycleChain  , 1,
   End;
}
///
/// MakeCheckGroup
//  Creates a labelled MUI checkmark object
Object *MakeCheckGroup(Object **check, char *label)
{
   return 
   HGroup,
      Child, *check = MakeCheck(label),
      Child, Label(label),
      Child, HSpace(0),
   End;
}
///
/// MakeString
//  Creates a MUI string object
Object *MakeString(int maxlen, char *label)
{
   return BetterStringObject,
      StringFrame,
      MUIA_String_MaxLen     , maxlen,
      MUIA_String_AdvanceOnCR, TRUE,
      MUIA_ControlChar       , ShortCut(label),
      MUIA_CycleChain        , 1,
   End;
}
///
/// MakePassString
//  Creates a MUI string object with hidden text
Object *MakePassString(char *label)
{
   return BetterStringObject,
      StringFrame,
      MUIA_String_MaxLen     , SIZE_PASSWORD,
      MUIA_String_Secret     , TRUE,
      MUIA_String_AdvanceOnCR, TRUE,
      MUIA_ControlChar       , ShortCut(label),
      MUIA_CycleChain        , 1,
   End;
}
///
/// MakeInteger
//  Creates a MUI string object for numeric input
Object *MakeInteger(int maxlen, char *label)
{
   Object *str = MakeString(maxlen, label);
   if (str)
   {
      set(str, MUIA_String_Integer, 0);
      set(str, MUIA_String_Accept, "0123456789");
   }
   return str;
}
///
/// MakePGPKeyList
//  Creates a PGP id popup list
Object *MakePGPKeyList(APTR *st, BOOL secret, char *label)
{
   APTR po, lv;

   if (po = PopobjectObject,
         MUIA_Popstring_String, *st = MakeString(SIZE_DEFAULT, label),
         MUIA_Popstring_Button, PopButton(MUII_PopUp),
         MUIA_Popobject_StrObjHook, &PO_ListPublicKeysHook,
         MUIA_Popobject_ObjStrHook, &PO_SetPublicKeyHook,
         MUIA_Popobject_WindowHook, &PO_WindowHook,
         MUIA_Popobject_Object, lv = ListviewObject,
            MUIA_UserData, secret,
            MUIA_Listview_List, ListObject,
               InputListFrame,
               MUIA_List_AdjustWidth, TRUE,
               MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
               MUIA_List_DestructHook, MUIV_List_DestructHook_String,
            End,
         End,
      End)
      DoMethod(lv, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, po, 2, MUIM_Popstring_Close, TRUE);
   return po;
}
///
/// MakePicture
//  Creates a MUI image object that uses image datatypes
Object *MakePicture(char *fname)
{
   return G->DtpicSupported ?
      MUI_NewObject("Dtpic.mui", MUIA_Dtpic_Name, fname, End :
      NewObject(CL_BodyChunk->mcc_Class,NULL, MUIA_Bodychunk_File, fname, End;
}
///
/// MakeStatusFlag
//  Creates a MUI object for status images
Object *MakeStatusFlag(char *fname)
{
   return NewObject(CL_BodyChunk->mcc_Class,NULL,
      MUIA_Bodychunk_File, fname,
      MUIA_Bodychunk_UseOld, TRUE,
      MUIA_Bitmap_Transparent, 0,
   End;
}
///
/// MakeNumeric
//  Creates a MUI numeric slider
Object *MakeNumeric(int min, int max, BOOL percent)
{
   return
   NumericbuttonObject,
      MUIA_Numeric_Min, min,
      MUIA_Numeric_Max, max,
      MUIA_Numeric_Format, percent ? "%ld%%" : "%ld",
      MUIA_CycleChain, 1,
   End;
}
///
/// SetupToolbar
//  Initializes a single button in a MUI toolbar object
void SetupToolbar(struct MUIP_Toolbar_Description *tb, char *label, char *help, UWORD flags)
{
   tb->Type = label ? (*label ? TDT_BUTTON : TDT_SPACE) : TDT_END;
   tb->Flags = flags;
   tb->ToolText = tb->Type == TDT_BUTTON ? label : NULL;
   tb->HelpString = help;
   tb->MutualExclude = 0; tb->Key = 0;
}
///
/// SetupMenu
//  Initializes a MUI menu item
void SetupMenu(int type, struct NewMenu *menu, char *label, char *shortcut, int id)
{
   menu->nm_Type = type;
   menu->nm_Label = (STRPTR)label;
   menu->nm_CommKey = (STRPTR)shortcut;
   menu->nm_Flags = 0;
   menu->nm_MutualExclude = 0;
   menu->nm_UserData = (APTR)id;
}
///
/// DoSuperNew
//  Calls parent NEW method within a subclass
#ifdef __MORPHOS__
ULONG DoSuperNew(struct IClass *cl, Object *obj, ULONG tag1, ...)
{
   ULONG tags[4 * 2];
   va_list args;
   // SVR4 varargs to Tags hack.
   va_start(args, tag1);
   tags[0] = tag1;
   memcpy(tags + 1, args->reg_save_area + 3 * 4, 5 * 4);
   tags[6] = TAG_MORE;
   tags[7] = (ULONG)args->overflow_arg_area;
   va_end(args);
   return DoSuperMethod(cl, obj, OM_NEW, tags, NULL);
}
#else
ULONG DoSuperNew(struct IClass *cl, Object *obj, ULONG tag1, ...)
{
   return DoSuperMethod(cl, obj, OM_NEW, &tag1, NULL);
}
#endif
///
/// GetMUI
//  Gets an attribute value from a MUI object
int GetMUI(struct Object *obj,int attr)
{
   LONG b;
   get(obj,attr,&b);
   return (int)b;
}
///
/// GetMUIStringPtr
//  Returns a pointer to the value of a MUI string object
char *GetMUIStringPtr(struct Object *obj)
{
	return (char*)GetMUI(obj,MUIA_String_Contents);
}
///
/// GetMUIString
//  Returns the value of a MUI string object
void GetMUIString(char *a,struct Object *obj)
{
   strcpy(a,(char*)GetMUI(obj,MUIA_String_Contents));
}
///
/// GetMUIText
//  Returns the value of a MUI text object
void GetMUIText(char *a,struct Object *obj)
{
   strcpy(a,(char*)GetMUI(obj,MUIA_Text_Contents));
}
///
/// GetMUIInteger
//  Returns the numeric value of a MUI string object
int GetMUIInteger(struct Object *obj)
{
   return GetMUI(obj,MUIA_String_Integer);
}
///
/// GetMUICheck
//  Returns the value of a MUI checkmark object
BOOL GetMUICheck(struct Object *obj)
{
   return (BOOL)GetMUI(obj, MUIA_Selected);
}
///
/// GetMUICycle
//  Returns the value of a MUI cycle object
int GetMUICycle(struct Object *obj)
{
   return GetMUI(obj, MUIA_Cycle_Active);
}
///
/// GetMUIRadio
//  Returns the value of a MUI radio object
int GetMUIRadio(struct Object *obj)
{
   return GetMUI(obj, MUIA_Radio_Active);
}
///
/// GetMUINumer
//  Returns the value of a MUI numeric slider
int GetMUINumer(struct Object *obj)
{
   return GetMUI(obj, MUIA_Numeric_Value);
}
///
/// SafeOpenWindow
//  Tries to open a window
BOOL SafeOpenWindow(struct Object *obj)
{
   int isopen, isicon;
   set(obj, MUIA_Window_Open, TRUE);
   get(obj, MUIA_Window_Open, &isopen);
   get(_app(obj), MUIA_Application_Iconified, &isicon);
   if (isopen || isicon) return TRUE;
   DisplayBeep(0);
   return FALSE;
}
///
/// DisposeModule
//  Frees resources of a MUI window
void DisposeModulePush(void *module)
{
   DoMethod(G->App, MUIM_Application_PushMethod, G->App, 3, MUIM_CallHook, &DisposeModuleHook, module);
}
void DisposeModule(void *modptr)
{
   struct UniversalClassData **module = (struct UniversalClassData **)modptr;
   if (*module)
   {
      APTR window = (*module)->GUI.WI;
      set(window, MUIA_Window_Open, FALSE);
      DoMethod(G->App, OM_REMMEMBER, window);
      MUI_DisposeObject(window);
      free(*module);
      *module = NULL;
   }
}
SAVEDS ASM void DisposeModuleFunc(REG(a1,void **arg))
{
   DisposeModule(arg[0]);
}
MakeHook(DisposeModuleHook,DisposeModuleFunc);
///
/// LoadLayout
//  Loads column widths from ENV:MUI/YAM.cfg
void LoadLayout(void)
{
   char *ls;
   DoMethod(G->App, MUIM_Application_Load, MUIV_Application_Load_ENV);
   if (!*(ls = GetMUIStringPtr(G->MA->GUI.ST_LAYOUT))) ls = "35 100 25 100 30 100";
   sscanf(ls, "%ld %ld %ld %ld %ld %ld", &G->Weights[0], &G->Weights[1], &G->Weights[2], &G->Weights[3], &G->Weights[4], &G->Weights[5]);
}
///
/// SaveLayout
//  Saves column widths to ENV(ARC):MUI/YAM.cfg
void SaveLayout(BOOL permanent)
{
   char buf[SIZE_DEFAULT];
   sprintf(buf, "%ld %ld %ld %ld %ld %ld", G->Weights[0], G->Weights[1], G->Weights[2], G->Weights[3], G->Weights[4], G->Weights[5]);
   setstring(G->MA->GUI.ST_LAYOUT, buf);
   DoMethod(G->App, MUIM_Application_Save, MUIV_Application_Save_ENV);
   if (permanent) DoMethod(G->App, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
}
///
/// ConvertKey
//  Converts input event to key code
ULONG ConvertKey(struct IntuiMessage *imsg)
{
   struct InputEvent event;
   UBYTE code = 0;
   event.ie_NextEvent    = NULL;
   event.ie_Class        = IECLASS_RAWKEY;
   event.ie_SubClass     = 0;
   event.ie_Code         = imsg->Code;
   event.ie_Qualifier    = imsg->Qualifier;
   event.ie_EventAddress = (APTR *) *((ULONG *)imsg->IAddress);
   MapRawKey(&event, &code, 1, NULL);
   return code;
}
///

/**** BodyChunk ****/
/// FreeBCImage
//  Frees a bodychunk image
void FreeBCImage(struct BodyChunkData *bcd)
{
   if (bcd)
   {
      if (bcd->Colors) free(bcd->Colors);
      if (bcd->Body) free(bcd->Body);
      free(bcd);
   }
}
///
/// GetBCImage
//  Searches for a bodychunk image by filename
struct BodyChunkData *GetBCImage(char *fname)
{
   int i;
   for (i = 0; i < MAXIMAGES; i++)
      if (G->BImage[i]) if (!strcmp(G->BImage[i]->File, fname)) return G->BImage[i];
   return NULL;
}
///
/// LoadBCImage
//  Loads a bodychunk image from disk
struct BodyChunkData *LoadBCImage(char *fname)
{
   struct IFFHandle *iff;
   struct ContextNode *cn;
   struct BodyChunkData *bcd;

   if (bcd = calloc(1,sizeof(struct BodyChunkData)))
   {
      if (iff = AllocIFF())
      {
         if (iff->iff_Stream = Open(fname, MODE_OLDFILE))
         {
            InitIFFasDOS(iff);
            if (!OpenIFF(iff, IFFF_READ))
            {
               if (!ParseIFF(iff, IFFPARSE_STEP))
               {
                  if ((cn=CurrentChunk(iff)) && (cn->cn_ID == ID_FORM))
                  {
                     if (cn->cn_Type == ID_ILBM)
                     {
                        struct StoredProperty *sp;
                        struct BitMapHeader *bmhd;
                        int i, size;

                        if (!PropChunk (iff, ID_ILBM, ID_BMHD) &&
                            !PropChunk (iff, ID_ILBM, ID_CMAP) &&
                            !StopChunk (iff, ID_ILBM, ID_BODY) &&
                            !StopOnExit(iff, ID_ILBM, ID_FORM) &&
                            !ParseIFF  (iff, IFFPARSE_SCAN))
                        {
                           if (sp = FindProp(iff, ID_ILBM, ID_CMAP))
                           {
                              bcd->Colors = calloc(sp->sp_Size,sizeof(ULONG));
                              for (i = 0; i < sp->sp_Size; i++)
                              {
                                 UBYTE c = ((UBYTE *)sp->sp_Data)[i];
                                 bcd->Colors[i] = (c<<24)|(c<<16)|(c<<8)|c;
                              }
                           }
                           if (sp = FindProp(iff,ID_ILBM,ID_BMHD))
                           {
                              bmhd = (struct BitMapHeader *)sp->sp_Data;
                              if (bmhd->bmh_Compression == cmpNone || bmhd->bmh_Compression==cmpByteRun1)
                              {
                                 size = CurrentChunk(iff)->cn_Size;
                                 if (bcd->Body = calloc(size,1))
                                 {
                                    if (ReadChunkBytes(iff, bcd->Body, size) == size)
                                    {
                                       bcd->Width  = bmhd->bmh_Width;
                                       bcd->Height = bmhd->bmh_Height;
                                       bcd->Depth = bmhd->bmh_Depth;
                                       bcd->Compression = bmhd->bmh_Compression;
                                       bcd->Masking = bmhd->bmh_Masking;
                                       stcgfn(bcd->File, fname);
                                    }
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
            CloseIFF(iff);
         }
         Close(iff->iff_Stream);
      }
      FreeIFF(iff);
   }
   else return NULL;
   if (!bcd->Depth) { FreeBCImage(bcd); return NULL; }
   return bcd;
}
///

/*** Miscellaneous stuff ***/
/// PGPGetPassPhrase
//  Asks user for the PGP passphrase
void PGPGetPassPhrase(void)
{
   if (!G->PGPPassPhrase[0])
   {
      G->PGPPassVolatile = FALSE;
      if (GetVar("PGPPASS", G->PGPPassPhrase, SIZE_DEFAULT, 0) < 0)
      {
         char pgppass[SIZE_DEFAULT];
         G->PGPPassVolatile = TRUE; *pgppass = 0;
         if (StringRequest(pgppass, SIZE_DEFAULT, "PGP", GetStr(MSG_UT_PGPPassReq), GetStr(MSG_Okay), NULL, GetStr(MSG_Cancel), TRUE, G->MA->GUI.WI))
            strcpy(G->PGPPassPhrase, pgppass);
      }
      else return;
   }
   SetVar("PGPPASS", G->PGPPassPhrase, -1, GVF_GLOBAL_ONLY);
}
///
/// PGPClearPassPhrase
//  Clears the ENV variable containing the PGP passphrase
void PGPClearPassPhrase(BOOL force)
{
   if (G->PGPPassVolatile) DeleteVar("PGPPASS", 0);
   if (force) G->PGPPassPhrase[0] = 0;
}
///
/// PGPCommand
//  Launches a PGP command
int PGPCommand(char *progname, char *options, int flags)
{
   BPTR fhi,fho;
   int error = -1;
   char command[SIZE_LARGE];

	if (fhi = Open("NIL:", MODE_OLDFILE))
	{
	   if (fho = Open("NIL:", MODE_NEWFILE))
   	{
	      Busy(GetStr(MSG_BusyPGPrunning), "", 0, 0);
	      strmfp(command, C->PGPCmdPath, progname);
	      strcat(command, " >" PGPLOGFILE " ");
	      strcat(command, options);
	      error = SystemTags(command, SYS_Input, fhi, SYS_Output, fho, NP_StackSize, C->StackSize, TAG_DONE);
	      Close(fho);
	      BusyEnd;
		}
		Close(fhi);
   }
   if (error > 0 && !(flags & NOERRORS)) ER_NewError(GetStr(MSG_ER_PGPreturnsError), command, PGPLOGFILE);
   if (error < 0) ER_NewError(GetStr(MSG_ER_PGPnotfound), C->PGPCmdPath, NULL);
   if (!error && !(flags & KEEPLOG)) DeleteFile(PGPLOGFILE);
   return error;
}
///
/// AppendToLogfile
//  Appends a line to the logfile
LOCAL void AppendToLogfile(int id, char *text, void *a1, void *a2, void *a3, void *a4)
{
   FILE *fh;
   char logfile[SIZE_PATHFILE], filename[SIZE_FILE];
   if (!C->LogAllEvents && (id < 30 || id > 49)) return;
   if (C->SplitLogfile)
   {
      struct ClockData cd;
      Amiga2Date(GetDateStamp(), &cd);
      sprintf(filename, "YAM-%s%ld.log", months[cd.month-1], cd.year);
   }
   else strcpy(filename, "YAM.log");
   strmfp(logfile, *C->LogfilePath ? C->LogfilePath : G->ProgDir, filename);
   if (fh = fopen(logfile, "a"))
   {
      fprintf(fh, "%s [%02ld] ", DateStamp2String(NULL, DSS_DATETIME), id);
      fprintf(fh, text, a1, a2, a3, a4);
      fprintf(fh, "\n");
      fclose(fh);
   }
}
///
/// AppendLog
//  Appends a line to the logfile, depending on log mode
void AppendLog(int id, char *text, void *a1, void *a2, void *a3, void *a4)
{
   if (C->LogfileMode != 0) AppendToLogfile(id, text, a1, a2, a3, a4);
}
void AppendLogNormal(int id, char *text, void *a1, void *a2, void *a3, void *a4)
{
   if (C->LogfileMode == 1) AppendToLogfile(id, text, a1, a2, a3, a4);
}
void AppendLogVerbose(int id, char *text, void *a1, void *a2, void *a3, void *a4)
{
   if (C->LogfileMode == 2) AppendToLogfile(id, text, a1, a2, a3, a4);
}
///
/// Busy
//  Displays busy message and sleep pointer
int BusyLevel = 0;
void Busy(char *text, char *parameter, int cur, int max)
{
   static char infotext[SIZE_DEFAULT];
   if (text)
   {
      if (*text)
      {
         sprintf(infotext, text, parameter);
         if (G->MA && !BusyLevel)
         {
            set(G->MA->GUI.GA_INFO, MUIA_Gauge_InfoText, infotext);
            set(G->MA->GUI.GA_INFO, MUIA_Gauge_Max, max);
         }
         BusyLevel++;
         set(G->App, MUIA_Application_Sleep, TRUE);
      }
      else
      {
         set(G->App, MUIA_Application_Sleep, FALSE);
         if (BusyLevel) --BusyLevel;
         if (G->MA && !BusyLevel)
         {
            set(G->MA->GUI.GA_INFO, MUIA_Gauge_InfoText, "");
            set(G->MA->GUI.GA_INFO, MUIA_Gauge_Current, 0);
         }
      }
   }
   else
   {
      set(G->MA->GUI.GA_INFO, MUIA_Gauge_Current, cur);
   }
}
///
/// DisplayStatistics
//  Calculates folder statistics and update mailbox status icon
void DisplayStatistics(struct Folder *fo)
{
   struct Mail *mail;
   BOOL check = FALSE;
   struct Folder *actfo = FO_GetCurrentFolder();
   static char apptit[SIZE_DEFAULT/2];

   if (!fo) fo = actfo;
   if (fo == (struct Folder *)-1) { fo = FO_GetFolderByType(FT_INCOMING, NULL); check = TRUE; }
   for (mail = fo->Messages, fo->Unread = fo->New = 0; mail; mail = mail->Next)
   {
      if (mail->Status == STATUS_NEW) { fo->New++; fo->Unread++; }
      if (mail->Status == STATUS_UNR) fo->Unread++;
   }
   DoMethod(G->MA->GUI.NL_FOLDERS, MUIM_NList_Redraw, FO_GetFolderPosition(fo));
   if (fo == actfo) { MA_SetMessageInfoFunc(); MA_SetFolderInfoFunc(); }
   if (fo->Type == FT_INCOMING && !G->AppIconQuiet) if (check || fo->New != G->NewMsgs || fo->Total != G->TotMsgs)
   {
      int mode = fo->Total ? (fo->New ? 2 : 1) : 0;
      if (G->TR) if (G->TR->Checking) mode = 3;
      sprintf(apptit, GetStr(MSG_UT_AppStats), fo->New, fo->Total);
      if (G->AppIcon) { RemoveAppIcon(G->AppIcon); G->AppIcon = NULL; }
      if (G->DiskObj[mode])
      {
         struct DiskObject *dobj=G->DiskObj[mode];
         G->AppIcon = AddAppIconA(0, 0, (STRPTR)apptit, G->AppPort, NULL, dobj, NULL);
      }
      G->NewMsgs = fo->New; G->TotMsgs = fo->Total;
   }
}
///
/// CheckPrinter
//  Checks if printer is ready
BOOL CheckPrinter(void)
{
   struct MsgPort *PrintPort;
   struct IOStdReq *PrintIO;
   UWORD  Result = 0;
   char  _PrinterDeviceName[] = "printer.device";
   long  _PrinterDeviceUnit = 0;
   char *error = NULL;

   if (PrintPort = CreateMsgPort())
   {
      PrintPort->mp_Node.ln_Name = "YAM PrintPort";
      if (PrintIO = CreateIORequest(PrintPort, sizeof(struct IOStdReq)))
      {
         if (!(OpenDevice((STRPTR)_PrinterDeviceName, _PrinterDeviceUnit, (struct IORequest *)PrintIO, 0)))
         {
            PrintIO->io_Message.mn_ReplyPort = PrintPort;
            PrintIO->io_Command = PRD_QUERY;
            PrintIO->io_Data = &Result;
            DoIO((struct IORequest *)PrintIO);
            if (((Result>>8) & 3) == 0) error = NULL;
            else if ((Result>>8) & 01) error = GetStr(MSG_UT_NoPaper);
            else error = GetStr(MSG_UT_NoPrinter);
            CloseDevice((struct IORequest *)PrintIO);
         }
         DeleteIORequest((struct IORequest *)PrintIO);
      }
      DeleteMsgPort(PrintPort);
   }
   if (error) if (!MUI_Request(G->App, NULL, 0, GetStr(MSG_ErrorReq), GetStr(MSG_OkayCancelReq), error)) return False;
   return True;
}
///
/// PlaySound
//  Plays a sound file using datatypes
void PlaySound(char *filename)
{
   if (DataTypesBase)
   {
      struct dtTrigger Play = { DTM_TRIGGER, NULL, STM_PLAY, NULL };
      Object *sound;
      BYTE sig;
      if (sound = NewDTObject((APTR)filename, DTA_GroupID, GID_SOUND, SDTA_Volume, 64, SDTA_Cycles, 1, TAG_DONE))
      {
         if ((sig = AllocSignal(-1)) >= 0)
         {
            if (SetDTAttrs(sound, NULL, NULL, SDTA_SignalTask, FindTask(NULL), SDTA_SignalBit, 1<<sig, TAG_END) == 2)
            {
               DoDTMethodA(sound, NULL, NULL, (Msg)&Play);
               Wait(1<<sig);
            }
            else
            {
               ULONG length = 0, period = 394, cycles = 1, frequency, seconds;
               GetDTAttrs(sound, SDTA_SampleLength, &length, SDTA_Period, &period, SDTA_Cycles, &cycles, TAG_END);
               if (length > 131072) length = 131072;
               frequency = (ULONG)(SysBase->ex_EClockFrequency * 5) / period;
               seconds = (length * cycles) / frequency + 1;
               DoDTMethodA(sound, NULL, NULL, (Msg)&Play);
               Delay(seconds * 50);
            }
            FreeSignal(sig);
         }
         DisposeDTObject(sound);
      }
   }
}
///
/// MatchExtension
//  Matches a file extension against a list of extension
BOOL MatchExtension(char *fileext, char *extlist)
{
   while (extlist = strtok(extlist, " "))
   {
      if (!stricmp(extlist, fileext)) return TRUE;
      extlist = NULL;
   }
   return FALSE;
}
///
/// IdentifyFileDT
//  Detects the file type using datatypes.library
LOCAL char *IdentifyFileDT(char *fname)
{
   static char ctype[SIZE_CTYPE], *type = NULL;
   struct Library *DataTypesBase = OpenLibrary("datatypes.library", 39);
   strcpy(ctype, "application/octet-stream");
   if (DataTypesBase)
   {
      BPTR lock;
      if (lock = Lock(fname, ACCESS_READ))
      {
         struct DataType *dtn;
         if (dtn = ObtainDataTypeA(DTST_FILE, (APTR)lock, NULL))
         {
            struct DataTypeHeader *dth = dtn->dtn_Header;
            switch (dth->dth_GroupID)
            {
               case GID_SYSTEM:     break;
               case GID_DOCUMENT:   type = "application"; break;
               case GID_TEXT:       type = "text"; break;
               case GID_SOUND:
               case GID_INSTRUMENT: type = "audio"; break;
               case GID_PICTURE:    type = "image"; break;
               case GID_MOVIE:
               case GID_ANIMATION:  type = "video"; break;
            }
            if (type) sprintf(ctype, "%s/x-%s", type, dth->dth_BaseName);
            ReleaseDataType(dtn);
         }
         UnLock (lock);
      }
      CloseLibrary(DataTypesBase);
   }
   return ctype;
}
///
/// IdentifyFile
//  Detects the file type by analyzing file extension and contents
char *IdentifyFile(char *fname)
{
   char *ctype = "";
   long bits = FileProtection(fname);
   FILE *fh;

   if (fh = fopen(fname, "r"))
   {
      int i, len;
      char buffer[SIZE_LARGE], *ext;

      len = fread(buffer, 1, SIZE_LARGE-1, fh);
      buffer[len] = 0;
      fclose(fh);
      if (ext = strrchr(fname, '.')) ++ext; else ext = "--";
      if (!stricmp(ext, "htm") || !stricmp(ext, "html"))
         ctype = ContType[CT_TX_HTML];
      else if (!strnicmp(buffer, "@database", 9) || !stricmp(ext, "guide"))
         ctype = ContType[CT_TX_GUIDE];
      else if (!stricmp(ext, "ps") || !stricmp(ext, "eps"))
         ctype = ContType[CT_AP_PS];
      else if (!stricmp(ext, "rtf"))
         ctype = ContType[CT_AP_RTF];
      else if (!stricmp(ext, "lha") || !strncmp(&buffer[2], "-lh5-", 5))
         ctype = ContType[CT_AP_LHA];
      else if (!stricmp(ext, "lzx") || !strncmp(buffer, "LZX", 3))
         ctype = ContType[CT_AP_LZX];
      else if (!stricmp(ext, "zip"))
         ctype = ContType[CT_AP_ZIP];
      else if (*((long *)buffer) >= HUNK_UNIT && *((long *)buffer) <= HUNK_INDEX)
         ctype = ContType[CT_AP_AEXE];
      else if (!stricmp(ext, "rexx") || !stricmp(ext+strlen(ext)-2, "rx"))
         ctype = ContType[CT_AP_REXX];
      else if (!strncmp(&buffer[6], "JFIF", 4))
         ctype = ContType[CT_IM_JPG];
      else if (!strncmp(buffer, "GIF8", 4))
         ctype = ContType[CT_IM_GIF];
      else if (!strnicmp(ext, "png",4) || !strncmp(&buffer[1], "PNG", 3))
         ctype = ContType[CT_IM_PNG];
      else if (!strnicmp(ext, "tif",4))
         ctype = ContType[CT_IM_TIFF];
      else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "ILBM", 4))
         ctype = ContType[CT_IM_ILBM];
      else if (!stricmp(ext, "au") || !stricmp(ext, "snd"))
         ctype = ContType[CT_AU_AU];
      else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "8SVX", 4))
         ctype = ContType[CT_AU_8SVX];
      else if (!stricmp(ext, "wav"))
         ctype = ContType[CT_AU_WAV];
      else if (!stricmp(ext, "mpg") || !stricmp(ext, "mpeg"))
         ctype = ContType[CT_VI_MPG];
      else if (!stricmp(ext, "qt") || !stricmp(ext, "mov"))
         ctype = ContType[CT_VI_MOV];
      else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "ANIM", 4))
         ctype = ContType[CT_VI_ANIM];
      else if (!stricmp(ext, "avi"))
         ctype = ContType[CT_VI_AVI];
      else if (stristr(buffer, "\nFrom:"))
         ctype = ContType[CT_ME_EMAIL];
      else
      {
         for (i = 1; i < MAXMV; i++) if (C->MV[i])
            if (MatchExtension(ext, C->MV[i]->Extension)) ctype = C->MV[i]->ContentType;
      }
      if (!*ctype)
      {
         int notascii = 0;
         for (i = 0; i < len; i++)
            if ((int)buffer[i] < 32 || (int)buffer[i] > 127)
               if (buffer[i] != '\t' && buffer[i] != '\n') notascii++;
         if (notascii < len/10) ctype =  ContType[(bits&FIBF_SCRIPT) ? CT_AP_SCRIPT : CT_TX_PLAIN];
         else ctype = IdentifyFileDT(fname);
      }
   }
   return ctype;
}
///
/// LoadTranslationTable
//  Load a translation table into memory
BOOL LoadTranslationTable(struct TranslationTable **tt, char *file)
{
   FILE *fp;
   if (*tt) free(*tt);
   if (!*file) return FALSE;
   if (!(*tt = calloc(1,sizeof(struct TranslationTable)))) return FALSE;
   if (fp = fopen(file, "r"))
   {
      UBYTE buf[SIZE_DEFAULT], *p;
      int i;
      for (i = 0; i < 256; i++) (*tt)->Table[i] = (UBYTE)i;
      stccpy((*tt)->File, file, SIZE_PATHFILE);
      fgets(buf, SIZE_DEFAULT, fp);
      if (!strncmp(buf, "YCT1", 4))
      {
         fgets((*tt)->Name, SIZE_DEFAULT, fp);
         if (p = strchr((*tt)->Name,'\n')) *p = 0;
         while (fgets(buf, SIZE_DEFAULT, fp))
            if (!strnicmp(buf, "from", 4)) stccpy((*tt)->SourceCharset, Trim(&buf[5]), SIZE_CTYPE);
            else if  (!strnicmp(buf, "to", 2)) stccpy((*tt)->DestCharset, Trim(&buf[3]), SIZE_CTYPE);
            else if (!strnicmp(buf, "header", 6)) (*tt)->Header = TRUE;
            else if (!strnicmp(buf, "author", 6));
            else if (strchr(buf, '='))
            {
               int source, dest;
               p = buf;
               if (*p == '$') stch_i(&p[1], &source); else source = (int)*p;
               while (*p++ != '=');
               if (*p == '$') stch_i(&p[1], &dest); else dest = (int)*p;
               if (source >= 0 && source <= 0xFF && dest >= 0 && dest <= 0xFF) (*tt)->Table[source] = (UBYTE)dest;
            }
         fclose(fp);
         return TRUE;
      }
      fclose(fp);
   }
   ER_NewError(GetStr(MSG_ER_ErrorTTable), file, NULL);
   free(*tt); *tt = NULL;
   return FALSE;
}
///
/// ExecuteCommand
//  Executes a DOS command
BOOL ExecuteCommand(char *cmd, BOOL asynch, BPTR outdef)
{
   BPTR in, out, path;
   int ret;
   switch (outdef)
   {
      case NULL:    in = Input();out = Output(); break;
      case OUT_NIL: out = Open("NIL:", MODE_NEWFILE); in = Open("NIL:", MODE_OLDFILE); break;
      default:      out = outdef; in = Open("NIL:", MODE_OLDFILE); break;
   }
   if (!outdef) asynch = FALSE;
   if (_WBenchMsg)
   {
      path = CloneWorkbenchPath(_WBenchMsg);
      if ((ret = SystemTags(cmd, SYS_Input,in, SYS_Output,out, NP_Path,path, NP_StackSize,C->StackSize, SYS_Asynch,asynch, TAG_DONE)) == -1) FreeWorkbenchPath(path);
   }
   else ret = SystemTags(cmd, SYS_Input,in, SYS_Output,out, NP_StackSize,C->StackSize, SYS_Asynch,asynch, TAG_DONE);
   if (ret == -1 && asynch && outdef) { Close(out); Close(in); }
   return (BOOL)(!ret);
}
///
/// GetSimpleID
//  Returns a unique number
int GetSimpleID(void)
{
   static int num = 0;
   return ++num;
}
///
/// GotoURL
//  Loads an URL using an ARexx script or openurl.library
void GotoURL(char *url)
{
   if (C->RX[MACRO_URL].Script[0])
   {
      char newurl[SIZE_LARGE];
      sprintf(newurl, "%c%s%c", '"', url, '"');
      MA_StartMacro(MACRO_URL, newurl);
   }
   else if (OpenURLBase = OpenLibrary("openurl.library", 1))
   {
      URL_Open(url, TAG_DONE);
      CloseLibrary(OpenURLBase);
   }
}
///

/// strtok_r()
// Reentrant version of stdlib strtok()
// Call like this:
// char *next=input, *token, breakstring[]=", ";
// do	{ token = strtok_r(&next,breakstring); /* ... */ } while(next);
char *strtok_r(char **s, char *brk)
{
char *p,*ret;

	if((s == NULL) || (*s == NULL) || (brk == NULL))
		return NULL;

	/* find break character */
	if((p = strpbrk(*s,brk)))
	{
		/* if found, terminate string there */
		*p = '\0';

		/* scan forward to next non-break */
		do { p++; } while(*p && (strchr(brk,*p) != NULL));

		/* if *p is a nullbyte, then no more tokens */
		if(*p == '\0') p = NULL;

		/* save current *s to return it */
		ret = *s;

		/* and let *s point to first non-break */
		*s = p;

		return ret;
	}

	/* no break character found - *s gets NULL */
	ret = *s;
	*s = NULL;

	return ret;
}
///

/*** REXX interface support ***/

/// InsertAddresses
//  Appends an array of addresses to a string gadget
void InsertAddresses(APTR obj, char **addr, BOOL add)
{
   char *buf;
   get(obj, MUIA_String_Contents, &buf);
   if (*buf && add) DoMethod(obj, MUIM_BetterString_Insert, ", ", MUIV_BetterString_Insert_EndOfString);
   else setstring(obj, "");
   DoMethod(obj, MUIM_BetterString_Insert, *addr, MUIV_BetterString_Insert_EndOfString);
   while (*++addr)
   {
      DoMethod(obj, MUIM_BetterString_Insert, ", ", MUIV_BetterString_Insert_EndOfString);
      DoMethod(obj, MUIM_BetterString_Insert, *addr, MUIV_BetterString_Insert_EndOfString);
   }
}
///
/// AllocReqText
//  Prepare multi-line text for requesters, converts \n to line breaks
char *AllocReqText(char *s)
{
   char *d, *reqtext;
   d = reqtext = calloc(strlen(s)+1, 1);
   while (*s)
      if (*s == '\\' && s[1] == 'n') { *d++ = '\n'; s++; s++; }
      else *d++ = *s++;
   return reqtext;
}
///

/// SPrintF
//  sprintf() replacement with Locale support
void SPrintF(char *outstr, char *fmtstr, ...)
{
	struct Hook hook;
#ifdef __PPC__
	va_list ap;
	ULONG arg[32];
	int args = 0, i;

	for (i=0; fmtstr[i] != '\0'; i++) {
		if (fmtstr[i] == '%' && fmtstr[i+1] != '%') args++;
	}

	va_start(ap, fmtstr);
	for (i=0; i<args; i++) {
		arg[i] = va_arg(ap, ULONG);
	}
	va_end(ap);
#endif

	InitHook(&hook, putChar, outstr);

#ifdef __PPC__
	FormatString(G->Locale, fmtstr, &arg[0], &hook);
#else
	FormatString(G->Locale, fmtstr, &fmtstr+1, &hook);
#endif
}
///
/// putChar
//  Hook used by FormatString()
SAVEDS ASM void putChar(REG(a0, struct Hook *hook), REG(a1, char c), REG(a2, struct Locale *locale))
{
	char **tmp;

	((char *)hook->h_Data)[0] = c;
	tmp = (char **)(&hook->h_Data);
	(*tmp)++;
}
///
