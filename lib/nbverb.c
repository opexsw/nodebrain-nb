/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbverb.h 
*
* Title:    Verb Object Methods
*
* Function:
*
*   This file provides methods for NodeBrain VERB objects used internally
*   to parse commands.  The VERB type extends the OBJECT type defined in
*   nbobject.h.
*
* Synopsis:
*
*   #include "nb.h"
*
*   void initVerb();
*   struct VERB *newVerb();
*   void printVerb();
*   void printVerbAll();
* 
* Description
*
*   A verb object represents a verb within the NodeBrain language.  Verbs
*   are defined at initialization time only---verbs are never destroyed and
*   the language does not provide for user defined verbs.  The language is
*   extended only via node module commands.
*  
*=============================================================================
* Enhancements:
*
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2005-11-22 Ed Trettevik (original prototype version introduced in 0.6.4)
* 2010-06-19 eat 0.8.2  Making verbs objects so modules can contribute extensions
*            By making verbs objects we can manage them via the API in a way 
*            consistent with other types of objects.
* 2012-01-16 dtl        Checker updates.
* 2013-01-11 eat 0.8.13 Checker updates
* 2014-05-04 eat 0.9.02 Replaced newType with nbObjectType
*=============================================================================
*/
#include <nb/nbi.h>

struct TYPE *nb_verbType;

//int nbVerbDeclare(nbCELL context,char *ident,int authmask,int flags,void (*parse)(struct NB_CELL *context,char *verb,char *cursor),char *syntax){
int nbVerbDeclare(nbCELL context,char *ident,int authmask,int flags,void *handle,int (*parse)(struct NB_CELL *context,void *handle,char *verb,char *cursor),char *syntax){
  struct NB_VERB *verb;
  
  verb=(struct NB_VERB *)newObject(nb_verbType,NULL,sizeof(struct NB_VERB));
  //strcpy(verb->verb,ident);   // depend on caller not to pass verb over 15 characters
  verb->authmask=authmask;
  verb->flags=flags;
  verb->handle=handle;
  verb->parse=parse;
  verb->syntax=syntax;
  verb->term=nbTermNew(context->object.type->stem->verbs,ident,verb,0);
  if(trace) outMsg(0,'T',"verb created - %s\n",ident);
  //outMsg(0,'T',"verb created - [%d]%s\n",verb->term->word->object.refcnt,ident);
  return(0);
  }

void nbVerbLoad(nbCELL context,char *ident){
  char *cursor,modName[256],msg[NB_MSGSIZE];
  size_t len;

  cursor=strchr(ident,'.');
  if(!cursor){
    outMsg(0,'E',"Expecting '.' in identifier %s",ident);
    return;
    }
  len=cursor-ident;
  if(len>=sizeof(modName)){
    outMsg(0,'E',"Module must not exceed %d characters in identifier %s",sizeof(modName)-1,ident);
    return;
    }
  strncpy(modName,ident,len); 
  *(modName+len)=0;
  nbModuleBind(context,modName,msg,sizeof(msg));
  }

struct NB_VERB *nbVerbFind(nbCELL context,char *ident){
  struct NB_STEM *stem=context->object.type->stem;
  struct NB_TERM *term;

  term=nbTermFindDown(stem->verbs,ident);
  if(!term && strchr(ident,'.')){
    nbVerbLoad(context,ident);
    term=nbTermFindDown(stem->verbs,ident);
    }
  if(!term) return(NULL);
  return((struct NB_VERB *)term->def);
  }

/**********************************************************************
* Object Management Methods
**********************************************************************/
void nbVerbPrint(struct NB_VERB *verb){
  outPut("verb ::= %s",verb->syntax);
  }

//void nbVerbPrintAll(struct NB_VERB *verb){
void nbVerbPrintAll(nbCELL context){
  struct NB_STEM *stem;
  outPut("Command Syntax:\n");
  outPut(
    " <command> ::= [<context>. ]<command> |\n"
    "               <context>:<node_command> |\n"
    "               <context>(<list>):<node_command> |\n"
    "               #<comment>\n"
    "               ^<stdout_message>\n"
    "               -[|][:]<shell_command>\n"
    "               =[|][:]<shell_command>\n"
    "               {<rule>}\n"
    "               ?<cell>\n"
    "               (<option>[,...])<command>\n"
    "               `<assertion>\n"
    " <context> ::= <term>      # defined as a node\n"
    " <term>    ::= <ident>[.<term>]\n"
    " <ident>   ::= <alpha>[<alphanumerics>]\n"
    "-------------------------------------------------\n");
  outPut("Verb Table:\n");
  stem=context->object.type->stem;
  nbTermPrintGloss((nbCELL)stem->verbs,(nbCELL)stem->verbs);
  }

void nbVerbDestroy(struct NB_VERB *verb){
  nbFree(verb,sizeof(struct NB_VERB));
  }

/**********************************************************************
* Public Methods
**********************************************************************/
void nbVerbInit(NB_Stem *stem){
  nb_verbType=nbObjectType(stem,"verb",0,0,nbVerbPrint,nbVerbDestroy);
  nb_verbType->apicelltype=NB_TYPE_VERB;
  stem->verbs=nbTermNew(NULL,"verb",useString("verb"),0);
  }
