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
* File:     nbstem.c 
*
* Title:    NodeBrain Stem Cell Routines
*
* Function:
*
*   This file provides routines for initializing and managing a NodeBrain
*   environment as defined by a stem cell.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2005-04-11 Ed Trettevik (split out in 0.6.2)
* 2005-10-11 eat 0.6.4  Switched prompt and log from stdout to stderr.
*            This creates an incompatibility and must be tested to make sure
*            there aren't unexpected consequences.  This enables NodeBrain to
*            operate like a "consultant" sending commands to a parent or down
*            stream process on stdout.
* 2005-11-22 eat 0.6.4  Included call to nbCmdInit() in nbStemInit()
* 2005-12-30 eat 0.6.4  Supporting servant mode (see nb_opt_servant)
* 2006-05-12 eat 0.6.6  Included initialization of servejail, servedir, serveuser options.
* 2007-06-26 eat 0.6.8  Type "expert" changed to "node".
* 2008-03-08 eat 0.7.0  Removed support for skull_socket, serveipaddr, and serveoar
* 2008-11-11 eat 0.7.3  Changed failure exit codes to NB_EXITCODE_FAIL
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
* 2010-06-19 eat 0.8.2  Included support for commands provided by modules
* 2010-10-14 eat 0.8.4  Included initialization of servepid.
* 2010-10-16 eat 0.8.4  Included initialization of servegroup.
* 2012-01-16 dtl 0.8.5  Checker updates
* 2012-06-16 eat 0.8.10 Replaced srand with srandom
* 2012-10-13 eat 0.8.13 Replaced malloc with nbAlloc
* 2013-01-01 eat 0.8.13 Checher updates
* 2013-04-08 eat 0.8.15 Change prefix switch from -"'..." to -">..." to match command
* 2013-04-27 eat 0.8.15 Included option parameter in nbSource calls
* 2014-01-20 eat 0.9.00 glossary back to hash and double link IF rule list
*============================================================================*/
#include <nb/nbi.h>
#include <nb/nbmedulla.h>
  
char *nb_cmd_prefix;
char *nb_cmd_prompt;  

#if defined(WIN32)
  char   myuserdir[MAX_PATH+12];
#else
  char   myuserdir[1024];
#endif

char *nbGetUserDir(void){
  return(myuserdir);
  }

void nbStemInit(NB_Stem *stem){
  nb_cmd_prefix=nbAlloc(NB_CMD_PROMPT_LEN);
  *nb_cmd_prefix=0;
  nb_cmd_prompt=nbAlloc(NB_CMD_PROMPT_LEN);
  strcpy(nb_cmd_prompt,"> ");
  nbObjectInit(stem);
  nbParseInit();
  nbHashInit(stem);
  nbRealInit(stem);
  initString(stem);
  initText(stem);
  nbCellInit(stem);
  nbSynapseInit(stem);
  initMath(stem);
  initRegexp(stem);
  initTerm(stem);
  nbConditionInit(stem);
  nbConditionalInit(stem);
  nbAxonInit(stem);
  nbAssertionInit(stem);
  NbCallInit(stem);              /* must follow initCondition */
  nbNodeInit(stem);
  nbSentenceInit(stem);
  //nbListenerInit(stem);
  //nbBrainInit(stem);
  nbMacroInit(stem);
  nbStreamInit(stem);
  // 2010-06-19 eat - creating a glossary of verbs
  nbVerbInit(stem);
  nbCmdInit(stem);
  }

void nbStartParseArgs(nbCELL context,struct NB_STEM *stem,int argc,char *argv[]){
  int i;

  for(i=1;i<argc;i++){
    if(*argv[i]=='+' && *(argv[i]+1)!='+'){
      outMsg(0,'I',"Argument [%u] %s",i,argv[i]);
      nbCmdSet(context,stem,"set",argv[i]);
      }
    }
  }

/*
*  Parse command line arguments - may come from shebang line
*/
void nbServeParseArgs(nbCELL context,struct NB_STEM *stem,int argc,char *argv[]){
  char *cursor;
  int i;
  char *comma,*equal;
  /* if(argc<2) nb_opt_prompt=1; */ /* default to prompt */
  
  for(i=1;i<argc && nb_flag_stop==0;i++){
    if(*argv[i]=='+') continue;
    outMsg(0,'I',"Argument [%u] %s",i,argv[i]);
    cursor=argv[i];
    switch(*cursor){
      case '-':
        cursor++;
        if(*cursor==0 || *cursor==','){
          nbSource(context,0,argv[i]);
          nb_flag_input=1;
          }
        else if(*cursor=='>' || *cursor=='\''){  // handle command prefix argument
          if(*cursor=='\'') outMsg(0,'W',"The single quote is deprecated. Use > to set a command prefix.");
          cursor++;
          while(*cursor==' ') cursor++;
          if(strlen(cursor)>NB_CMD_PROMPT_LEN-3){
            outMsg(0,'E',"Command prefix too large for buffer - ignoring: %s",cursor);
            }
          else{
            strcpy(nb_cmd_prefix,cursor);
            sprintf(nb_cmd_prompt,"%s> ",nb_cmd_prefix);
            nb_opt_prompt=1;  // turn on prompt option
            }
          }
        else nbCmdSet(context,stem,"set",cursor-1);
        break;
      case '=': nbSource(context,0,argv[i]); nb_flag_input=1; break;
      case ':': nbCmd(context,cursor+1,1); nb_flag_input=1; break;
      default:
        if(NULL!=(equal=strchr(cursor,'='))){
          if(NULL!=(comma=strchr(cursor,',')) && comma<equal) nbSource(context,0,cursor);
          else nbParseArgAssertion(cursor);
          }
        else nbSource(context,0,cursor);
        nb_flag_input=1;
      }
    }
     
  outFlush();
  }

/*
* Print routines to register
*/
void stdPrint(char *buffer){
//  if(ofile==NULL) return;
  fputs(buffer,stderr);
  fflush(stderr);
  }
void logPrint(char *buffer){
  if(lfile==NULL) return;
  fputs(buffer,lfile);
  fflush(lfile);
  }
void logPrintNl(char *buffer){
  if(lfile==NULL) return;
  fputs(buffer,lfile);
  fputs("\n",lfile);
  fflush(lfile);
  }
/*
void clientPrint(char *buffer){
  if(serving) chput(currentSession->channel,buffer,strlen(buffer));
  }
*/


void nbLoadUserProfile(nbCELL context){
  char filename[1024];
  FILE *file;

  snprintf(filename,sizeof(filename),"%s/%s",myuserdir,"user.nb"); //2012-01-16 dtl: used snprintf
  //outMsg(0,'T',"filename:%s\n",filename);
  if((file=fopen(filename,"r"))==NULL){     // 2013-01-01 eat - VID 1650-0.8.13-1 FP
    snprintf(filename,sizeof(filename),"%s/%s",myuserdir,"profile.nb");  // 2013-01-01 eat VID 5379-0.8.13-1
    if((file=fopen(filename,"r"))==NULL){
      snprintf(filename,sizeof(filename),"%s/%s",myuserdir,"private.nb");
      if((file=fopen(filename,"r"))==NULL) return;
      outMsg(0,'W',"Using 'private.nb' as profile because 'user.nb' was not found."); 
      }
    else outMsg(0,'W',"Using 'profile.nb' as profile because 'user.nb' was not found.");
    }
  while(fgets(bufin,NB_BUFSIZE,file)!=NULL){
    nbCmd(context,bufin,NB_CMDOPT_HUSH);
    }
  fclose(file);  
  outMsg(0,'I',"User profile %s loaded.",filename);
  }

// 2008-05-25 eat 0.7.0 - check for caboodle profile
void nbLoadCaboodleProfile(nbCELL context){
  FILE *file; 
  if((file=fopen(".nb/caboodle.nb","r"))==NULL) return;
  while(fgets(bufin,NB_BUFSIZE,file)!=NULL){
    nbCmd(context,bufin,NB_CMDOPT_HUSH);
    }
  fclose(file);
  outMsg(0,'I',"Caboodle profile .nb/caboodle.nb loaded.");
  }

void nbBind(nbCELL context);

// this function would be used if we passed standard input
// unfortunately Windows can only select() on sockets, so we should
// stay away for standard input unless we are going to read to end of file
// and then terminate
void plainTextFileCmdListener(nbCELL context,int file,void *session){
  char *buffer=(char *)session;
  outMsg(0,'T',"plainTextFileCmdListener called");
  nbGets(file,buffer,NB_BUFSIZE);
  outMsg(0,'T',"plainTextFileCmdListener back from nbGets");
  if(*buffer!=0) nbCmd(context,buffer,1);
  outMsg(0,'T',"plainTextFileCmdListener back from nbCmd");
  }

int medullaScheduler(void *session){
  return(nbClockAlert());
  }

// Medulla process termination handler

int medullaProcessHandler(nbPROCESS process,int pid,char *exittype,int exitcode){
  if(process->status&NB_MEDULLA_PROCESS_STATUS_BLOCKING)
    outPut("[%d] %s(%d)\n",nb_mode_check ? 0:pid,exittype,exitcode);
  else outMsg(0,'I',"[%d] %s(%d)",nb_mode_check ? 0:pid,exittype,exitcode);
  return(0);
  }

// Signal Handler - SIGCHLD is handled by Medulla
void nbSigHandler(int sig){
  outPut("\n");
  switch(sig){
    case SIGTERM:
      outMsg(0,'W',"SIGTERM - stopping");
      outFlush();
      if(agent) nbCmd((nbCELL)rootGloss,"stop",1);
      else exit(NB_EXITCODE_FAIL);
      break;
    case SIGINT:
      outMsg(0,'W',"SIGINT - stopping");
      outFlush();
      if(agent) nbCmd((nbCELL)rootGloss,"stop",1);
      else exit(NB_EXITCODE_FAIL);
      break;
#if !defined(WIN32)
    case SIGHUP:
      outMsg(0,'W',"SIGHUP - stopping");
      outFlush();
      if(agent) nbCmd((nbCELL)rootGloss,"stop",1);
      else exit(NB_EXITCODE_FAIL);
      break;
    default:
      outMsg(0,'W',"Signal %d ignored\n",sig);
      outFlush();
#endif
    }
  }

/***************************************************************
*  Startup routine
*/
nbCELL nbStart(int argc,char *argv[]){
  int  i;             /* temp counter */
#if defined(WIN32)
  LPTSTR myusernamePtr;    /* pointer to system information string */ 
  DWORD  myusernameLen;    /* size of computer or user name */
  static struct WSAData myWSAData;
#else
  struct passwd *pwd;
#endif
  //nbCELL privateContext=NULL;
  char *cursor;
  char mypid[20];
  NB_Stem *stem;
 
  setlocale(LC_CTYPE,"");          // 2012-12-25 eat - let's figure out our codeset
  nb_charset=nl_langinfo(CODESET);

  nbHeap();  // allocate the object heap so we can call nbAlloc
  bufin=(char *)nbAlloc(NB_BUFSIZE);
/*
*  Handle informational options that must stand alone
*  and that don't require any initialization
*/
  if(argc==2){
    if(strcmp(argv[1],"--about")==0){
      printAbout();
      return(NULL);
      }
    if(strcmp(argv[1],"--help")==0 || strcmp(argv[1],"-h")==0){
      printHelp();
      return(NULL);
      }
    if(strcmp(argv[1],"--version")==0 || strcmp(argv[1],"-v")==0){
      printVersion();
      return(NULL);
      }
    } 

/*
*  Create the stem cell
*/
  if((stem=(NB_Stem *)nbAlloc(sizeof(NB_Stem)))==NULL) return(NULL); /* pass to all init(init) functions who pass to all nbObjectType() calls */
  memset(stem,0,sizeof(NB_Stem));
  //stem->parentChannel=NULL;
  stem->exitcode=0; 
  nbMedullaOpen(stem,medullaScheduler,medullaProcessHandler);

#if defined(WIN32)
  if(nb_Service) nb_medulla->service=1;  // let Medulla know when we are running as a windows service
#endif

  *servejail=0;  // chroot directory
  *servedir=0;   // chdir directory
  *servepid=0;   // pid file 
  *serveuser=0;  // su user
  *servegroup=0; // sg group
  nb_symBuf1=nbAlloc(NB_BUFSIZE);
  nb_symBuf2=nbAlloc(NB_BUFSIZE);
  *lname=0;
  //*quedir=0;
  jfile=NULL;
//  if(nb_Service==0 && (ofile=fdopen(2,"w"))==NULL){   // my standard print file is stderr
//    fprintf(stderr,"NB000E fdopen failed. errno=%d",errno);
//    exit(NB_EXITCODE_FAIL);
//    }
  
  if(outInit()){     /* we'll skip this when running as a windows service */
    lfile=NULL;
    outStream(0,&stdPrint);
    outStream(1,&logPrint);
    //outStream(2,&clientPrint);
    }

/*
*  Get some variables
*/

#if defined(WIN32)
  myusernamePtr = myusername;
  myusernameLen = sizeof(myusername);
  if(!GetUserName(myusernamePtr,&myusernameLen)) strcpy(myusername,"unknown");
//  nbpTimer=CreateWaitableTimer(NULL,TRUE,NULL);
//  if(!nbpTimer){
//    outMsg(0,'L',"CreateWaitableTimer failed (%d)", GetLastError());
//    exit(NB_EXITCODE_FAIL);
//    }

  if(WSAStartup(MAKEWORD(1,1),&myWSAData)!=0){
    fprintf(stderr,"NB000E Unable to startup winsock.\n");
    exit(NB_EXITCODE_FAIL);
    }
#else
  pwd=getpwuid(getuid());
  if(pwd==NULL) strcpy(myusername,"???");
  else if(strlen(pwd->pw_name)>=sizeof(myusername)){
    fprintf(stderr,"NB000E Username %s too large",pwd->pw_name);
    exit(NB_EXITCODE_FAIL);
    }
  else strcpy(myusername,pwd->pw_name); 
#endif
  if(gethostname(nb_hostname,sizeof(nb_hostname))) strcpy(nb_hostname,"unknown");
  if(strlen(argv[0])>=sizeof(mypath)){
    fprintf(stderr,"NB000E First argument larger than path buffer.\n");
    exit(NB_EXITCODE_FAIL);
    }
  strcpy(mypath,argv[0]);
  cursor=mypath+strlen(mypath)-1;
  while(cursor>mypath && *cursor!='/' && *cursor!='\\') cursor--;
  if(*cursor=='/' || *cursor=='\\') cursor++;
  myname=cursor;
  mycommand=(char *)nbAlloc(NB_BUFSIZE);
  *mycommand=0;
  for(i=0;i<argc;i++) {
    strcat(mycommand,argv[i]);
    strcat(mycommand," ");
    }

  /* initialize nodebrain logic structures */
  /* first set the trace shim */  
  if(argc>1 && strcmp(argv[1],"++shim")==0){
    fprintf(stderr,"NB000T Setting shim option\n");
    nb_opt_shim=1;
    }
  for(i=1;i<argc;i++){
    if(strncmp(argv[i],"++",2)==0){
      if(strcmp(argv[i],"++shim")==0)  nb_opt_shim=1;
      else if(strcmp(argv[i],"++hush")==0) nb_opt_hush=1;
      else if(strcmp(argv[i],"++stats")==0) nb_opt_stats=1;
      else if(strcmp(argv[i],"++bnr")==0) nb_opt_boolnotrel=1;
      else if(strcmp(argv[i],"++test")==0) nb_opt_test=1;
      else if(strcmp(argv[i],"++safe")==0) nb_opt_safe=1;
      else fprintf(stderr,"Startup setting %s not recognized.\n",argv[i]);
      }
    }

  showHeading();
  srandom(time(NULL));    /* seed the random number generator */

  nbStemInit(stem); 

  /* initialize lists and hash tables*/

  actList=NULL;
  ashList=NULL;
  nbClockInit(stem);
  nbTimeInit(stem);
  nbRuleInit(stem);
    
  nbListInit(stem);        /* initialize list hash */
  nbSchedInit(stem);       /* initialize schedule hash */
  nbTranslatorInit(stem);
  
  /* initialize root context, verbs, and types */
 
  // 2014-11-01 eat - changed term from "root" to "_" so nbNodeGetName will return the correct value
  rootGloss=nbTermNew(NULL,"_",nbNodeNew(),0);
  
  nbModuleInit(stem);

  initIdentity(stem);   
  defaultIdentity=nbIdentityNew("default",AUTH_OWNER);  // default identity
  clientIdentity=defaultIdentity; 
  ((NB_Node *)rootGloss->def)->owner=clientIdentity;
  ((NB_Node *)rootGloss->def)->context=rootGloss;
  nbTermNew(identityC,"default",defaultIdentity,0);
  
  //locGloss=nbTermNew(rootGloss,"@",nbNodeNew(),0);
  //((NB_Node *)locGloss->def)->context=locGloss;
  symGloss=nbTermNew(NULL,"%",nbNodeNew(),0);
  addrContext=rootGloss;
  symContext=symGloss;
  /* 
  *  Get startup options  
  */
  nbStartParseArgs((nbCELL)rootGloss,stem,argc,argv);

  /*
  *  Define some handy terms 
  */
  sprintf(mypid,"%u",getpid());
  nbTermNew(symGloss,"_pid",useString(mypid),0);
  nbTermNew(symGloss,"_username",useString(myusername),0);
  nbTermNew(symGloss,"_hostname",useString(nb_hostname),0);

  /*
  *  Define types in separate dictionary for now
  */
  nb_TypeGloss=nbTermNew(NULL,"type",nbNodeNew(),0);
  nbTermNew(nb_TypeGloss,"cell",useString("cell"),0);
  //nbTermNew(nb_TypeGloss,"file",useString("file"),0);
  nbTermNew(nb_TypeGloss,"on",useString("on"),0);
  nbTermNew(nb_TypeGloss,"when",useString("when"),0);
  nbTermNew(nb_TypeGloss,"if",useString("if"),0);
  nbTermNew(nb_TypeGloss,"nerve",useString("nerve"),0);
  //nbTermNew(nb_TypeGloss,"translator",useString("translator"),0);
  nbTermNew(nb_TypeGloss,"node",useString("node"),0);
  nbTermNew(nb_TypeGloss,"macro",useString("macro"),0);
  nbTermNew(nb_TypeGloss,"text",useString("text"),0);
  //nbTermNew(nb_TypeGloss,"verb",useString("verb"),0);

  
  nbBind((nbCELL)addrContext);  /* bind imbedded skills */

/*
*  Locate user directory
*/
#if defined(WIN32)
  if(nb_Service) sprintf(myuserdir,"%s/Service/%s",windowsPath,serviceName);
  else if(SUCCEEDED(SHGetFolderPath(NULL,CSIDL_APPDATA,NULL,0,myuserdir)))
      strcat(myuserdir,"/NodeBrain");
  else strcpy(myuserdir,windowsPath);
#else
  if((pwd=getpwuid(getuid()))==NULL){
    outMsg(0,'E',"Unable to get account info for user id=%d",getuid());
    return(NULL);
    }
  if(strlen(pwd->pw_dir)>=sizeof(myuserdir)-4){
    outMsg(0,'E',"Home directory name must not be greater than %d characters.",sizeof(myuserdir)-4);
    return(NULL);
    }
  strcpy(myuserdir,pwd->pw_dir);
  strcat(myuserdir,"/.nb");
#endif
  /*
  *  get private definitions
  */
  if(nb_opt_user) nbLoadUserProfile((nbCELL)rootGloss);
  nbLoadCaboodleProfile((nbCELL)rootGloss);

  // 2005-12-12 eat 0.6.4  modified signal handling
  // signal(SIGCHLD,childSigHandler);  // not needed because we are using medulla

  signal(SIGTERM,nbSigHandler);
  signal(SIGINT,nbSigHandler);
#if !defined(WIN32)
  signal(SIGHUP,nbSigHandler);
  signal(SIGPIPE,SIG_IGN);          // this is required to avoid terminating when a child pipe breaks
#endif

  outFlush();
  return((nbCELL)rootGloss);
  }
  
int stdReader(nbPROCESS process,int pid,void *session,char *msg){
  nbCELL context=(nbCELL)session;
  nbCmd(context,msg,1);
  return(0);
  }

int stdWriter(nbPROCESS process,int pid,void *session){
  return(0);
  }

int nbServe(nbCELL context,int argc,char *argv[]){
  NB_Stem *stem=context->object.type->stem;

  // First process all parameters
  nbServeParseArgs(context,stem,argc,argv);  /* parse arguments */
  outFlush();

  if(!nb_opt_servant && (nb_opt_prompt || !nb_flag_input)) nbSource(context,0,"-");
  if(nb_opt_query){
    nbCmdQuery(context,stem,"query","");
    nbRuleReact(); /* let rules fire */  
    }

  if(nb_opt_servant){
    outMsg(0,'T',"Servant mode selected");
    outPut("---------- -------- --------------------------------------------\n");
    nbMedullaProcessEnable(nbMedullaProcessFind(0),context,stdWriter,stdReader);
    nbListenerStart(context);
    }
  else{
    //if(stem->parentChannel){  // running as a socket child
    //  outMsg(0,'T',"registering parent socket command listener");
    //  nbListenerAdd((nbCELL)rootGloss,stem->parentChannel->socket,stem->parentChannel,socketCmdListener);
    //  outMsg(0,'I',"Parent channel established\n");
    //  nbListenerStart(context);  
    //  }
    //else if(nb_opt_daemon==1){
    if(nb_opt_daemon==1){
      daemonize();
      nbListenerStart(context);
      }
    }
  outFlush();
  return(stem->exitcode);
  }

void nbMedullaExit(void);

int nbStop(nbCELL context){
  if(nb_opt_stats) nbHashStats(); // display hash metrics

  NB_Stem *stem=context->object.type->stem;
  nbMedullaExit();             // clean up processes
#if !defined(WIN32)
  nbMedullaProcessHandler(1);  // wait for children to stop
#endif
  outMsg(0,'I',"NodeBrain %s[%u] terminating - exit code=%d",myname,getpid(),stem->exitcode);
  outFlush();
  fflush(stderr);
  /* need to include code here to clean up stem and all subordinate memory */
  /* this is needed for multiple calls to nbStart() */ 
#if defined(WIN32)
  if(nb_Service){
    nbwServiceStopped();
    nb_ServiceStopped=1; // let the handler know - this can go into nbServiceStopped()
    }
#endif
  return(stem->exitcode);
  }
