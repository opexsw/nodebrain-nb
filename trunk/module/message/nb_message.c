/*
* Copyright (C) 2009-2010 The Boeing Company
*                         Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place Suite 330, Boston, MA 02111-1307, USA.
*
*=========================================================================
*
* Name:     nb_message.c
*
* Title:    Message Broadcasting Module 
*
* Function:
*
*   This program is a NodeBrain node module for broadcasting messages
*   to all nodes that are members of a group called a message "cabal".
*   It provides the following skills.
*
*     producer  - node that writes NodeBrain commands to a message log
*     consumer  - node that reads and executes NodeBrain commands from a message log
*     client    - node that reads and executes NodeBrain commands from a message server
*     server    - node that serves message from a message log to message clients
*
*   The server skill is general purpose---not specific to the processing
*   of NodeBrain commands.  The other skills are devotes to NodeBrain
*   command processing.  However, they also serve as examples for the development
*   of other applications that need to replicate transactions for processing
*   at multiple nodes.
*
* Description:
*
*   The module is a companion to a NodeBrain messaging API (nbmsg.c).
*   The API enables any NodeBrain module to function as a message
*   producer.  The "producer" skill provided by this module is a
*   special case that executes and logs nodebrain commands.  Other
*   modules (e.g. bingo) may use messages to represent transactions
*   that are foreign to NodeBrain.
*
*   A producer (in term of this module) is defined as follows.
*
*     define <term> node message.producer("<cabal>",<node>);
*     # <cabal>    - name given to a set of cooperating nodes
*     # <node>     - number of this node within the cabal
*     <term>. define filesize cell <filesize>; # maximum file size
*
*   Messages can be initiated via the node command.
*
*     <term>:<message>
*
*   Received messages are passed to the interpreter and written to a log
*   file in the message directory.
*
*     CABOODLE/message/<cabal>/n<node>/m<time>.nbm
*
*   Messages are also received from consumers.  A producer has a
*   preferred consumer.
*
*   A consumer is defined as follows.
*
*     define <term> node message.consumer("<cabal>",<node>);
*     # <cabal>    - name given to a set of cooperating nodes
*     # <node>     - number of this node within the cabal
*     <term>. define retry cell <retry>; # seconds between connection retries
*
*   Messages are read from a log by a consumer and sent to a prefered
*   peer within the cabal. 
*
*===========================================================================
* Change History:
*
* Date       Name/Change
* ---------- ---------------------------------------------------------
* 2009-12-13 Ed Trettevik - original prototype version 0.7.7
* 2009-12-30 eat 0.7.7  Organized into producer/consumer/server/client/peer skills
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
*===================================================================================
*/
#include "config.h"
#include <nb.h>
#include <nb-tls.h>
#include <nbtls.h>

/*==================================================================================
*  Producer - just uses the Message Log API (not Message Peer API)
*
*=================================================================================*/

/*
*  The following structure is created by the skill module's "construct"
*  function (messageContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_PRODUCER{    // message.producer node descriptor
  char           cabalName[32];    // cabal name identifying a group of nodes
  char           nodeName[32];     // name of node within cabal
  int            cabalNode;        // number of node within cabal
  nbMsgLog      *msglog;           // msglog structure
  unsigned char  trace;            // trace option 
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } nbModProducer;

/*==================================================================================
*
*  M E T H O D S
*
*  The code above this point is very specific to the goals of this skill module. The
*  code below this point is also specific in some of the details, but the general
*  structure applies to any skill module.  The functions below are "methods" called
*  by NodeBrain.  Their parameters must conform to the NodeBrain Skill Module
*  API.  A module is not required to provide all possible methods, so modules may
*  vary in the set of methods they implement.
*
*=================================================================================*/

/*
*  construct() method
*
*    define <term> node message.producer("<cabal>","<node>");
*/
void *producerConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModProducer *producer;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char cabalName[32];
  char nodeName[32];
  int cabalNode=0;
  int type,trace=0,dump=0,echo=1;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Cabal name required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  strcpy(cabalName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node name required as second argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Second argument must be string identifying node");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"Second argument must node exceed %d characters",sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(nodeName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    nbLogMsg(context,0,'E',"The message.producer skill only accepts two arguments.");
    return(NULL);
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim!=NULL){
      saveDelim=*delim;
      *delim=0;
      }
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"silent")==0) echo=0; 
    if(delim!=NULL){
      *delim=saveDelim;
      cursor=delim;
      while(*cursor==' ' || *cursor==',') cursor++;
      }
    else cursor=strchr(cursor,0);
    }
  producer=nbAlloc(sizeof(nbModProducer));
  memset(producer,0,sizeof(nbModProducer));
  strcpy(producer->cabalName,cabalName);
  strcpy(producer->nodeName,nodeName);
  producer->cabalNode=cabalNode;
  producer->msglog=NULL;
  producer->trace=trace;
  producer->dump=dump;
  producer->echo=echo;
  nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(producer);
  }

/*
*  enable() method
*
*    enable <node>
*/
int producerEnable(nbCELL context,void *skillHandle,nbModProducer *producer){
  nbMsgLog *msglog;
  nbMsgState *msgstate;
  int state;

  // We set the message state to NULL to let the message log define our state
  // for other applications we may want to initialize a message state and
  // reprocess messages from the log to resynchronize
  //msgstate=nbMsgStateCreate(context);
  msgstate=NULL;
  //time(&utime);
  //msgTime=utime;
  //msgCount=0;
  //nbMsgStateSet(msgstate,producer->cabalNode,msgTime,msgCount);
  msglog=nbMsgLogOpen(context,producer->cabalName,producer->nodeName,producer->cabalNode,"",NB_MSG_MODE_PRODUCER,msgstate);
  if(!msglog){
    nbLogMsg(context,0,'E',"Unable to open message log for cabal \"%s\" node %d",producer->cabalName,producer->cabalNode);
    return(1);
    }
  producer->msglog=msglog;
  while(!((state=nbMsgLogRead(context,msglog))&NB_MSG_STATE_LOGEND));
  nbLogMsg(context,0,'T',"State after read loop is %d",state);
  if(state<0){
    nbLogMsg(context,0,'E',"Unable to read to end of file for cabal \"%s\" node %d",msglog->cabal,msglog->node);
    return(1);
    }
  state=nbMsgLogProduce(context,msglog,1024*1024);
  //state=nbMsgLogProduce(context,msglog,10*1024*1024);
  nbLogMsg(context,0,'T',"Return from nbMsgLogProduce() is %d",state);
  nbLogMsg(context,0,'I',"Message.producer enabled for cabal \"%s\" node %d",msglog->cabal,msglog->node);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int producerDisable(nbCELL context,void *skillHandle,nbModProducer *producer){
  nbMsgLogClose(context,producer->msglog);
  producer->msglog=NULL;
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *producerCommand(nbCELL context,void *skillHandle,nbModProducer *producer,nbCELL arglist,char *text){
  if(producer->trace){
    nbLogMsg(context,0,'T',"nb_message:producerCommand() text=[%s]\n",text);
    }
  if(producer->msglog) nbMsgLogWriteString(context,producer->msglog,(unsigned char *)text);
  else nbLogMsg(context,0,'E',"nb_message:producerCommand():message log not open");
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int producerDestroy(nbCELL context,void *skillHandle,nbModProducer *producer){
  nbLogMsg(context,0,'T',"producerDestroy called");
  if(producer->msglog) producerDisable(context,skillHandle,producer);
  nbFree(producer,sizeof(nbModProducer));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *producerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,producerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,producerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,producerEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,producerCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,producerDestroy);
  return(NULL);
  }


/*==================================================================================
*  Consumer
*
*=================================================================================*/

/*
*  The following structure is created by the skill module's "construct"
*  function (messageContruct) defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_CONSUMER{  // message.consumer node descriptor
  char           cabalName[32];    // cabal name identifies a group of nodes
  char           nodeName[32];     // name of node within cabal
  int            cabalNode;        // number of node within cabal
  nbMsgState    *msgstate;         // consumer message state
  nbMsgLog      *msglog;           // msglog structure
  unsigned char  trace;            // trace option 
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } nbModConsumer;

int consumerMessageHandler(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbModConsumer *consumer=handle;
  char *cmd;
  int cmdlen;

  if(consumer->trace) nbLogMsg(context,0,'T',"consumerMessageHandler: called");
  cmd=nbMsgData(context,msgrec,&cmdlen);
  nbCmd(context,cmd,1);
  return(0);
  }

/*==================================================================================
*
*  M E T H O D S
*
*  The code above this point is very specific to the goals of this skill module. The
*  code below this point is also specific in some of the details, but the general
*  structure applies to any skill module.  The functions below are "methods" called
*  by NodeBrain.  Their parameters must conform to the NodeBrain Skill Module
*  API.  A module is not required to provide all possible methods, so modules may
*  vary in the set of methods they implement.
*
*=================================================================================*/

/*
*  construct() method
*
*    define <term> node message.consumer("<cabal>",<node>);
*/
void *consumerConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModConsumer *consumer;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char cabalName[32];
  char nodeName[32];
  int cabalNode=0;
  int type,trace=0,dump=0,echo=1;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Cabal name required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"First argument must not exceed %d characters",sizeof(cabalName)-1);
    return(NULL);
    }
  strcpy(cabalName,str);  // size checked
  nbCellDrop(context,cell);

  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node name required as second argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Second argument must be string identifying a node within the cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"Second argument must not exceed %d characters",sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(nodeName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    nbLogMsg(context,0,'E',"The message.consumer skill only accepts three arguments.");
    return(NULL);
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim!=NULL){
      saveDelim=*delim;
      *delim=0;
      }
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"silent")==0) echo=0; 
    if(delim!=NULL){
      *delim=saveDelim;
      cursor=delim;
      while(*cursor==' ' || *cursor==',') cursor++;
      }
    else cursor=strchr(cursor,0);
    }
  consumer=malloc(sizeof(nbModConsumer));
  memset(consumer,0,sizeof(nbModConsumer));
  strcpy(consumer->cabalName,cabalName);
  strcpy(consumer->nodeName,nodeName);
  consumer->cabalNode=cabalNode;
  consumer->trace=trace;
  consumer->dump=dump;
  consumer->echo=echo;
  nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(consumer);
  }

/*
*  enable() method
*
*    enable <node>
*/
int consumerEnable(nbCELL context,void *skillHandle,nbModConsumer *consumer){
  consumer->msgstate=nbMsgStateCreate(context);
  consumer->msglog=nbMsgLogOpen(context,consumer->cabalName,consumer->nodeName,consumer->cabalNode,"",NB_MSG_MODE_CONSUMER,consumer->msgstate);
  if(!consumer->msglog){
    nbLogMsg(context,0,'E',"nbMsgCacheOpen: Unable to open message log for cabal \"%s\" node %d",consumer->cabalName,consumer->cabalNode);
    return(1);
    }
  if(nbMsgLogConsume(context,consumer->msglog,consumer,consumerMessageHandler)!=0){
    nbLogMsg(context,0,'E',"Unable to register message handler for cabal \"%s\" node %d",consumer->cabalName,consumer->cabalNode);
    return(1);
    }
  nbLogMsg(context,0,'I',"Enabled for cabal \"%s\" node \"%s\"",consumer->cabalName,consumer->nodeName);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int consumerDisable(nbCELL context,void *skillHandle,nbModConsumer *consumer){
  //nbMsgCacheClose(context,consumer->msgcache);
  // take care of TLS structures here
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *consumerCommand(nbCELL context,void *skillHandle,nbModConsumer *consumer,nbCELL arglist,char *text){
  if(consumer->trace){
    nbLogMsg(context,0,'T',"nb_message:consumerCommand() text=[%s]\n",text);
    }
  nbCmd(context,text,1);
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int consumerDestroy(nbCELL context,void *skillHandle,nbModConsumer *consumer){
  nbLogMsg(context,0,'T',"consumerDestroy called");
  consumerDisable(context,skillHandle,consumer);
  free(consumer);
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *consumerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,consumerConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,consumerDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,consumerEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,consumerCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,consumerDestroy);
  return(NULL);
  }

/*==================================================================================
*  Peer
*
*=================================================================================*/

/*
*  The following structure is created by the skill module's "construct"
*  function defined in this file.  This is a module specific structure.
*  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_CLIENT{      // message.client node descriptor
  char           cabalName[32];    // cabal name
  char           nodeName[32];     // node name within cabal
  nbMsgCabal     *msgclient;        // client mode peer
  unsigned char  trace;            // trace option 
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } nbModClient;

/*
*  Client message handler
*/
int clientMessageHandler(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbModClient *client=handle;
  char *cmd;
  int cmdlen;

  if(client->trace) nbLogMsg(context,0,'T',"clientMessageHandler: called");
  cmd=nbMsgData(context,msgrec,&cmdlen);
  nbCmd(context,cmd,1);
  return(0);
  }

/*==================================================================================
*
*  M E T H O D S
*
*  The code above this point is very specific to the goals of this skill module. The
*  code below this point is also specific in some of the details, but the general
*  structure applies to any skill module.  The functions below are "methods" called
*  by NodeBrain.  Their parameters must conform to the NodeBrain Skill Module
*  API.  A module is not required to provide all possible methods, so modules may
*  vary in the set of methods they implement.
*
*=================================================================================*/

/*
*  construct() method
*
*    define <term> node message.peer("<cabal>",<node>,<port>);
*    <term>. define filelines cell <filelines>; # number of lines per file
*/
void *clientConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModClient *client;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char cabalName[32];
  char nodeName[32];
  int type,trace=0,dump=0,echo=1;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Cabal name required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  strcpy(cabalName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node name required as second argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Second argument must be string identifying node within cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"Second argument has node name \"%s\" too long for buffer - limit is %d characters",str,sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(nodeName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    nbLogMsg(context,0,'E',"The message.client skill only accepts two arguments.");
    return(NULL);
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim!=NULL){
      saveDelim=*delim;
      *delim=0;
      }
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"silent")==0) echo=0; 
    if(delim!=NULL){
      *delim=saveDelim;
      cursor=delim;
      while(*cursor==' ' || *cursor==',') cursor++;
      }
    else cursor=strchr(cursor,0);
    }
  client=malloc(sizeof(nbModClient));
  memset(client,0,sizeof(nbModClient));
  strcpy(client->cabalName,cabalName);
  strcpy(client->nodeName,nodeName);
  client->msgclient=NULL;
  client->trace=trace;
  client->dump=dump;
  client->echo=echo;
  nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(client);
  }

/*
*  enable() method
*
*    enable <node>
*/
int clientEnable(nbCELL context,void *skillHandle,nbModClient *client){
  // We call nbMsgCabalClient with a NULL message state to let the message log
  // define our state.  For other applications we may want to set the message state
  // from another source, and reprocess message from the log to resynchronize.
  if(!client->msgclient) client->msgclient=nbMsgCabalClient(context,client->cabalName,client->nodeName,NULL,client,clientMessageHandler);
  if(!client->msgclient){
    nbLogMsg(context,0,'E',"Unable to instantiate message client for cabal \"%s\" node \"%s\"",client->cabalName,client->nodeName);
    return(1);
    }
  nbMsgCabalEnable(context,client->msgclient);
  nbLogMsg(context,0,'I',"Enabled for cabal \"%s\" node \"%s\"",client->cabalName,client->nodeName);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int clientDisable(nbCELL context,void *skillHandle,nbModClient *client){
  nbMsgCabalDisable(context,client->msgclient);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *clientCommand(nbCELL context,void *skillHandle,nbModClient *client,nbCELL arglist,char *text){
  if(client->trace){
    nbLogMsg(context,0,'T',"nb_message:clientCommand() text=[%s]\n",text);
    }
  nbCmd(context,text,1);
  if(client->msgclient->msglog) nbMsgLogWriteString(context,client->msgclient->msglog,(unsigned char *)text);
  else nbLogMsg(context,0,'E',"nb_message:clientCommand():message log file not open");
  return(0);
  }


/*
*  destroy() method
*
*    undefine <node>
*/
int clientDestroy(nbCELL context,void *skillHandle,nbModClient *client){
  nbLogMsg(context,0,'T',"clientDestroy called");
  if(client->msgclient) clientDisable(context,skillHandle,client);
  nbFree(client,sizeof(nbModClient));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *clientBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,clientConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,clientDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,clientEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,clientCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,clientDestroy);
  return(NULL);
  }


/*==================================================================================
*  Server
*
*=================================================================================*/

/*
*  The following structure is created by the skill module's "construct"
*  function defined in this file.  This is a module specific
*  structure.  NodeBrain is only aware of the address of instances of this
*  structure which it stores in a node's "handle".  The handle is passed to
*  various functions defined in this module.
*/
typedef struct NB_MOD_SERVER{      // message.server node descriptor
  char           cabalName[32];    // name of cabal
  char           nodeName[32];     // name of node within cabal
  int            cabalNode;        // number of node within cabal
  nbMsgCabal     *msgserver;        // message server peer structure
  unsigned char  trace;            // trace option 
  unsigned char  dump;             // option to dump packets in trace
  unsigned char  echo;             // echo option
  } nbModServer;

/*==================================================================================
*
*  M E T H O D S
*
*  The code above this point is very specific to the goals of this skill module. The
*  code below this point is also specific in some of the details, but the general
*  structure applies to any skill module.  The functions below are "methods" called
*  by NodeBrain.  Their parameters must conform to the NodeBrain Skill Module
*  API.  A module is not required to provide all possible methods, so modules may
*  vary in the set of methods they implement.
*
*=================================================================================*/

/*
*  construct() method
*
*    define <term> node message.server("<cabal>","<node>");
*/
void *serverConstruct(nbCELL context,void *skillHandle,nbCELL arglist,char *text){
  nbModServer *server;
  nbCELL cell=NULL;
  nbSET argSet;
  char *cursor=text,*delim,saveDelim;
  char cabalName[32];
  char nodeName[32];
  int type,trace=0,dump=0,echo=1;
  char *str;

  argSet=nbListOpen(context,arglist);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Cabal name required as first argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"First argument must be string identifying cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(cabalName)-1){
    nbLogMsg(context,0,'E',"First argument has cabal name \"%s\" too long for buffer - limit is %d characters",str,sizeof(cabalName)-1);
    return(NULL);
    }
  strcpy(cabalName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell==NULL){
    nbLogMsg(context,0,'E',"Node name required as second argument");
    return(NULL);
    }
  type=nbCellGetType(context,cell);
  if(type!=NB_TYPE_STRING){
    nbLogMsg(context,0,'E',"Second argument must be string identifying node within cabal");
    return(NULL);
    }
  str=nbCellGetString(context,cell);
  if(strlen(str)>sizeof(nodeName)-1){
    nbLogMsg(context,0,'E',"First argument has node name \"%s\" too long for buffer - limit is %d characters",str,sizeof(nodeName)-1);
    return(NULL);
    }
  strcpy(nodeName,str);  // size checked
  nbCellDrop(context,cell);
  cell=nbListGetCellValue(context,&argSet);
  if(cell!=NULL){
    nbLogMsg(context,0,'E',"The message.server skill only accepts two arguments.");
    return(NULL);
    }
  while(*cursor==' ') cursor++;
  while(*cursor!=';' && *cursor!=0){
    delim=strchr(cursor,' ');
    if(delim==NULL) delim=strchr(cursor,',');
    if(delim==NULL) delim=strchr(cursor,';');
    if(delim!=NULL){
      saveDelim=*delim;
      *delim=0;
      }
    if(strcmp(cursor,"trace")==0){trace=1;}
    else if(strcmp(cursor,"dump")==0){trace=1;dump=1;}
    else if(strcmp(cursor,"silent")==0) echo=0; 
    if(delim!=NULL){
      *delim=saveDelim;
      cursor=delim;
      while(*cursor==' ' || *cursor==',') cursor++;
      }
    else cursor=strchr(cursor,0);
    }
  server=malloc(sizeof(nbModServer));
  memset(server,0,sizeof(nbModServer));
  strcpy(server->cabalName,cabalName);
  strcpy(server->nodeName,nodeName);
  server->trace=trace;
  server->dump=dump;
  server->echo=echo;
  nbLogMsg(context,0,'I',"calling nbListenerEnableOnDaemon");
  nbListenerEnableOnDaemon(context);  // sign up to enable when we daemonize
  return(server);
  }

/*
*  enable() method
*
*    enable <node>
*/
int serverEnable(nbCELL context,void *skillHandle,nbModServer *server){
  if(!server->msgserver) server->msgserver=nbMsgCabalServer(context,server->cabalName,server->nodeName);
  if(!server->msgserver){
    nbLogMsg(context,0,'E',"Unable to instantiate message peer server for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
    return(1);
    }
  nbMsgCabalEnable(context,server->msgserver);
  nbLogMsg(context,0,'I',"Enabled for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
  return(0);
  }

/*
*  disable method
* 
*    disable <node>
*/
int serverDisable(nbCELL context,void *skillHandle,nbModServer *server){
  nbMsgCabalDisable(context,server->msgserver);
  return(0);
  }

/*
*  command() method
*
*    <node>[(<args>)][:<text>]
*/
int *serverCommand(nbCELL context,void *skillHandle,nbModServer *server,nbCELL arglist,char *text){
  if(server->trace){
    nbLogMsg(context,0,'T',"nb_message:serverCommand() text=[%s]\n",text);
    }
  //nbCmd(context,text,1);
  return(0);
  }

/*
*  destroy() method
*
*    undefine <node>
*/
int serverDestroy(nbCELL context,void *skillHandle,nbModServer *server){
  nbLogMsg(context,0,'T',"serverDestroy called");
  nbMsgCabalFree(context,server->msgserver);
  nbFree(server,sizeof(nbModServer));
  return(0);
  }

#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *serverBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,serverConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,serverDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,serverEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,serverCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,serverDestroy);
  return(NULL);
  }

//===============================================
// Peer
//===============================================
// This skill binds to the server skill mostly
// with the message handler being almost identical
// the client skill.  By giving the client and server
// skills the same data structure, all three skills
// can be combined further.

/*
*  Client message handler
*/
int peerMessageHandler(nbCELL context,void *handle,nbMsgRec *msgrec){
  nbModServer *server=handle;
  char *cmd;
  int cmdlen;

  if(server->trace) nbLogMsg(context,0,'T',"clientMessageHandler: called");
  cmd=nbMsgData(context,msgrec,&cmdlen);
  nbCmd(context,cmd,1);
  return(0);
  }

/*
*  enable() method
*
*    enable <node>
*/
int peerEnable(nbCELL context,void *skillHandle,nbModServer *server){
  if(!server->msgserver) server->msgserver=nbMsgCabalOpen(context,NB_MSG_CABAL_MODE_PEER,server->cabalName,server->nodeName,NULL,NULL,peerMessageHandler);
  if(!server->msgserver){
    nbLogMsg(context,0,'E',"Unable to instantiate message peer for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
    return(1);
    }
  nbMsgCabalEnable(context,server->msgserver);
  nbLogMsg(context,0,'I',"Enabled for cabal \"%s\" node \"%s\"",server->cabalName,server->nodeName);
  return(0);
  }


#if defined(_WINDOWS)
_declspec (dllexport)
#endif
extern void *peerBind(nbCELL context,void *moduleHandle,nbCELL skill,nbCELL arglist,char *text){
  nbSkillSetMethod(context,skill,NB_NODE_CONSTRUCT,serverConstruct);
  nbSkillSetMethod(context,skill,NB_NODE_DISABLE,serverDisable);
  nbSkillSetMethod(context,skill,NB_NODE_ENABLE,serverEnable);
  nbSkillSetMethod(context,skill,NB_NODE_COMMAND,serverCommand);
  nbSkillSetMethod(context,skill,NB_NODE_DESTROY,serverDestroy);
  return(NULL);
  }
