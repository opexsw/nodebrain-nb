/*
* Copyright (C) 1998-2010 The Boeing Company
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*=============================================================================
* Program:  NodeBrain
*
* File:     nbcmd.c 
*
* Title:    NodeBrain Command Interpreter
*
* Function:
*
*   This file provides a set of functions for processing commands.  It is
*   separate from the main routine to simplify integration into other
*   programs.
*
* Note:
*
*   The goal is to migrate toward an official API.  Anyone taking advantage
*   of an evolving "API like" structure does so at the risk of disruption
*   in future releases.  This will be true even for the initial published
*   API.
*
*=============================================================================
* Change History:
*
* Date       Name/Change
* ---------- -----------------------------------------------------------------
* 2003/02/13 Ed Trettevik (version 0.5.0)            
*
* 2003/03/03 eat 0.5.1  Conditional compile for Max OS X [ see defined(MACOS) ]
*
* 2003/03/03 eat 0.5.1  Changed "daemon" variable to "agent"
*            "daemon" was already defined in a standard header on Mac OS X
*
* 2003/03/08 eat 0.5.1  Changed more references to "daemon" to "agent".
*            These were not flagged by the Mac compiler because they were
*            compatible with the definition in the standard header.
*
* 2003/03/08 eat 0.5.1  Changed some option names to match document.
*            e.g. symbolicTrace changed to traceSymbolic
*            Old values where retained for compatibility with existing code.
*
* 2003/03/16 eat 0.5.2  Broke out of nbmain.c to begin API'ish structure.
* 2003/04/22 eat 0.5.4  Included:  declare <term> module <filename>;
* 2003/11/04 eat 0.5.5  Included:  declare <calendar> calendar <timeExpr>;
* 2004/04/06 eat 0.6.0  changed version note
* 2004/08/26 eat 0.6.1  changed version note
* 2004/08/28 eat 0.6.1  included dropObject after ->compute
* 2004/10/10 eat 0.6.1  included support for check scripts
* 2004/10/22 eat 0.6.2  allowing spaces in source file names for windows
* 2005/04/06 eat 0.6.2  changed nbCmd() first argument type to NB_Cell *
* 2005/04/08 eat 0.6.2  moved nbParseStdin() here from nbapi.c
* 2005/04/09 eat 0.6.2  removed references to cache routines
*            We replaced nbcache.c with nb_mod_cache.c and needed to uncouple
*            from code in the module. This revealed an unfortunate situation
*            where an implicit reference to the command context in the first
*            assertion was assumed to be a cache reference.  Furthermore, in
*            this case all assertions where completed before the context was
*            alerted.  This deviated from the way other node assertions work,
*            where the context is alerted from within the node's "assert"
*            method.  To maintain compatibility with existing rules and make
*            all nodes work the same, we have temporarily applied the "cache"
*            approach to all nodes.  We no longer assume that an implicit
*            node referenece in the first position is a cache, but we do
*            preserve the order of assertions used in the old cache logic. So
*            When an implicit reference starts the list, we do the assertion
*            starting with the second one and then do the first one.
*
*               assert john(a,b,c),x=2,y=3;      # explicit node reference
*               john. assert (a,b,c),x=2,y=3;    # implicit node reference
*
*            In the second example above, x and y are asserted before john(a,b,c)
*            is asserted.  In the first example, all assertions are done left to
*            right.
*
*            In an effort to make the syntax more consistent, implicit node
*            references are now supported anywhere within the assertion list.
*
*               john. assert x=2,(a,b,c),y=3,(1,2,3);
*           
*            This makes it possible to use an alternate syntax that produces
*            the same result without relying on the weird compatibility with
*            old rules.
*
*               john. assert (a,b,c),x=2,y=3;    # weird compatibiity order applies
*               john. assert x=2,y=3,(a,b,c);    # same result
*
*            After exiting rules are modified, we can remove the special handling
*            of order when the first assertion is an implicit node reference.
*            
*            One additional enhancement under consideration is illustrated below.
*            This would allow for asserting context terms before calling the
*            node's "assert" method.
*
*               assert john(a,b,c:x=2,y=3),john(1,2,3:x=0,y=4);   # preferred syntax
*               assert john{x=2,y=3}(a,b,c),john{x=0,y=4}(1,2,3); # another possible syntax
*
*            The second syntax is less desirable because we may have other uses
*            for the term{...} syntax.
*            
*            Note: This still will not completely get us out of the water.  We
*            still don't handle the alerts from within a node cleanly with
*            respect to our "scheduled" approach to reacting to a full set of
*            assertions.  For now, it is best not to mix assertions of alerting
*            nodes with unrelated assertions that you want to be processed as
*            a logical group.  The alerting node will split the processing of
*            assertions before it and after it.  This is a serious problem to
*            resolve, not just work around.
*
* 2005/04/30 eat 0.6.2  resolved alerting node problem I'm babbling about above 
*            Now the assertions all take pace in left to right order.  Anything
*            a node wants to do as the result of an assertion gets scheduled
*            via the nbAction() API function.  These actions take place in the
*            order they are scheduled within a common priority.  These actions
*            will all take place in response to the entire assertion.
*
* 2005/05/03 eat 0.6.2  changed SOURCE command again to eliminate incompatibility
*            On 4/10 spaces where allowed in file names, but this required the
*            use of a comma after the file name.  A comma where a space was
*            allowed previously.  To eliminate this incompatibility with existing
*            rule files, spaces are now allowed in a file name only when enclosed
*            by quotes.  This makes for messy use in parameters, but provides
*            compatibility.
*
*               source "c:/Program Files/NodeBrain/some file" a=1,b=2;
*
*            Escape the quotes in parameters.
*
*               nb "\"c:/Program Files/NodeBrain/some file\" a=1,b=2"
*
*            We still have the option of deprecating the use of a space before   
*            the assertion, eventually requiring it and making the use of quotes
*            optional.  No decision on that yet.  We can leave it this way or
*            cracefully change it.
*
* 2005/05/12 eat 0.6.3  Went back to allowing unquoted names with spaces on Windows
*            The file extension is used to start a NodeBrain script on Windows,
*            we have no opportunity to include quotes, so we have to support
*            unquoted names with spaces on Windows.  This means we must also require
*            a comma to let us know where there is an assertion.  We can't key
*            on an "=" because our assertion syntax does not require it.  In the
*            following example a is set to one.
*
*              nb "c:/Program Files/NodeBrain/some file,a"
*
*            On Unix/Linux we still allow a space as a file name delimiter for
*            compatibility with existing rules.  As described above, we can
*            deprecate the space in a future release and then obselete it later
*            to get Windows and Unix/Linux versions to be consistent in requiring
*            the comma.
*
* 2005-08-20 eat 0.6.3  Fixed problem with portray command from standard input
*            nbCmdSid() was restorying clientIdentity after change.
*
* 2005-10-11 eat 0.6.4  Included ":" command for sending data to stdout
*
* 2005-11-21 eat 0.6.4  Command table replaces command glossary
*            In the initial prototype of NodeBrain we anticipated extensions to
*            the command language via verb definition or declaration.  We 
*            eventually settled on the use of skill modules.  This means
*            it is no longer desirable to use a term glossary for looking up
*            command verbs.  Since the command table is static, we now represent
*            it with a static table.  Entries in the command table are linked
*            alphabetically and linked for binary search. Each entry contains a
*            pointer to the parsing routine and an authority mask.
*
* 2006-01-12 eat 0.6.5  Added show +p (processes) option
* 2006-05-09 eat 0.6.6  Modified TRANSLATE command to stop fussing the symbolic context
*                       It is handled when the translator is contructed and
*                       the scope is the set of command generated by the translator
*                       which spans all files tranlated.  Previously we created
*                       a new symbolic context for each TRANSLATE command, so the
*                       scope was limited to a single file.          
* 2006-05-12 eat 0.6.6  Included jail, dir, and user options.
* 2007-06-26 eat 0.6.8  Changed DEFINE object type of "expert" to "node".
* 2007-06-26 eat 0.6.8  Included warning messages for deprecated commands.
* 2008-03-06 eat 0.7.0  Removed deprecated commands
*                       o  portray
*                       o  pipe
*                       o  copy
*                       o  declare brain
*                       o  declare file
*                       o  declare listener
*                       o  remote commands: >, \, /
* 2008-03-08 eat 0.7.0  Removed socket, ipaddr, and oar options
* 2008-03-08 eat 0.7.0  Removed fileTrace option - see nb_mod_audit.c
* 2008-04-26 eat 0.7.0  Made symbolic substitution by explicit request only
*                       This change was don't to improve performance.  Most
*                       assertions and alerts do not use symbolic substitution
*                       so we had an unnecessary pass looking for symbolic
*                       substitution.  It is now only performed when explicitly
*                       requested with the "$ " command prefix.
* 2009-02-20 eat 0.7.5  Included test for NULL from readline and changed nbGetCmdInteractive return codes
* 2009-12-28 eat 0.7.7  Include traceMessage and notraceMessage options
* 2010-02-25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010-02-26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.5.0)
*==============================================================================
*/
#include "nbi.h"

#if !defined(WIN32)
#include <readline/readline.h>
#include <readline/history.h>
#endif

// Get a command from interactive user.
//
// A command starting with "'" is interpreted as a command prefix.  The
// prefix is used for subsequent prompts and commands.
//
// Returns:
//   1 - have command (may be empty)
//   0 - eof

int nbGetCmdInteractive(char *cmd){
  static char *lastInput="";
  static char *userInput="";
  char *cursor;
#if defined(WIN32)
  char buffer[NB_BUFSIZE];
#endif

  outPut("\n");
  outFlush();
#if !defined(WIN32)
  if(*lastInput) free(lastInput); // free up previous last command
#endif
  lastInput=userInput;            // get current last command
  userInput="'";
  while(userInput && *userInput=='\''){;
#if defined(WIN32)
    printf("%s",nb_cmd_prompt);
    userInput=fgets(buffer,sizeof(buffer),stdin);
    //userInput=buffer;
    if(!userInput) return(0);
    cursor=strchr(buffer,10);
    if(cursor) *cursor=0;
    cursor=strchr(buffer,13);
    if(cursor) *cursor=0;
#else
    userInput=readline(nb_cmd_prompt);  // get a line from the user
    if(!userInput) return(0);
#endif
    if(*userInput=='\''){
      cursor=userInput+1;
      while(*cursor==' ') cursor++;
      if(strlen(cursor)>NB_CMD_PROMPT_LEN-3){
        outMsg(0,'E',"Command prefix too large for buffer - ignoring: %s",cursor);
        }
      else{
        strcpy(nb_cmd_prefix,cursor);
        sprintf(nb_cmd_prompt,"%s> ",nb_cmd_prefix);
        }
#if !defined(WIN32)
      free(userInput);
#endif
      userInput="'";
      }
    }
  if(*userInput){
#if !defined(WIN32)
    if(strcmp(lastInput,userInput)) add_history(userInput);  // add to history
#endif
    if(!*nb_cmd_prefix) strcpy(cmd,userInput);
    else sprintf(cmd,"%s %s",nb_cmd_prefix,userInput);
    return(1);
    }
  *cmd=0;
  return(1);
  }

/*
*  Print version for --version option
*/
void printVersion(void){
  printf("nb %s\n\n",PACKAGE_VERSION);
  printf("N o d e B r a i n\n");
  printf("Copyright (C) 1998-2009 The Boeing Company\n");
  printf("GNU General Public License\n\n");
  }

/*
*  Print help for --help option
*/
void printHelp(void){
  printVersion();
  printf("This is free software that you may copy and redistribute under\n");
  printf("the terms of the GPL license.\n");
  printf("----------------------------------------------------------------\n\n");
  printf("Usage: nb [-options] [file]\n");
  printf("\nSwitch Options:   May specify multiple times.\n\n");
  printf("  -b --bail       Bail out on first command error (exit 254)\n");
  printf("  -B --noBail     off\n");
  printf("  -d --daemon     Daemonize after loading rules.\n");
  printf("  -D --noDaemon   off\n");
  printf("  -p --prompt     Prompt user for commands after loading rules\n");
  printf("  -P --noPrompt   off\n");
  printf("  -s --servant    Run as child in forground after loading rules.\n");
  printf("  -S --noServant  off\n");
  printf("  -q --query      Query to resolve unknowns after loading rules.\n");
  printf("  -Q --noQuery    off\n");
  printf("\nFiles:\n\n");
  printf("     -            Read from stdin with prompt to stdout.\n");
  printf("     =            Read from stdin - typically piped input.\n");
  printf("     filename     Rule file.\n");
  printf("\nSolo Options:\n\n");
  printf("     --about      Display short description of NodeBrain.\n");
  printf("     --help       Display this page.\n");
  printf("     --version    Display program version.\n\n");
  printf("----------------------------------------------------------------\n");
  printf("For additional supported arguments refer to the local Unix/Linux\n");
  printf("manual page, 'man nb', or Windows help file.  Documentation is\n");
  printf("also available on the web at www.nodebrain.org.\n");
  }

char *aboutText=
    "NodeBrain is an open source rule engine for state and event\n"
    "monitoring applications.  It is an interpreter of a small\n"
    "declarative rule language extended by node modules (plug-ins)\n"
    "conforming to a C API, and servant programs written in any\n"
    "language.  Node modules and servants support state and event\n"
    "collection, knowledge representation, and communication with\n"
    "peers, consoles, and other applications.  Interactive, batch,\n"
    "and agent operating modes are supported.\n\n"
    "See http://www.nodebrain.org for more information.\n\n"
    "Author: Ed Trettevik <eat@nodebrain.org>\n\n";

void printAbout(void){
  printVersion();
  printf(aboutText);
  }

void showVersion(void){
  outPut("\nN o d e B r a i n   %s (Dunce) %s\n\n",PACKAGE_VERSION,NB_RELEASE_DATE);
  outPut("Compiled %s %s %s\n\n",__DATE__,__TIME__,NB_COMPILE_PLATFORM);
  }

void showCopyright(void){
  showVersion();
  outPut("Copyright (C) 1998-2009 The Boeing Company\n");
  outPut("GNU General Public License\n");
  outPut("----------------------------------------------------------------\n\n");
  }

void showHeading(void){
  showCopyright();
  outPut("%s\n\n",mycommand);
  outPut("Date       Time     Message\n");
  outPut("---------- -------- --------------------------------------------\n");
  outMsg(0,'I',"NodeBrain %s[%d] %s@%s",myname,getpid(),myusername,nb_hostname);
  if(agent) outMsg(0,'I',"Agent log is %s",outLogName(NULL));
  outFlush(); 
  }  

void showAbout(void){
  showCopyright();

  outPut(
    "NodeBrain is an open source rule engine for state and event\n"
    "monitoring applications.  It is an interpreter of a small\n"
    "declarative rule language extended by node modules (plug-ins)\n"
    "conforming to a C API, and servant programs written in any\n"
    "language.  Node modules and servants support state and event\n"
    "collection, knowledge representation, and communication with\n"
    "peers, consoles, and other applications.  Interactive, batch,\n"
    "and agent operating modes are supported.\n\n"
    "See http://www.nodebrain.org for more information.\n\n"
    "Author: Ed Trettevik <eat@nodebrain.org>\n"
    "----------------------------------------------------------------\n\n");
  }

void showSet(){
  //outPut("ipaddr:\t%s\n",serveipaddr);
  //outPut("oar:\t%s\n",serveoar);
  //outPut("nbp:\t%u\n",nbp);
  outPut("log:\t%s\n",outLogName(NULL));
  outPut("out:\t%s\n",outDirName(NULL));
  //outPut("que:\t%s\n",quedir);
  outPut("jail:\t%s\n",servejail);
  outPut("dir:\t%s\n",servedir);
  outPut("user:\t%s\n",serveuser);
  }

// Show process list
//   We do this here instead of in nbmedulla.c because we want to use
//   NodeBrain skull output and don't want the Medulla to know about the
//   skull layer.
void showProcessList(){
  nbPROCESS process;
  for(process=nb_process->next;process!=nb_process;process=process->next){
    outPut("%x %x %5d %s %s\n",process->status,process->options,process->pid,process->prefix,process->cmd);
    }
  outFlush();
  }

/***************************************************************
*  Interpret Statements
*  
*/
void nbCmdShow(nbCELL context,char *verb,char * cursor){
  char symid,optid,ident[1024],*cursave;
  NB_Term *term;
  NB_Cell *ref,*def,*val;
  int len;

  cursave=cursor;
  while(*cursor==' ') cursor++;
  if(*cursor==0 || strchr("+-=/%*",*cursor)==NULL){
    symid=nbParseSymbol(ident,&cursor);
    if(symid=='t' || symid=='('){
      if(symid=='('){ /* compute a cell expression */
        if(NULL==(def=(NB_Cell *)nbParseCell((NB_Term *)context,&cursor,0))) return;
        grabObject(def);
        ref=def;
        val=(NB_Cell *)nbCellCompute_(def);
        cursor++;
        }
      else if(symid=='t'){
        term=nbTermFind((NB_Term *)context,ident);
        if(term==NULL){
          if(*ident==0){ // symid='t' but ident=="" --- must have been "show ."
            term=(NB_Term *)context;  // reference current context
            if(*cursor=='.') cursor++;
            }
          else{
            outMsg(0,'E',"Term \"%s\" not defined.",ident);
            return;
            }
          }
        ref=(NB_Cell *)term;
        val=grabObject((NB_Cell *)term->cell.object.value);
        def=grabObject((NB_Cell *)term->def);
        }
      optid=nbParseSymbol(ident,&cursor);
      len=strlen(ident);
      if(optid==';'){
        if(symid=='t')  nbTermShowReport(term);
        else{
          outPut("() = ");
          printObject((NB_Object *)val);
          outPut(" == ");
          printObject((NB_Object *)def);
          outPut("\n");
          }
        }
      else if(strncmp(ident,"subscribers",len)==0) nbCellShowSub(ref);
      else if(strncmp(ident,"impact",len)==0) nbCellShowImpact(ref);
      else if(strncmp(ident,"value",len)==0) printObject((NB_Object *)val);
      else if(strncmp(ident,"definition",len)==0) printObject((NB_Object *)def);
      else{
        if(strcmp(ident,"?")!=0) outMsg(0,'E',"Option \"%s\" not recognized.",ident);
        outPut("\nTo show information about a term in the active context:\n\n",ident);
        outPut("  show <term> [<option>]\n\n",ident);
        outPut("You may specify an option with a single character:\n\n");
        outPut("  (v)alue       - object representing a value\n");
        outPut("  (d)efinition  - object generating the value\n");
        outPut("  (s)ubscribers - objects subscribing to the value\n");
        outPut("  (i)mpact      - subscription hierarchy\n"); 
        outPut("\n");
        }
      dropObject(val);
      dropObject(def);
      }
    else{
      if(*ident!=0 && symid!='?') outMsg(0,'E',"Expecting (<expression>) | <term> | - | + | = | / | % | *  at \"%s\".",cursave);
      outPut("\nThe show command provides context specific and global information.\n\n");
      outPut("  show (<cell>) [<option>]  Show value of a cell expression.\n");
      outPut("  show <term> [<option>]    Show specific term in active context.\n");
      outPut("  show -<term_type>         Terms of a given type from active context.\n");
      outPut("  show +<dictionary>        Terms in an alternate dictionary (name space).\n");
      outPut("  show =<cell_type>         Global cell expressions of a specified type.\n");
      outPut("  show /<trigger_type>      Global triggers of a specified type.\n");
      outPut("  show %<measures>          Performance measures.\n");
      outPut("  show *<section> [<topic>] Help on specified topic.\n\n");
      outPut("A partial SHOW command displays a menu (e.g. \"show -\").\n\n");
      outPut("Use \"?\" in place of options [<...>] for more information.\n");
      }
    return;
    }
  symid=*cursor;
  cursor++;
  cursave=cursor;
  nbParseSymbol(ident,&cursor);
  /* don't worry about symid - if not 't' we'll get error message anyway */
  len=strlen(ident);
  if(len==0){
    strcpy(ident,"?");
    len=1;
    }
  if(symid=='-'){  /* active context terms */
    if(strncmp(ident,"terms",len)==0) termPrintGloss((NB_Term *)context,NULL,0);
    else if(strncmp(ident,"cells",len)==0) termPrintGloss((NB_Term *)context,NULL,0);
    else if(strncmp(ident,"facts",len)==0) termPrintGloss((NB_Term *)context,NULL,TYPE_IS_FACT);
    else if(strncmp(ident,"if",len)==0) termPrintGloss((NB_Term *)context,NULL,0);
    else if(strncmp(ident,"numbers",len)==0) termPrintGloss((NB_Term *)context,realType,0);
    else if(strncmp(ident,"on",len)==0) termPrintGloss((NB_Term *)context,NULL,0);
    else if(strncmp(ident,"rules",len)==0) termPrintGloss((NB_Term *)context,NULL,TYPE_IS_RULE);
    else if(strncmp(ident,"strings",len)==0) termPrintGloss((NB_Term *)context,strType,0);
    else if(strncmp(ident,"when",len)==0) termPrintGloss((NB_Term *)context,NULL,0);
    else{
      if(strcmp(ident,"?")!=0) outMsg(0,'E',"Expecting term type option at \"%s\".",cursave);
      outPut("\nTo show all terms of a specified type in the active context:\n\n");
      outPut("  show -<term_type>\n\n");
      outPut("The <term_type> option may be specified with a single character:\n\n");
      outPut("  (c)ells     - terms defined as dynamic cell expressions\n");
      outPut("  (f)acts     - terms defined as constant numbers or strings\n");
      outPut("  (i)f        - if rules\n");
      outPut("  (n)umbers   - numbers\n");
      outPut("  (o)n        - on rules\n");
      outPut("  (r)ules     - if, on, and when rules\n");
      outPut("  (s)trings   - strings\n");
      outPut("  (t)erms     - all terms defined in the current context\n");
      outPut("  (w)hen      - when rules\n");
      outPut("\n");
      }
    }
  else if(symid=='+'){  /* dictionary */
    if(strncmp(ident,"settings",len)==0) showSet();
    else if(strncmp(ident,"identities",len)==0){
      outPut("active: %s\n",clientIdentity->name->value);
      termPrintGloss(identityC,NULL,0);
      }
    else if(strncmp(ident,"calendars",len)==0) termPrintGlossHome(nb_TimeCalendarContext,NULL,0);
    else if(strncmp(ident,"globals",len)==0) termPrintGloss(symGloss,NULL,0);
    else if(strncmp(ident,"locals",len)==0) termPrintGloss(symContext,NULL,0);
    else if(strncmp(ident,"modules",len)==0){
      termPrintGloss(moduleC,NULL,0);
      termPrintGloss(nb_SkillGloss,NULL,0);
      nbModuleShowInstalled(context);
      }
    else if(strncmp(ident,"processes",len)==0) showProcessList();
    else if(strncmp(ident,"types",len)==0) termPrintGloss(nb_TypeGloss,NULL,0);
    else if(strncmp(ident,"verbs",len)==0) nbVerbPrintAll(context->object.type->stem->verbTree);
    else{
      if(strcmp(ident,"?")!=0) outMsg(0,'E',"Expecting name space option at \"%s\".",cursave);
      outPut("\nTo show all terms in an alternate dictionary (name space):\n\n");
      outPut("  show +<dictionary>\n\n");
      outPut("You may specify the <dictionary> option with a single character:\n\n");
      //outPut("  (b)rains     - declared brains (peers)\n");
      outPut("  (c)alendars  - declared calendars (time expressions)\n");
      outPut("  (g)lobals    - global source variables\n");
      outPut("  (i)dentities - declared identities\n");
      outPut("  (l)ocals     - local source variables\n");
      outPut("  (m)odules    - declared modules (extensions)\n");
      outPut("  (s)ettings   - control variables assigned with the SET command.\n");
      outPut("  (t)ypes      - recognized term definition types\n");
      outPut("  (v)erbs      - recognized verbs\n");
      outPut("\n");
      }
    }
  else if(symid=='='){  /* cell expression */   
    if(strncmp(ident,"conditions",len)==0) condPrintAll(0);
    else if(strncmp(ident,"boolean",len)==0) condPrintAll(2);
    else if(strncmp(ident,"relations",len)==0) condPrintAll(1);
    else if(strncmp(ident,"math",len)==0)  printMathAll();
    else if(strncmp(ident,"times",len)==0) condPrintAll(3);
    else if(strncmp(ident,"string",len)==0) printStringAll();
    else if(strncmp(ident,"number",len)==0) printRealAll();
    else if(strncmp(ident,"list",len)==0) nbListShowAll();
    else if(strncmp(ident,"projection",len)==0) nbProjectionShowAll();
    /* old stuff */
    else if(strncmp(ident,"timers",len)==0) nbClockShowTimers(cursor);   /* logical    */
    else if(strncmp(ident,"schedule",len)==0) nbClockShowTimers(cursor); /* deprecated */
    else{
      if(strcmp(ident,"?")!=0) outMsg(0,'E',"Expecting cell expression type option at \"%s\".",cursave);
      outPut("\nTo show all cells of a given type:\n\n");
      outPut("  show =<cell_type>\n\n");
      outPut("You may specify the <cell_type> option with a single character:\n\n");
      outPut("  (b)oolean   - boolean condition cells\n");
      outPut("  (c)ondition - all condition cells\n");
      outPut("  (l)ist      - list cells\n");
      outPut("  (m)ath      - math cells (real number operations)\n");
      outPut("  (n)umber    - number constants\n");
      outPut("  (r)elation  - relational condition cells\n");
      outPut("  (s)tring    - string constants\n");
      outPut("  (t)ime      - time condition cells\n");
      outPut("\n");
      }
    }
  else if(symid=='/'){  /* trigger type */   
    if(strncmp(ident,"clock",len)==0) nbClockShowTimers(cursor);    /* preferred   */
    else if(strncmp(ident,"rule",len)==0) nbRuleShowAll();
    else if(strncmp(ident,"process",len)==0) nbClockShowProcess(cursor);   /* process time */
    /* old stuff */
    else if(strncmp(ident,"timers",len)==0) nbClockShowTimers(cursor);   /* logical    */
    else if(strncmp(ident,"schedule",len)==0) nbClockShowTimers(cursor); /* deprecated */
    else{
      if(strcmp(ident,"?")!=0) outMsg(0,'E',"Expecting trigger type option at \"%s\".",cursave);
      outPut("\nTo show all triggers of a specified type:\n\n");
      outPut("  show /<trigger_type>\n\n");
      outPut("You may specify the <trigger_type> with a single character.\n\n");
      outPut("  (c)lock     - active timers\n");
      outPut("  (p)roblem   - rules representing a problem to be solved\n");
      outPut("  (r)ule      - rules\n");
      outPut("\n");
      }
    }
#if !defined(WIN32)
  else if(symid=='%'){  // experimental measures
    if(strncmp(ident,"type",len)==0) nbObjectShowTypes();
    //else if(strncmp(ident,"facet",len==0 nbSkillShowTypes();
    else{
      if(strcmp(ident,"?")!=0) outMsg(0,'E',"Expecting performance type option at \"%s\".",cursave);
      outPut("\nTo show all time measurements of a specified type:\n\n");
      outPut("  show ~<time_measure_type>\n\n");
      outPut("You may specify the <time_measure_type> with a single character.\n\n");
      outPut("  (t)ype      - cell types\n");
      outPut("  (s)kill     - skills\n");
      outPut("\n");
      }
    }
#endif
  else if(symid=='*'){  /* help topic */   
    if(strncmp(ident,"about",len)==0){
      symid=nbParseSymbol(ident,&cursor);
      if(symid=='t'){
        if(strncmp(ident,"copyright",len)==0) showCopyright();
        else if(strncmp(ident,"version",len)==0) showVersion();
        }
      else showAbout();
      }
    else if(strncmp(ident,"copyright",len)==0) showCopyright();
    else if(strncmp(ident,"version",len)==0) showVersion();
    else{
      if(strcmp(ident,"?")!=0) outMsg(0,'E',"Expecting help topic option at \"%s\".",cursave);
      outPut("\nTo obtain help on a particular topic:\n\n");
      outPut("  show *<section> [<topic>]\n\n");
      outPut("You may specify the <section> with a single character:\n\n");
      outPut("  (a)bout  [(v)ersion|(c)opyright]\n\n");
      outPut("More topics will be added in the future.\n");
      outPut("\n");
      }
    }
  }

void nbCmdQuery(nbCELL context,char *verb,char *cursor){
  char symid,ident[256];
  NB_Term *term;
  NB_Object *object;
  if(!(clientIdentity->authority&AUTH_CONTROL)){
    outMsg(0,'E',"Identity \"%s\" does not have authority to query.",clientIdentity->name->value);
    return;
    }
  if(strcmp(verb,"solve")==0) outMsg(0,'W',"The 'solve' command is deprecated, use 'query' instead.");
    
  symid=nbParseSymbol(ident,&cursor);
  if(symid==';'){
     nbRuleSolve((NB_Term *)context);
     return;
     }
  term=nbTermFind((NB_Term *)context,ident);
  if(term==NULL){
    outMsg(0,'E',"Term \"%s\" not defined.",ident);
    return;
    }
  if(((NB_Object *)term->def)->type==nb_NodeType){
    nbRuleSolve(term);
    return;
    }
  object=nbCellSolve_((NB_Cell *)term);
  outPut("Result: ");
  outPut("%s ",object->type->name);
  printObject(object);
  outPut("\n");
  }         
  

/*
*  Define global process variables
*/
void nbParseArgAssertion(char *cursor){
  char ident[256],value[1024],symid,*valcur;
 
  symid=nbParseSymbol(ident,&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expecting term at \"%s\".",ident);
    return;
    }
  if(*cursor!='='){
    outMsg(0,'E',"Expecting '=' at \"%s\".",cursor);
    return;
    }
  cursor++;
  if(trace){
    outPut("Defining argument \"%s\"\n",ident);
    outPut("Value is [%s]\n",cursor);
    }
  valcur=cursor;
  symid=nbParseSymbol(value,&cursor);
  if(*cursor==0){
    if(symid=='i' || symid=='r'){
      nbTermNew(symGloss,ident,parseReal(value));
      return;
      }
    else if(symid=='s'){
      nbTermNew(symGloss,ident,useString(value));
      return;
      }
    }
  if(strchr(valcur,'"')!=NULL){
    outMsg(0,'E',"Quotes not supported in strings [%s]",valcur);
    return;
    }
  nbTermNew(symGloss,ident,useString(valcur));
  }

/* 
*  Set node (context) options
*
*    <context>. use[(<option_list>)][:<consultant>];
*
*/
void nbCmdUse(nbCELL contextCell,char *verb,char *cursor){
  NB_Term *context=(NB_Term *)contextCell;  // goof around with type---we assume contextCell is an NB_Term
  char *optcur;
  int optlen;
  if(!(clientIdentity->authority&AUTH_CONTROL)){
    outMsg(0,'E',"Identity \"%s\" not authorized to set control values.",clientIdentity->name->value);
    return;
    }
  if(((NB_Node *)context->def)->owner!=clientIdentity){
    outMsg(0,'E',"Identity \"%s\" not owner of \"%s\" node.",clientIdentity->name->value,context->word->value);
    return;
    }
  while(*cursor==' ') cursor++;
  if(*cursor=='('){
    while(*cursor!=')'){
      cursor++;
      while(*cursor==' ') cursor++;
      optcur=cursor;
      while(*cursor!=',' && *cursor!=')' && *cursor!=0) cursor++;
      if(*cursor==0){
        outMsg(0,'E',"Unbalanced parentheses in option list - end of line reached");
        return;
        }
      if(*optcur=='!'){  // set option off
        optcur++;
        optlen=cursor-optcur;
        if(strncmp(optcur,"echo",optlen)==0) ((NB_Node *)context->def)->cmdopt&=255-NB_CMDOPT_ECHO; 
        else if(strncmp(optcur,"hush",optlen)==0) ((NB_Node *)context->def)->cmdopt&=255-NB_CMDOPT_HUSH;
        else if(strncmp(optcur,"trace",optlen)==0)((NB_Node *)context->def)->cmdopt&=255-NB_CMDOPT_TRACE;
        else{
          outMsg(0,'E',"Option not recognized at: %s",optcur);
          return;
          }
        }
      else{              // set option on
        optlen=cursor-optcur;
        if(strncmp(optcur,"echo",optlen)==0) ((NB_Node *)context->def)->cmdopt|=NB_CMDOPT_ECHO;
        else if(strncmp(optcur,"hush",optlen)==0) ((NB_Node *)context->def)->cmdopt|=NB_CMDOPT_HUSH;
        else if(strncmp(optcur,"trace",optlen)==0)((NB_Node *)context->def)->cmdopt|=NB_CMDOPT_TRACE;
        else{
          outMsg(0,'E',"Option not recognized at: %s",optcur);
          return;
          }
        }
      }
    cursor++;
    while(*cursor==' ') cursor++;
    }
  if(*cursor==':'){
    cursor++;
    while(*cursor==' ') cursor++;
    ((NB_Node *)context->def)->source=grabObject(useString(cursor));
    }
  else if(*cursor!=';' && *cursor!='\n' && *cursor!=0){
    outMsg(0,'E',"Unexpected character '%c' at:  %s",*cursor,cursor);
    }
  }

// Set option string safely

void nbSetOptStr(char *option,char *buf,char *value,size_t bufsize){
  if(strlen(value)>=bufsize){
    outMsg(0,'E',"Length of %s option (%d) is longer than the maximum allowed (%d)",option,strlen(value),bufsize-1);
    outMsg(0,'E',"Terminating on error");
    outFlush();
    exit(NB_EXITCODE_FAIL);
    }
  strncpy(buf,value,bufsize);
  *(buf+bufsize-1)=0;
  }

/*
*  Set options
*
*    nb -<o>   --<option>=<value>,<option>=<value>
*
*    -ooooo[,<longOption>]
*    --<logonOption>         
*    option="value",...
*/    
void nbCmdSet(nbCELL context,char *verb,char *cursor){
  /* Assign a new value to an option */
  //struct NB_STEM *stem=context->object.type->stem;
  char ident[256],operator[256],token[256];
  int i;
  char *strcur,*cursave;
  char symid=',';
  if(!(clientIdentity->authority&AUTH_CONTROL)){
    outMsg(0,'E',"Identity \"%s\" not authorized to set control values.",clientIdentity->name->value);
    return;
    }
  cursave=cursor;
  if(*cursor=='-' || *cursor=='+'){
    cursor++; /* accept "--option" from command line */
    while(NB_ISALPHA((int)*cursor)){
      switch(*cursor){
        case 'a': nb_opt_audit=1;   break;
        case 'A': nb_opt_audit=0;   break;
        case 'b': nb_opt_bail=1;    break;
        case 'B': nb_opt_bail=0;    break;
        case 'd': nb_opt_daemon=1;  break;
        case 'D': nb_opt_daemon=0;  break;
        case 'p': nb_opt_prompt=1;  break;
        case 'P':
          nb_opt_prompt=0;   // turn off prompt option
          nb_flag_input=1;   // flag input so we don't default to interactive mode
          *nb_cmd_prefix=0;  // clear prefix
          strcpy(nb_cmd_prompt,"> ");  // reset prompt string
          break;
        case 'q': nb_opt_query=1;   break;
        case 'Q': nb_opt_query=0;   break;
        case 's': nb_opt_servant=1; break;
        case 'S': nb_opt_servant=0; break;
        case 't': trace=1;          break;
        case 'T': trace=0;          break;
        case 'U': nb_opt_user=0;    break;  // don't load user profile
        default:
          outMsg(0,'E',"Switch option '%c' not recognized.",*cursor);
          outPut("Usage:  nb -aAbBdDpPqQsS\n");
          return;
        }
      cursor++;
      }
    if(*cursor==0 || *cursor==';') return;
    if(*cursor=='-' || *cursor==',') cursor++,symid=',';
    else{
      outMsg(0,'E',"Unrecognized symbol '%c' in switch option: %s",*cursor,cursave);
      return;
      }
    }
  while(symid==','){
    symid=nbParseSymbol(ident,&cursor);
    if(symid!='t'){
      outMsg(0,'E',"Expecting term \"%s\".",ident);
      return;
      }    
    symid=nbParseSymbol(operator,&cursor);
    if(symid=='='){ /* non-boolean option */
      /* let's tolerate strings without quotes for the command line */
      if(*cursor!='"' && (*cursor<'0' || *cursor>'9')){
        strcur=token;
        for(strcur=token;*cursor!=' ' && *cursor!=',' && *cursor!=';' && *cursor!=0;strcur++){
          *strcur=*cursor;
          cursor++;
          } 
        *strcur=0;
        symid='s';
        }
      else symid=nbParseSymbol(token,&cursor);
      if(symid=='s'){ /* string */
        if(strcmp(ident,"tee")==0){
          nbSetOptStr(ident,lname,token,sizeof(lname));
          if(lfile) fclose(lfile);  // close if we are switching files
          if((lfile=fopen(lname,"a"))==NULL){
            outMsg(0,'E',"Unable to open log file \"%s\", errno=%d",lname,errno);
            outMsg(0,'E',"nodebrain Aborting.");
            outFlush();
            exit(NB_EXITCODE_FAIL);
            } 
          outMsg(0,'I',"Using log file \"%s\"",lname);
          }
        else if(strcmp(ident,"log")==0){
          outLogName(token);
          outMsg(0,'I',"NodeBrain %s will log to %s",myname,token);
          }
        else if(strcmp(ident,"out")==0){
          if(*(token+strlen(token)-1)!='/') strcat(token,"/");
          outDirName(token);
          }
        //else if(strcmp(ident,"que")==0) nbSetOptStr(ident,quedir,token,sizeof(quedir));
        else if(strcmp(ident,"jnl")==0){
          nbSetOptStr(ident,jname,token,sizeof(jname));
          if((jfile=fopen(jname,"a"))==NULL){
            outMsg(0,'E',"Unable to open journal file \"%s\", errno=%d",jname,errno);
            outMsg(0,'E',"nodebrain Aborting.");
            outFlush();
            exit(NB_EXITCODE_FAIL);
            } 
          }
        //else if(strcmp(ident,"ipaddr")==0) nbSetOptStr(ident,serveipaddr,token,sizeof(serveipaddr));
        //else if(strcmp(ident,"oar")==0)  nbSetOptStr(ident,serveoar,token,sizeof(serveoar));
        else if(strcmp(ident,"jail")==0) nbSetOptStr(ident,servejail,token,sizeof(servejail)); // 2006-05-12 eat 0.6.6
        else if(strcmp(ident,"dir")==0)  nbSetOptStr(ident,servedir,token,sizeof(servedir));   // 2006-05-12 eat 0.6.6
        else if(strcmp(ident,"user")==0) nbSetOptStr(ident,serveuser,token,sizeof(serveuser)); // 2006-05-12 eat 0.6.6
        else{
          outMsg(0,'E',"Unrecognized string option \"%s\".",ident);
          return;
          }
        }
      else if(symid=='i'){ /* integer */
        i=atoi(token);
        //if(strcmp(ident,"parent")==0){
        //  stem->parentChannel=challoc();  // allocate channel
        //  stem->parentChannel->socket=i;  // set socket to parent socket
        //  }
        //else if(strcmp(ident,"socket")==0) skull_socket=i;
        //else if(strcmp(ident,"trace")==0) trace=i; 
        if(strcmp(ident,"trace")==0) trace=i; 
        /*
        else if(strcmp(ident,"nbp")==0){
          if(i<nbpmin){
            outMsg(0,'E',"Using minimum supported protocol NBP%u",nbpmin);
            nbp=nbpmin;
            }
          else if(i>nbpmax){
            outMsg(0,'E',"Using highest supported protocol NBP%u",nbpmax);
            nbp=nbpmax;
            }
          else nbp=i;
          }
        */
        else if(strcmp(ident,"processLimit")==0) nbMedullaProcessLimit(i);
        else{
          outMsg(0,'E',"Unrecognized integer option \"%s\".",ident);
          return;
          }
        }       
      else{
        outMsg(0,'E',"Unrecognized value [%s] for \"%s\".",token,ident);
        return;
        }
      symid=nbParseSymbol(ident,&cursor);  
      }
    else if(symid==',' || symid==';'){ /* boolean option */
      if(strcmp(ident,"a")==0      || strcmp(ident,"audit")==0)     nb_opt_audit=1;
      else if(strcmp(ident,"A")==0 || strcmp(ident,"noAudit")==0)   nb_opt_audit=0;
      else if(strcmp(ident,"b")==0 || strcmp(ident,"bail")==0)      nb_opt_bail=1;
      else if(strcmp(ident,"B")==0 || strcmp(ident,"noBail")==0)    nb_opt_bail=0;
      else if(strcmp(ident,"d")==0 || strcmp(ident,"daemon")==0)    nb_opt_daemon=1;
      else if(strcmp(ident,"D")==0 || strcmp(ident,"noDaemon")==0)  nb_opt_daemon=0;
      else if(strcmp(ident,"p")==0 || strcmp(ident,"prompt")==0)    nb_opt_prompt=1;
      else if(strcmp(ident,"P")==0 || strcmp(ident,"noPrompt")==0){
        nb_opt_prompt=0;   // turn off prompt option
        nb_flag_input=1;   // flag input so we don't default to interactive mode
        *nb_cmd_prefix=0;  // clear prefix
        strcpy(nb_cmd_prompt,"> ");  // reset prompt string
        }
      else if(strcmp(ident,"q")==0 || strcmp(ident,"query")==0)     nb_opt_query=1;
      else if(strcmp(ident,"Q")==0 || strcmp(ident,"noQuery")==0)   nb_opt_query=0;
      // The --solve option is deprecated - use solve command
      else if(strcmp(ident,"solve")==0)     nb_opt_query=1;
      else if(strcmp(ident,"noSolve")==0)   nb_opt_query=0;
      // The -s --servant option replaces the -c --child option
      // and assigns a new meaning to the short option -s which
      // was previously associated with the --solve option
      else if(strcmp(ident,"s")==0 || strcmp(ident,"servant")==0)   nb_opt_servant=1;
      else if(strcmp(ident,"S")==0 || strcmp(ident,"noServant")==0) nb_opt_servant=0;
      else if(strcmp(ident,"showterms")==0) termPrintGloss((NB_Term *)context,NULL,0);

      /*
      *  Debugging options for tracing interpreter function calls
      */
      else if(strcmp(ident,"shim")==0); /* this option is processed earlier */
      else if(strcmp(ident,"t")==0 || strcmp(ident,"trace")==0)   trace=1;
      else if(strcmp(ident,"T")==0 || strcmp(ident,"noTrace")==0) trace=0;
      else if(strcmp(ident,"traceParse")==0) parseTrace=1;
      else if(strcmp(ident,"notraceParse")==0) parseTrace=0;
      //else if(strcmp(ident,"traceFile")==0) fileTrace=1;
      //else if(strcmp(ident,"notraceFile")==0) fileTrace=0;
      else if(strcmp(ident,"traceMessage")==0) msgTrace=1;
      else if(strcmp(ident,"notraceMessage")==0) msgTrace=0;
      else if(strcmp(ident,"traceQuery")==0) queryTrace=1;
      else if(strcmp(ident,"notraceQuery")==0) queryTrace=0;
      else if(strcmp(ident,"traceSolve")==0) queryTrace=1;
      else if(strcmp(ident,"notraceSolve")==0) queryTrace=0;
      else if(strcmp(ident,"traceSource")==0) sourceTrace=1;
      else if(strcmp(ident,"notraceSource")==0) sourceTrace=0;
      else if(strcmp(ident,"traceSymbolic")==0) symbolicTrace=1;
      else if(strcmp(ident,"notraceSymbolic")==0) symbolicTrace=0;
      /* continue to support old option names for a few versions - now 0.5.1 */
      else if(strcmp(ident,"parseTrace")==0) parseTrace=1;
      else if(strcmp(ident,"noparseTrace")==0) parseTrace=0;
      //else if(strcmp(ident,"fileTrace")==0) fileTrace=1;
      //else if(strcmp(ident,"nofileTrace")==0) fileTrace=0;
      //else if(strcmp(ident,"queryTrace")==0) queryTrace=1;
      //else if(strcmp(ident,"noqueryTrace")==0) queryTrace=0;
      else if(strcmp(ident,"sourceTrace")==0) sourceTrace=1;
      else if(strcmp(ident,"nosourceTrace")==0) sourceTrace=0;
      else if(strcmp(ident,"symbolicTrace")==0) symbolicTrace=1;
      else if(strcmp(ident,"nosymbolicTrace")==0) symbolicTrace=0;
      /* 
      *  Debugging options used by "show" command
      */
      else if(strcmp(ident,"state")==0)       showstate=1;
      else if(strcmp(ident,"nostate")==0)     showstate=0;
      else if(strcmp(ident,"showValue")==0)   showvalue=1;
      else if(strcmp(ident,"noshowValue")==0) showvalue=0;
      else if(strcmp(ident,"showLevel")==0)   showlevel=1;
      else if(strcmp(ident,"noshowLevel")==0) showlevel=0;
      else if(strcmp(ident,"showCount")==0)   showcount=1;
      else if(strcmp(ident,"noshowCount")==0) showcount=0;
      /* change document to use the option names above */
      else if(strcmp(ident,"showValue")==0)   showvalue=1;
      else if(strcmp(ident,"noshowValue")==0) showvalue=0;
      else if(strcmp(ident,"showLevel")==0)   showlevel=1;
      else if(strcmp(ident,"noshowLevel")==0) showlevel=0;
      else if(strcmp(ident,"showCount")==0)   showcount=1;
      else if(strcmp(ident,"noshowCount")==0) showcount=0;
      else{
        outMsg(0,'E',"Unrecognized Boolean option \"%s\".",ident);
        return;
        }
      }
    else{
      outMsg(0,'E',"Unexpected symbol \"%c\" before \"%s\".",symid,cursor);
      return;
      }
    }
  }

/*
*  Assert (or alert)
*/
void nbCmdAssert(nbCELL context,char *verb,char *cursor){
  /* assert a new value for a variable */
  /* Note: nbTermFind will locate variables in */
  /* class definitions and peer contexts.  It seems odd to allow assertions */
  /* to variables in class definitions from any context. */
  /* Think about the necessary controls. */
  NB_Link *assertion=NULL;
  int alert=0,alertCount;

  if(*(verb+1)=='l') alert=1; // determine if assert or alert

  /* handle cache reference */
  while(*cursor==' ') cursor++;
  assertion=nbParseAssertion((NB_Term *)context,(NB_Term *)context,&cursor);
  if(*cursor!=';' && *cursor!=0){
    outMsg(0,'E',"Unrecognized at-->%s",cursor);
    dropMember(assertion);
    return;
    }
  if(assertion!=NULL){
    assert(assertion,alert);  // assert or alert
    dropMember(assertion);
    }
  if(alert){
    // This alertCount is used to avoid alerting the address context if a skill already did
    alertCount=((NB_Node *)((NB_Term *)context)->def)->alertCount;
    nbRuleReact(); // react to changing conditions
    if(alertCount==((NB_Node *)((NB_Term *)context)->def)->alertCount) contextAlert((NB_Term *)context);
    }
  return;
  }

/*
*  Set symbolic variables
*     <parm1>=<value1>,<parm2>="<value2>",...
*
*     mode: 0 - update or create [let] ; 1 - create only [default]
*/   
void iLet(char *cursor,NB_Term *context,int mode){
  char ident[256],operator[256],token[4096],*cursave;
  NB_Term *term;
  NB_Object *object;
  int found;
  char symid=',';
  
  if(!(clientIdentity->authority&AUTH_ASSERT)){
    outMsg(0,'E',"Identity \"%s\" does not have authority to assign symbolic values.",clientIdentity->name->value);
    return;
    }
  while(symid==','){
    symid=nbParseSymbol(ident,&cursor);
    if(symid!='t'){
      outMsg(0,'E',"Expecting term \"%s\".",ident);
      return;
      }
    symid=nbParseSymbol(operator,&cursor);
    if(symid!='='){
      outMsg(0,'E',"Expecting '=' \"%s\".",operator);
      return;
      }
    cursave=cursor;
    symid=nbParseSymbol(token,&cursor);
    term=nbTermFind(context,ident);
    if(term==NULL){
      term=nbTermNew(context,ident,nb_Unknown);
      found=0;
      }
    else found=1;  
    cursor=cursave;
    object=nbParseCell((NB_Term *)context,&cursor,0);
    if(found==0 || mode==0){
      if(strcmp(operator,"==")==0) nbTermAssign(term,object);
      else{
        nbTermAssign(term,(NB_Object *)nbCellCompute((NB_Cell *)context,(NB_Cell *)object));
        dropObject(term->def); /* 2004/08/28 eat */
        }
      }
    /* else dropObjectLight(object) - drop if zero but don't dec */
    cursave=cursor;
    symid=nbParseSymbol(ident,&cursor);  
    }
  if(symid!=';'){
    outMsg(0,'E',"Expected delimiter ';' not found. [%s]",cursor);
    return;
    }
  }

/*
*  Enable or disable an object
*/           
void nbCmdEnable(nbCELL context,char *verb,char *cursor){ 
  char ident[256],symid,*cursave;
  NB_Term *term;
  NB_Cell *cell;

  if(!(clientIdentity->authority&AUTH_DEFINE)){
    outMsg(0,'E',"Identity \"%s\" does not have enable/disable authority.",clientIdentity->name->value);
    return;
    }
  cursave=cursor;
  symid=nbParseSymbol(ident,&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expecting term at \"%s\"",cursave);
    return;
    }
  if((term=nbTermFind((NB_Term *)context,ident))==NULL){
    outMsg(0,'E',"Term \"%s\" not defined.",ident);
    return;
    }
  cell=(NB_Cell *)term->def;
  if(!(cell->object.type->attributes&TYPE_ENABLES)){
    outMsg(0,'E',"Term \"%s\" does not qualify for enable/disable command",ident);
    return;
    }
  if(*verb=='e'){  // enable
    if(cell->object.value==nb_Disabled){
      cell->object.type->enable(cell);
      cell->object.value=cell->object.type->eval(cell); // eval to get current value
      }
    else outMsg(0,'I',"Term \"%s\" is already enabled",ident);
    }
  else if(cell->object.value!=nb_Disabled){  // disable
    cell->object.type->disable(cell);
    cell->object.value=nb_Disabled;
    }
  else outMsg(0,'I',"Term \"%s\" is already disabled",ident);
  }

//struct SCHED *iSchedule(cursor) char **cursor; {
//  char *delim,msg[256];
//  char symid,ident[256];
//  struct SCHED *sched;
  
//  symid=nbParseSymbol(ident,cursor);
//  if(trace) outMsg(0,'T',"Calling newSched A [%s].",ident); 
//  sched=newSched(symid,ident,&delim,msg,1);
//  if(trace) outMsg(0,'T',"Back from newSched A.");
//  if(sched==NULL){
//    outPut("%s\n",msg);
//    return(NULL);
//    }
//  if(trace) outMsg(0,'T',"Schedule structure generated.");
//  return(sched);
//  }
  
void nbCmdArchive(nbCELL context,char *verb,char *cursor){
  char prefix[100],target[100];
  size_t len;
  time_t systemTime;
  struct tm  *localTime;
  int rc;
  char *logname;

  if(!(clientIdentity->authority&AUTH_CONTROL)){
    outMsg(0,'E',"Identity \"%s\" not authorized to archive the log file.",clientIdentity->name->value);
    return;
    }
  if(!agent){
    outMsg(0,'E',"archive command only supported in daemon mode"); 
    return;
    } 
  if(*(outLogName(NULL))==0){
    outMsg(0,'E',"Daemon log file not defined.");
    return;
    }
  /* rename log file */
  logname=outLogName(NULL);
  cursor=strstr(logname,".log");
  if(cursor==NULL) strcpy(prefix,logname);
  else{
    len=cursor-logname;
    strncpy(prefix,logname,len);
    *(prefix+len)=0;
    }
  time(&systemTime);
  localTime=localtime(&systemTime);
  sprintf(target,"%s.%.4d%.2d%.2d%.2d%.2d%.2d.log",prefix,
    localTime->tm_year+1900,localTime->tm_mon+1,localTime->tm_mday,
    localTime->tm_hour,localTime->tm_min,localTime->tm_sec);
  outMsg(0,'I',"Archiving log as %s",target); 
  outFlush();
  fflush(NULL);
#if defined(WIN32)
  //close(_fileno(stdout)); // we only archive stderr now
//  close(_fileno(stderr));
//  CloseHandle(GetStdHandle(STD_ERROR_HANDLE));
  rc=-1;
  if(!freopen("nul","w",stderr)) outMsg(0,'E',"Unable to switch stderr to nul");
  else{
    rc=rename(logname,target);
    freopen(logname,"a",stderr);
    }
//  hStdErr=CreateFile(logname,FILE_APPEND_DATA,FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  // If we can't open a new log file we are in trouble
  // Perhaps it would be better to abort here---but we'll just go blind for now
//  if(hStdErr==INVALID_HANDLE_VALUE) hStdErr=CreateFile("nul",GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
//  SetStdHandle(STD_ERROR_HANDLE,hStdErr);

//    if(freopen(logname,"a",stderr)==NULL)
//      freopen("nul","a",stderr); // we are in big trouble now
//    }
  //open(logname,O_CREAT|O_RDWR,S_IREAD|S_IWRITE);
#else
  //close(1);
  close(2);
  rc=rename(logname,target);
  open(logname,O_CREAT|O_RDWR,S_IRUSR|S_IWUSR|S_IRGRP);
  //dup(1);
#endif
  if(rc) outMsg(0,'E',"Unable to rename log file \"%s\" to \"%s\", errno=%d - %s",logname,target,errno,strerror(errno));
  else showHeading();
  }

struct IDENTITY *iIdentity(char **cursorP){
  /*   <identity> ... */
  NB_Term *term;
  char symid,*cursave;
  char ident[256];

  cursave=*cursorP;
  symid=nbParseSymbol(ident,cursorP); 
  if(symid!='t'){
    outMsg(0,'E',"Expecting term identifier at [%s].",cursave);
    return(NULL);
    }
  if((term=nbTermFind(identityC,ident))==NULL){
    outMsg(0,'E',"Identity \"%s\" not defined.",ident);
    return(NULL);
    }
  return((struct IDENTITY *)term->def);
  }
  
unsigned char nbGetAuthMask(char *rank){
  if(strcmp(rank,"owner")==0) return(AUTH_OWNER);
  else if(strcmp(rank,"user")==0) return(AUTH_USER);
  else if(strcmp(rank,"peer")==0)  return(AUTH_PEER);
  else if(strcmp(rank,"guest")==0) return(AUTH_GUEST);
  return(0);
  }

void nbCmdRank(nbCELL context,char *verb,char *cursor){
  /*   rank <identity> <rank>;  */
  char symid,*cursave;
  char ident[256];
  struct IDENTITY *identity;
  unsigned char authmask;

  identity=iIdentity(&cursor);
  if(identity==NULL) return;
  
  cursave=cursor;
  symid=nbParseSymbol(ident,&cursor); 
  if(symid!='t'){
    outMsg(0,'E',"Expecting permission name at [%s].",cursave);
    return;
    }
  authmask=nbGetAuthMask(ident);
  if(authmask) identity->authority=authmask;
  else{
    outMsg(0,'E',"Permission \"%s\" not recognized.",ident);
    return;
    }
  outMsg(0,'I',"Identity \"%s\" ranked as \"%s\".",identity->name->value,ident);
  }
  
void nbCmdGrant(nbCELL context,char *verb,char *cursor){
  outMsg(0,'E',"Statement not yet implemented.");
  } 
   
void nbCmdDeny(nbCELL context,char *verb,char *cursor){
  outMsg(0,'E',"Statement not yet implemented.");
  }  

void nbCmdDeclare(nbCELL context,char *verb,char *cursor){  
  /*   declare <identity> identity modulus.exponent+exponent.user; */   
  /*   declare <brain> brain <host>:<port>/identity; */
  /*   declare <module> module <filename>; */
  /*   declare <calendar> calendar <time_expression>; */
  char symid,*cursave;
  char ident[256],type[256],*string,msg[1024];
  struct IDENTITY *identity;
  //struct BRAIN *brain;
  unsigned char authmask=AUTH_GUEST;

  if(!(clientIdentity->authority&AUTH_DECLARE)){
    outMsg(0,'E',"Identity \"%s\" not authorized to declare control objects.",clientIdentity->name->value);
    return;
    }
  cursave=cursor;
  symid=nbParseSymbol(ident,&cursor); 
  if(symid!='t'){
    outMsg(0,'E',"Expecting term identifier at [%s].",cursave);
    return;
    }
  while(*cursor==' ') cursor++;
  cursave=cursor;
  symid=nbParseSymbol(type,&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expecting term identitier at [%s].",cursave);
    return;
    }
  if(strcmp(type,"identity")==0){
    if(nbTermFind(identityC,ident)!=NULL){
      outMsg(0,'E',"Identity \"%s\" already defined.",ident);
      return;
      }
    while(*cursor==' ') cursor++;
    string=cursor;
    while(*cursor!=' ' && *cursor!=';' && *cursor!=0) cursor++;
    *cursor=0;
    if(*string){
      if(strchr(string,'.')) outMsg(0,'W',"Obsolete key ignored in '%s' identity declaration.",ident);
      else authmask=nbGetAuthMask(string);
      }
    if((identity=nbIdentityNew(ident,0))==NULL){
      outMsg(0,'E',"Identity declaration failed.");
      return;
      }
    identity->authority=authmask;
    nbTermNew(identityC,ident,identity);
    }
  else if(strcmp(type,"module")==0){
    nbModuleDeclare((NB_Term *)context,ident,cursor);
    }
  else if(strcmp(type,"skill")==0){
    struct NB_SKILL *skill;
    if((skill=nbSkillParse((NB_Term *)context,cursor))!=NULL)
      skill->term=nbTermNew(nb_SkillGloss,ident,skill);
    }
  else if(strcmp(type,"calendar")==0){
    if(nbTimeDeclareCalendar(context,ident,&cursor,msg)==NULL) outPut("%s\n",msg);
    }
  else{
    outMsg(0,'E',"Expecting {identity|module|calendar} at [%s].",cursave);
    return;
    }
  }
         
void nbCmdDefine(nbCELL context,char *verb,char *cursor){
  char ident[256],type[256];
  //char ident[256],type[256],token[256];
  //NB_Term *term,*typeTerm,*context=(NB_Term *)contextCell;  // goof with context type
  NB_Term *term,*typeTerm;  // goof with context type
  struct COND *ruleCond;
  NB_Object *object;
  //struct SCHED *sched;
  struct ACTION *action;
  struct TYPE *rule_type;
  NB_Link *assertions=NULL;
  int standardRule=0;
  int rulePrty=0;
  char rulePrtyStr[20],rulePrtySign='+';
  
  char symid,*cursave;
  char *delim;
  //char msg[1024];

  if(!(clientIdentity->authority&AUTH_DEFINE)){
    outMsg(0,'E',"Identity \"%s\" not authorized to define terms.",clientIdentity->name->value);
    return;
    }
  cursave=cursor;
  symid=nbParseSymbol(ident,&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expecting term identifier at [%s].",cursave);
    return;
    }
  if(*ident=='$' || *ident=='_'){
    outMsg(0,'E',"Terms starting with '$' or '_' may not be user defined.");
    return;
    } 
  if(*ident=='@'){
    if(*(ident+1)=='.') context=(nbCELL)locGloss;
    else{
      outMsg(0,'E',"Terms starting '@' may not be user defined.");
      return;
      }
    } 
  if(*ident=='%') context=(nbCELL)symContext;
  // 2006-12-22 eat - when ready, experiment with using nb_Disabled as definition for "undefined" terms
  //if(NULL!=(term=nbTermFindDown((NB_Term *)context,ident)) && term->def!=nb_Disabled){
  if(NULL!=(term=nbTermFindDown((NB_Term *)context,ident)) && term->def!=nb_Undefined){
    outMsg(0,'E',"Term \"%s\" already defined.",ident);
    return;
    }
  cursave=cursor;
  symid=nbParseSymbol(type,&cursor);
  if(symid!='t'){
    outMsg(0,'E',"Expecting type identifier at \"%s\"",cursave);
    return;
    }

  // check for deprecated types
  if(strcmp(type,"expert")==0){
    outMsg(0,'W',"Deprecated type - use \"node\" instead of \"expert\"");
    strcpy(type,"node");
    }

  if((typeTerm=nbTermFind(nb_TypeGloss,type))==NULL)
    outMsg(0,'E',"Type \"%s\" not defined.",type);
  else {
    strcpy(type,((struct STRING *)typeTerm->def)->value);
    if(type==NULL) outMsg(0,'L',"nbCmdDefine typeTerm->value=NULL.");
    if(strcmp(type,"on")==0 || strcmp(type,"if")==0 || strcmp(type,"when")==0) {
      while(*cursor==' ') cursor++;
      if(*cursor=='('){
        standardRule=1; /* candidate for parsed assertions */
        cursor++;
        }
      else outMsg(0,'I',"Deprecated syntax - Conditions should be enclosed in parentheses.");
      /* Note: In the future, we will require the parenthesis. */
      if((object=nbParseCell((NB_Term *)context,&cursor,0))==NULL){
        outMsg(0,'E',"Rule not understood.");
        return;
        }
      if(standardRule){
        if(*cursor!=')'){
          outMsg(0,'E',"Expecting ')' at [%s]",cursor);
          return;
          }
        cursor++; /* step over paren */
        while(*cursor==' ') cursor++;
        if(*cursor=='['){   /* get priority */
          cursor++;
          cursave=cursor;
          symid=nbParseSymbol(rulePrtyStr,&cursor);
          if(symid=='+' || symid=='-'){
            rulePrtySign=symid;
            symid=nbParseSymbol(rulePrtyStr,&cursor);
            }
          if(symid!='i'){
            outMsg(0,'E',"Expecting integer priority at \"%s\"",cursave);
            return;
            }
          rulePrty=atoi(rulePrtyStr);
          if(rulePrtySign=='-') rulePrty=-rulePrty;
          if(rulePrty<-128 || rulePrty>127){
            outMsg(0,'E',"Expecting priority from -128 to 127, not %d",rulePrty);
            return;
            }
          while(*cursor==' ') cursor++;
          if(*cursor!=']'){
            outMsg(0,'E',"Expecting ']' at \"%s\"",cursor);
            return;
            }
          cursor++;
          while(*cursor==' ') cursor++;
          }
        if(*cursor==':' || *cursor==';' || *cursor==0) assertions=NULL;
        else{
          assertions=nbParseAssertion((NB_Term *)context,(NB_Term *)context,&cursor);
          if(assertions==NULL) return;
          }
        }
      if(*cursor!=':' && *cursor!=';' && *cursor!=0){
        outMsg(0,'E',"Expecting ':', ';' or end of line at [%s].",cursor);
        return;
        }
      action=malloc(sizeof(struct ACTION));
      action->nextAct=NULL;
      action->priority=rulePrty;
      if(strcmp(type,"on")==0) rule_type=condTypeOnRule;
      else if(strcmp(type,"when")==0) rule_type=condTypeWhenRule; 
      else rule_type=condTypeIfRule;
      action->assert=assertions;
      action->command=NULL;
      action->cmdopt=NB_CMDOPT_RULE;     /* do not suppress symbolic substitution */
      action->status='R';   /* ready */
      ruleCond=useCondition(0,rule_type,object,action);
      action->cond=ruleCond; /* plug the condition pointer into the action */
      term=nbTermNew((NB_Term *)context,ident,ruleCond);
      action->term=term;
      action->context=(NB_Term *)context;
      action->type='R';
      /* If a reused term already has subscribers, enable term and adjust levels */
      if(term->cell.sub!=NULL){
        nbCellEnable((NB_Cell *)ruleCond,(NB_Cell *)term);
        nbCellLevel((NB_Cell *)term);
        }
      if(rule_type==condTypeIfRule){
        if(trace) outMsg(0,'T',"nbCmdDefine() linking if rule to context list");
        action->cell.object.next=(NB_Object *)((NB_Node *)((NB_Term *)context)->def)->ifrule;
        ((NB_Node *)((NB_Term *)context)->def)->ifrule=action;
        }
      if(*cursor!=':') return;
      cursor++; /* step over ':' delimiter */
      while(*cursor==' ') cursor++;
      if(*cursor==';') return;
      action->command=grabObject(useString(strtok(cursor,"\n"))); /* action is rest of line */ 
      }
    else if(strcmp(type,"nerve")==0){
      object=nbParseCell((NB_Term *)context,&cursor,0);
      if(*cursor!=';' && *cursor!=0){
        outMsg(0,'E',"Expecting ';' at [%s].",cursor);
        return;
        }
      term=nbTermNew((NB_Term *)context,ident,nb_Unknown);
      ruleCond=useCondition(0,condTypeNerve,object,(struct STRING *)term->word);
      term->def=(NB_Object *)ruleCond;
      }
    else if(strcmp(type,"cell")==0) {
      object=nbParseCell((NB_Term *)context,&cursor,0);
      if(*cursor!=';' && *cursor!=0){
        outMsg(0,'E',"Expecting ';' at [%s].",cursor);
        return;
        }
      if(object==NULL) object=nb_Unknown; /* perhaps nbParseCell should return it, but...*/
      nbTermNew((NB_Term *)context,ident,object);
      }
    else if(strcmp(type,"translator")==0){
      NB_Translator *translator;
      while(*cursor==' ') cursor++;
      delim=cursor;
      while(*delim!=0 && *delim!=';') delim++;
      *delim=0;
      translator=(NB_Translator *)nbTranslatorCompile(context,cursor);
      outFlush();
      if(translator!=NULL) nbTermNew((NB_Term *)context,ident,translator);
      }
    else if(strcmp(type,"node")==0){
      nbNodeParse((NB_Term *)context,ident,cursor);
      }
    else if(strcmp(type,"macro")==0){
      NB_Macro *macro;
      if(NULL!=(macro=nbMacroParse(context,&cursor)))
        nbTermNew((NB_Term *)context,ident,macro);
      }
    else outMsg(0,'E',"Type \"%s\" not recognized.",type);
    }
  }
  
void nbCmdUndefine(nbCELL context,char *verb,char *cursor){
  char symid,ident[256];
  NB_Term *term;

  symid=nbParseSymbol(ident,&cursor);
  if(symid=='-'){
    /* we have dropped support for undefining a class of objects */
    /* why would anyone want to do that */
    if(*cursor!=0){
      outMsg(0,'E',"Syntax error at \"%s\".",cursor);
      return;
      }
    termUndefAll();
    }
  else {
    term=nbTermFindHere((NB_Term *)context,(NB_String *)grabObject(useString(ident)));
    if(term==NULL) outMsg(0,'E',"Term \"%s\" not defined in active context.",ident);
    else termUndef(term);
    }
  }

/*
* Write command to user profile
*/
void nbCmdProfile(nbCELL context,char *verb,char *cursor){
  //unsigned char symid,ident[256];
  int saveBail=nb_opt_bail;
  FILE *file;
#if !defined(WIN32)
  char *home;
#endif
  char filename[256];

  while(*cursor==' ') cursor++;
  if(*cursor==0 || *cursor==';' || *cursor=='\n') return;
  nb_opt_bail=1;  // turn on bail option so we don't save bad commands
  nbCmd(context,cursor,NB_CMDOPT_HUSH);
  nb_opt_bail=saveBail;
#if defined(WIN32)
  sprintf(filename,"%s\\user.nb",outUserDir(NULL));
  if((file=fopen(filename,"a"))==NULL){
    outMsg(0,'E',"Unable to open %s to append identity.",filename);
    return;
    }
#else
  home=((struct passwd *)getpwuid(getuid()))->pw_dir;
  strcpy(filename,home);
  strcat(filename,"/.nb/profile.nb");
  if((file=fopen(filename,"a"))==NULL){
    outMsg(0,'E',"Unable to open %s to append identity.",filename);
    return;
    }
#endif
  fprintf(file,"%s\n",cursor);
  fclose(file);
  }

/*
*  Forecast schedules
*/
void nbCmdForecast(nbCELL context,char *verb,char *cursor){
  char *delim,msg[256];
  char symid,ident[256],*cursave;
  time_t floor,start,end;
  int i;
  struct SCHED *sched;
  NB_Term *term;

  if(!(clientIdentity->authority&AUTH_CONNECT)){
    outMsg(0,'E',"Identity \"%s\" does not have authority to forecast.",clientIdentity->name->value);
    return;
    }
  symid=nbParseSymbol(ident,&cursor);
  if(symid=='t'){
    if((term=nbTermFind((NB_Term *)context,ident))==NULL){
      outMsg(0,'E',"Term \"%s\" not defined.",ident);
      return;
      }
    sched=(struct SCHED *)term->def;
    //fprintf(stderr,"sched=%p type=%p condTypeTime=%p schedTypeTime=%p schedTypePulse=%p schedTypeDelay=%p\n",sched,sched->cell.object.type,schedTypeTime,schedTypePulse,schedTypeDelay);
    if(sched->cell.object.type==condTypeTime){
      sched=(struct SCHED *)((struct COND *)sched)->right;
      }
    else if(sched->cell.object.type!=schedTypeTime && sched->cell.object.type!=schedTypePulse && sched->cell.object.type!=schedTypeDelay){
      outMsg(0,'E',"Term \"%s\" does not reference a schedule cell.",ident);
      return;
      }
    }  
  else if(symid=='~'){
    sched=newSched(context,symid,ident,&delim,msg,0);
    if(sched==NULL){
      outPut("%s\n",msg);
      outMsg(0,'E',"Schedule \"%s\" not understood.",ident);
      return;
      }   
    }
  else{
    outMsg(0,'E',"Parameter must be schedule term or expression.");
    return;
    }
  cursave=cursor;
  symid=nbParseSymbol(ident,&cursor);
  if(symid!=';'){
    outMsg(0,'E',"Expecting end of command at \"%s\".",cursave);
    return;
    }
  schedPrintDump(sched);
  time(&floor);
  for(i=1;i<30;i++){
    start=schedNext(floor,sched); /* start */
    if(start<=0 || start==eternity.end){
      outMsg(0,'I',"Forecast stopped in January of 2038.");
      return;
      }
    end=schedNext(0,sched);       /* end */
    tcPrintSeg(start,end,"");
    floor=end;
    }  
  }
  
void nbCmdStop(nbCELL context,char *verb,char *cursor){
  nb_flag_stop=1;
  nb_opt_prompt=0;
  nbMedullaStop();
  } 

void nbCmdExit(nbCELL context,char *verb,char *cursor){
  NB_Stem *stem=context->object.type->stem;
  NB_Cell *cell=NULL;

  if(!(clientIdentity->authority&AUTH_CONTROL)){
    outMsg(0,'E',"Identity \"%s\" does not have authority to issue stop.",clientIdentity->name->value);
    return;
    }
  while(*cursor==' ') cursor++;
  if(*cursor==0 || *cursor==';' || *cursor=='\n') stem->exitcode=0;
  else if(NULL==(cell=(NB_Cell *)nbParseCell((NB_Term *)context,&cursor,0))){
    outMsg(0,'E',"Syntax error in exit code cell expression - using 1");
    stem->exitcode=1;
    }
  else if(NULL==(cell=nbCellCompute(context,cell))){
    outMsg(0,'L',"Error computing exit code - using 1");
    stem->exitcode=1;
    }
  else if(cell->object.type==strType){
    outMsg(0,'W',"Exit code cell expression resolves to string - using 1");
    stem->exitcode=1;
    }
  else if(cell->object.type==realType){
    stem->exitcode=(int)((NB_Real *)cell)->value;
    if((double)stem->exitcode!=(int)((NB_Real *)cell)->value)
      outMsg(0,'W',"Exit code has been rounded to an integer value");
    }
  else{
    outMsg(0,'E',"Exit code does not resolve to a numeric value - using 1");
    stem->exitcode=1;
    }
  if(cell!=NULL) dropObject(cell);
  nb_flag_stop=1;
  nb_opt_prompt=0;
  nbMedullaStop();
  }

/*
*  Get command from stdin
*/
char *nbGets(int file,char *strbuf,size_t strlen){
  char *strcur=strbuf;
  static char *buf=NULL;
  static char *bufcur;
  static char *bufend;
  static char *bufnew;
  size_t seglen;
  int buflen;

  *strcur=0;  /* start with null string */
  if(buf==NULL){
    buf=malloc(NB_BUFSIZE);
    bufend=buf;
    bufcur=buf;
    }
  while((bufnew=memchr(bufcur,'\n',bufend-bufcur))==NULL){
     seglen=bufend-bufcur;
     if(strlen<=(size_t)(strcur+seglen-strbuf)) seglen=strbuf+strlen-strcur;
     strncpy(strcur,bufcur,seglen);
     strcur+=seglen;
     buflen=read(file,buf,NB_BUFSIZE);
     while(buflen==-1 && errno==EINTR){
       buflen=read(file,buf,NB_BUFSIZE); // read again if we were interrupted
       }
     if(buflen<=0){
       *strbuf=0;
       return(NULL);
       }
     bufcur=buf;
     bufend=buf+buflen;
     }
  seglen=bufnew-bufcur;
  if(strlen<=(size_t)(strcur+seglen-strbuf)) seglen=strbuf+strlen-strcur;
  strncpy(strcur,bufcur,seglen);
  strcur+=seglen;
  *strcur=0; /* insert end of string */
  bufcur=bufnew+1;
  return(strbuf);
  }

int nbIsInteractive(nbCELL context){
  return(nb_opt_prompt);
  }

char *nbGetReply(char *prompt,char *buffer,size_t len){
  if(!nb_opt_prompt) return(NULL);
  outPut(prompt);
#if defined(WIN32)
  return(nbGets(fileno(stdin),buffer,len));
#else
  //return(nbGets(stdin,buffer,len));
  return(nbGets(0,buffer,len));
#endif
  }
/*
*  Get input from standard input (optionally prompt interactive user)
*/
void nbParseStdin(int prompt){
  int promptSave=nb_opt_prompt;
  char *buffer;
  NB_Term *contextSave; /* context for input commands */
 
  outMsg(0,'I',"Reading from standard input.");
  outBar();
  nb_opt_prompt=1;
  while(nb_opt_prompt==1){
    if(prompt) nb_opt_prompt=nbGetCmdInteractive(bufin);
    else if(nbGets(0,bufin,NB_BUFSIZE)==NULL) nb_opt_prompt=0;
    if(nb_opt_prompt){
      outFlush();
      contextSave=addrContext;
      nbClockAlert();
#if defined(WIN32)
      if(nb_medulla->waitCount>0) nbMedullaPulse(0);
#else
      if(nb_medulla->handler!=NULL) nbMedullaPulse(0);  // experiment with pulsing here - eventually we may have a medulla listener for stdin
      else nbMedullaProcessHandler(0);   //  check processes and disable listeners as necessary
#endif
      addrContext=contextSave;
      if(lfile!=NULL) logPrintNl(bufin);
      if(!prompt) outPut("| %s\n",bufin);
      buffer=nbSymSource((nbCELL)symContext,bufin);
      if(buffer!=NULL){
        if(sourceTrace) outPut("] %s\n",buffer);
        if(*bufin!=0) nbCmd((nbCELL)locGloss,buffer,NB_CMDOPT_HUSH);
        }
      }
    }
  nb_opt_prompt=promptSave;
  /* this is temporary - we actually need to avoid query a user
     when solving if the input is not a terminal. */
  if(!prompt) nb_opt_query=0; /* turn off solve if piping */
  }

/*
*  Parse a source file, ignoring lines until terminating directive.
*
*  Terminating directive codes:
*
*    -1 - syntax error
*     0 - eof or exit (exit only during active parsing)
*     1 - endif
*     2 - else
*     3 - elseif
*
*  Til codes:
*
*     1 - endif, eof, or syntax error.
*     3 - elseif, else, endif, eof, syntax error.
*/
int nbParseSourceIgnoreTil(nbCELL context,FILE *file,char *buf,int til){
  int directive;
  char symid,*cursor,ident[256];

  while(fgets(buf,NB_BUFSIZE,file)!=NULL){
    if(*buf=='%'){
      cursor=buf+1;
      symid=nbParseSymbol(ident,&cursor);
      if(strcmp(ident,"if")==0){
        directive=nbParseSourceIgnoreTil(context,file,buf,1);
        if(directive!=1) return(directive);  /* should be 0 or -1 */
        }
      else if(strcmp(ident,"endif")==0) return(1);
      else if(strcmp(ident,"else")==0){
        if(til>1) return(2);
        }
      else if(strcmp(ident,"elseif")==0){
        if(til>1) return(3);
        }
      /* ignore any other directives - but make sure we recognize them */
      else if(strcmp(ident,"assert")==0);
      else if(strcmp(ident,"default")==0);
      else if(strcmp(ident,"include")==0);
      else if(strcmp(ident,"quit")==0);
      else{
        outMsg(0,'E',"Directive not recognized.");
        return(-1);
        }
      }
    if(sourceTrace) outPut("!] %s",buf);
    }
  return(0);
  }

/*
*  Parse %if directive
*
*     %if(<condition>) [; # comment]
*     ...
*     %elseif(<condition>) [; # comment]
*     ...
*     %else [; # comment]
*     ...
*     %endif [; # comment]
*
* Symbolic substitution has already been applied to the %if directive and is applied
* here for the %elseif directive.
*/  
int nbParseSourceIf(nbCELL context,FILE *file,char *buf,char *cursor){
  int directive,doif=0;
  NB_Object *object,*value;
  NB_Term *addrContextSave=addrContext;
  while(1){ /* until we bail with no more alternatives */
    while(*cursor==' ') cursor++;
    if(*cursor!='('){
      outMsg(0,'E',"Expecting '(' at \"%s\".",cursor);
      return(0);
      }
    /* get condition object */
    cursor++;
    addrContext=symContext;
    if(NULL==(object=grabObject(nbParseCell(symContext,&cursor,0)))){
      outMsg(0,'E',"Error in %cif condition - script terminating.");
      addrContext=addrContextSave;
      return(-1);
      }
    if(sourceTrace){
      outPut("Condition: ");
      printObject(object);
      outPut("\n");
      }
    addrContext=addrContextSave;
    if((value=object->type->compute(object))==NB_OBJECT_TRUE) doif=1;
    dropObject(object);
    if(sourceTrace){
      outPut("Value: ");
      printObject(value);
      outPut("\n");
      }
    dropObject(value);
    if(doif){  /* true - process to terminator */
      switch(nbParseSourceTil(context,file)){
        case -1: return(-1);
        case 0:
          outMsg(0,'E',"End of file before %cendif directive.",'%');
          break;
        case 1: /* %endif */
          return(1);
        case 2: /* %else */
          return(nbParseSourceIgnoreTil(context,file,buf,1));
          break;
        case 3: /* %elseif */
          return(nbParseSourceIgnoreTil(context,file,buf,1));
          break;
        default:
          outMsg(0,'L',"Unrecognized return code from nbParseSourceTil()");
          return(-1);
        }
      }
    else{ /* condition is not true, ignore to next alternative */
      directive=nbParseSourceIgnoreTil(context,file,bufin,3); /* look for elseif,else or endif */
      if(directive<0 || directive==1) return(directive);
      if(directive==0){
        outMsg(0,'E',"End of source file reached before %cendif directive.",'%');
        return(-1);
        }
      if(directive==2){
        directive=nbParseSourceTil(context,file);
        if(directive<0 || directive==1) return(directive);
        if(directive==0){
          outMsg(0,'E',"End of source file reached before %cendif directive.",'%');
          return(-1);
          }
        if(directive==2){
          outMsg(0,'E',"Directive %celse not expected.",'%');
          return(-1);
          }
        }
      if(directive!=3){
        outMsg(0,'L',"nbParseSourceIf(): Unexpect code returned by nbParseSourceIgnoreTil().");
        return(-1);
        }
      /* if directive is 3 "elseif" then we loop back to try again */
      if(sourceTrace) outPut("] %s",bufin);
      buf=nbSymSource((NB_Cell *)symContext,bufin);
      if(buf==NULL) return(-1);
      if(symbolicTrace) outPut("cmd1$ %s",buf);
      cursor=buf+7; /* step over %elseif */
      }
    }
  }  

/*
*  Parse source file until error, eof, or terminating directive
*
*  Return Codes:
*
*   -1 - syntax error
*    0 - end-of-file or exit
*    1 - endif
*    2 - else
*    3 - elseif
*/  
int nbParseSourceTil(nbCELL context,FILE *file){
  char *cursor,symid,ident[256],*buf;
  while(fgets(bufin,NB_BUFSIZE,file)!=NULL){
    /* When we're not in check mode ignore check command "~" */
    if((nb_mode_check==0 && *bufin!='~') || (nb_mode_check && outCheck(NB_CHECK_LINE,bufin))){
      if(sourceTrace) outPut("] %s",bufin);
      buf=bufin;
      while(*buf==' ') buf++;
      if(*buf=='%' && *(buf+1)==' '){
        buf=nbSymCmd((NB_Cell *)symContext,bufin,"%{}");
        if(buf==NULL) return(-1);
        if(symbolicTrace) outPut("cmd2$ %s\n",buf);
        }
      /* handle directive statements %if(), %elseif( ), %else, %endif */
      if(*buf=='%'){
        cursor=buf+1;
        symid=nbParseSymbol(ident,&cursor);
        if(symid=='t'){
          if(strcmp(ident,"quit")==0) return(0);
          else if(strcmp(ident,"endif")==0) return(1);
          else if(strcmp(ident,"else")==0) return(2);
          else if(strcmp(ident,"elseif")==0) return(3);
          else if(strcmp(ident,"if")==0) nbParseSourceIf(context,file,buf,cursor);
          else if(strcmp(ident,"assert")==0) iLet(cursor,symContext,0);
          else if(strcmp(ident,"default")==0) iLet(cursor,symContext,1);
          else if(strcmp(ident,"include")==0) nbParseSource(context,cursor);
          else{
            outMsg(0,'E',"Directive \"%s\" not recognized.",ident);
            return(-1);
            }
          }
        else{
          outMsg(0,'E',"Expecting directive identifier following %c.",'%');
          return(-1);
          }
        }    
      else{
        // 2009-01-31 eat - uncommented
        if(!nb_ClockAlerting) nbClockAlert();
        nbCmd((NB_Cell *)context,buf,1);
        }
      }
    }
  return(0);
  }

/*
*  Include command file with symbolic substitution
*/  
void nbParseSource(nbCELL context,char *cursor){
  FILE *file;
  char filename[256],*fcursor;
  //char buf[NB_BUFSIZE];
  long fileloc; /* file location test */
  NB_Term *symContextSave=symContext;  /* save symbolic context */

  symContext=nbTermNew(symContext,"%",nbNodeNew());
  while(*cursor==' ') cursor++;
  if(*cursor=='~'){
    nb_mode_check=4;  /* flag check mode, will flag 2 bit if check script (name ends with "~") */
    cursor++; 
    }
  if(*cursor=='"'){
    cursor++;
    fcursor=strchr(cursor,'"');
    if(fcursor==NULL){
      outMsg(0,'E',"Unbalanced quote in file name");
      symContext=symContextSave;
      return;
      }
    if(sizeof(filename)-1<fcursor-cursor){
      outMsg(0,'E',"File name too long - %d bytes supported",sizeof(filename)-1);
      symContext=symContextSave;
      return;
      } 
    strncpy(filename,cursor,fcursor-cursor);
    *(filename+(fcursor-cursor))=0;
    cursor=fcursor+1;
    }
  else{
// On Windows we allow unquoted file names with spaces, but require
// a comma before assertions.
//
//      > c:\Program Files\nodebrain\nb.exe "c:\Program Files\nodebrain\service\client.nb"
// 
// On Unix/Linux we allow a space as a file name delimiter (for backward compatibility)
// 
#if defined(WIN32)
    for(fcursor=filename;*cursor!=0 && *cursor!=';' && *cursor!=',';fcursor++){
#else
    for(fcursor=filename;*cursor!=' ' && *cursor!=0 && *cursor!=';' && *cursor!=',';fcursor++){
#endif
      *fcursor=*cursor;
      cursor++;
      }
    *fcursor=0;
    }
  while(*cursor==' ') cursor++;
  if(*cursor==',') cursor++;
  else while(*cursor==' ') cursor++;
  if(*cursor!=0 && *cursor!=';') iLet(cursor,symContext,0);
  if(strcmp(filename,"-")==0) nbParseStdin(1);
  else if(strcmp(filename,"=")==0) nbParseStdin(0);
  else{
    if((file=fopen(filename,"r"))==NULL)
      outMsg(0,'E',"Source file \"%s\" not found.",filename);
    else{
      if(nb_mode_check) outCheck(NB_CHECK_START,filename);
      if(nbParseSourceTil(context,file)!=0){
        if(nb_mode_check) outCheck(NB_CHECK_STOP,NULL);
        outMsg(0,'E',"Source file \"%s\" terminated with errors.",filename);
        }
      else{
        if(nb_mode_check) outCheck(NB_CHECK_STOP,NULL);
        fileloc=ftell(file);
        outMsg(0,'I',"Source file \"%s\" included. size=%u",filename,fileloc);
        }
      fclose(file);
      }
    }
  termUndef(symContext);      // undefine symbolic context 
  symContext=symContextSave;  // restore symbolic context 
  }

void nbCmdSource(nbCELL context,char *verb,char *cursor){
  nbParseSource(context,cursor);
  }

/*
*  Generate source from a file
*/
void nbCmdTranslate(nbCELL context,char *verb,char *cursor){
  char xtrname[256],filename[256];
  NB_Term *xtrTerm;
  char *delim;

  strcpy(xtrname,my_strtok_r(cursor," ",&cursor));
  if((xtrTerm=nbTermFind((NB_Term *)context,xtrname))==NULL){
    outMsg(0,'E',"Translator \"%s\" not defined.",xtrname);
    return;
    }
  if(xtrTerm->def==NULL || xtrTerm->def->type!=nb_TranslatorType){
    outMsg(0,'E',"Expecting translator name. Term \"%s\" not a translator.",xtrname);
    return;
    }
  delim=cursor;
  while(*delim==' ') delim++;
  cursor=delim;
  while(*delim!=' ' && *delim!=0 && *delim!=';') delim++;
  if(*delim==' '){
    *delim=0;
    delim++;
    while(*delim==' ') delim++;
    if(*delim!=0 && *delim!=';') iLet(delim,symContext,0);  
    }
  else *delim=0;
  strcpy(filename,cursor);
  outFlush(); 
  nbTranslatorExecuteFile(context,(nbCELL)(xtrTerm->def),filename);
  }
   
//
// Load Shared Library for use by modules
//
void nbCmdLoad(nbCELL context,char *verb,char *cursor){
  char *name,msg[1024];   // we might reconsider the way messages are handled
  while(*cursor==' ') cursor++;
  if(*cursor==0){
    outMsg(0,'E',"Quoted library name required by LOAD command.");
    return;
    }
  if(*cursor!='"'){
    outMsg(0,'E',"Expecting quoted string at: %s",cursor);
    return;
    }
  cursor++;
  name=cursor;
  while(*cursor && *cursor!='"') cursor++;
  if(*cursor==0){
    outMsg(0,'E',"Missing ending quote.");
    return;
    }
  *cursor=0;
  if(*name==0){
    outMsg(0,'E',"Null library name not expected - ignored.");
    return;
    }
  if(nbModuleLoad(name,1,msg)==NULL) outMsg(0,'E',"Unable to load %s - %s",name,msg);
  *cursor='"';  // restore quote because it may be reused
  }

void nbCmdQuit(nbCELL context,char *verb,char *cursor){
  nb_opt_prompt=0;
  }

char *iWord(cursor,word)
  char *cursor,*word; {
  while(*cursor==' ') cursor++;
  while(*cursor!=' ' && *cursor!=0 && *cursor!='\n'){
    *word=*cursor;
    word++;
    cursor++;
    }
  *word=0;
  return(cursor);  
  }

/*
*  Interpret command
*
*    cmdopt: 0x00
*            0x01 - echo statement
*            0x02 - suppress symbolic substitution
*            0x03 - both 1 and 2
*/
//
// 2006-01-01 eat 0.6.4  Experimenting with the elimination of the "+" for node commands.
//            We can use the "(" or ":" delimiter to indicate a node command.
//                       
//            term    - verb
//            term.   - context prefix
//            term:   - node command
//            term(   - node command
//                       
//            Note: need to finish verb tree

#if defined(WIN32)
__declspec(dllexport) 
#endif    
void nbCmd(nbCELL context,char *cursor,int cmdopt){ 
  NB_Stem *stem=context->object.type->stem;
  char symid,verb[256],*cmdbuf=cursor,*cursave;
  //NB_Term *term;
  NB_Term *saveContext;
  //int cmdlen;
  struct NB_VERB *verbEntry;

  //outMsg(0,'T',"nbCmd() called");
  // 2008-06-20 eat - this is a good place to check for schedule events
  //            If this was helpful, it just means we need to call nbClockAlert() somewhere else.
  //            Calling it here can cause unwanted output at a client.
  //if(!nb_ClockAlerting) nbClockAlert();
  /* new line char should be stripped off before we are called - stop fussing in the future */
  if((cursave=strchr(cursor,'\n'))) *cursave=0;
  while(*cursor==' ') cursor++;   /* ignore leading blanks */
  if(*cursor==0) return;
  saveContext=addrContext;
  addrContext=(NB_Term *)context;
  cmdopt|=((NB_Node *)((NB_Term *)context)->def)->cmdopt; // merge context and caller options
  symid=0;   // 0 - scan, 1 - stop scanning, else handle symid and verb
  while(symid==0){  // handle prefix options in a loop until we get down to a verb
    cursave=cursor;
    // 2008-05-26 eat 0.7.0 - improving symbolic substitution performance by making it explicit
    if(*cursor=='$'){
      if(*(cursor+1)==' ') cmdbuf=nbSymCmd(context,cursor,"${}"); // symbolic substitution with reduction
      else{ //handle macro substitution
        cmdbuf=nbMacroSub(context,&cursor);
        if(cmdbuf==NULL) return;
        while(*cursor==' ') cursor++;
        if(*cursor!=0 && *cursor!=';' && *cursor!='#'){
          outMsg(0,'E',"Expecting end of command at \"%s\".",cursor);
          addrContext=saveContext;
          return;
          }
        if(symbolicTrace) outPut("$ %s\n",cmdbuf);
        }
      if(cmdbuf==NULL){
        outMsg(0,'L',"Symbolic substitution failed");
        addrContext=saveContext;
        return;
        }
      cursor=cmdbuf;
      }
    else if(*cursor=='%' && *(cursor+1)==' '){
      cmdbuf=nbSymCmd((nbCELL)symContext,cursor,"%{}");
      cursor=cmdbuf;
      }
    else if(*cursor==0){
      addrContext=saveContext;
      return;
      } 
    else{
      symid=nbParseSymbol(verb,&cursor);  /* check for a term */
      //outMsg(0,'T',"symid=%c verb=%s",symid,verb);
      if(symid!='t'){
        if(*cursave=='`'){   // accept assert abbreviation
          symid='t';
          strcpy(verb,"assert"); // note: we let this go down to the lookup for authority checking
          }
        else{
          symid=*cursave;
          cursor=cursave+1;
          *verb=0;
          }
        }
      if(symid=='t'){
        if(*cursor=='.'){  // context prefix 
          cursor++;
          if(*cursor!=' '){
            outMsg(0,'E',"Expecting ' ' at [%s]\n",cursor);
            addrContext=saveContext;
            return;
            }
          cursor++;
          if(*verb==0); // special case of ". " as context prefix
          else if((context=(nbCELL)nbTermFind((NB_Term *)context,verb))==NULL ||
              ((NB_Term *)context)->def->type!=nb_NodeType){
            outMsg(0,'E',"Term \"%s\" not defined as node.",verb);
            addrContext=saveContext;
            return;
            }
          symid=0;
          }
        else if(*cursor==':' || *cursor=='(') symid=1;  // node command
        }
      }
    while(*cursor==' ') cursor++;
    addrContext=(NB_Term *)context;
    }
  //outMsg(0,'T',"nbCmd: symid=%c,verb=%s,cursor=%s",symid,verb,cursor);
  // 2008-05-29 eat 0.7.0 displaying before substitution now - let trace option show resolved command
  if(cmdopt&NB_CMDOPT_ECHO && !(cmdopt&NB_CMDOPT_HUSH)){
    if(cmdopt&NB_CMDOPT_RULE) outPut(":");
    else outPut(">");
    if(context!=(nbCELL)locGloss){
      outPut(" ");
      nbTermPrintLongName((NB_Term *)context);
      outPut(".");
      }
    //outPut(" %s\n",cursor);
    outPut(" %s\n",cursave);
    cmdopt|=NB_CMDOPT_HUSH;  /* suppress any futher attempts to echo */
    }
  switch(symid){
    case 1:
      nbNodeCmd((nbCELL)context,verb,cursor);   // execute node command in specified context 
      break;
    case 't':
      // need to move these to the verb table - but think about the symContext
      // should these really be available here - what is the scope of symContext - investigate
      if(strcmp(verb,"%assert")==0) iLet(cursor,symContext,0);    /* AUTH_ASSERT  */
      else if(strcmp(verb,"%default")==0) iLet(cursor,symContext,1);   /* AUTH_ASSERT  */
      else if(strcmp(verb,"%include")==0) nbParseSource(context,cursor);       /* AUTH_DEFINE  */
      else if((verbEntry=nbVerbFind(stem->verbTree,verb))!=NULL){
        if(!(clientIdentity->authority&verbEntry->authmask)){
          outMsg(0,'E',"Identity \"%s\" does not have authority to issue %s command.",clientIdentity->name->value,verb);
          return;
          }
        (*verbEntry->parse)(context,verb,cursor);
        }
      else if(strcmp(verb,"address")==0){
        outMsg(0,'E',"The ADDRESS command is obsolete. Use single quote (') to establish command prefix.");
        }
      else outMsg(0,'E',"Verb \"%s\" not recognized.",verb);
      break;
    case '#': 
      break;
    case '^':     // 2008-05-30 eat 0.7.0 - stopped support ':' for this function
      if(cmdopt&NB_CMDOPT_ECHO && !(cmdopt&NB_CMDOPT_HUSH)) outPut("%s\n",cursave);
      printf("%s\n",cursor);
      break; 
    case '-':
    case '=':
      if(!(cmdopt&NB_CMDOPT_HUSH)) outPut("> %s\n",cmdbuf);    /* always echo system commands */
      nbSpawnChild(context,0,cursave);                       /* AUTH_SYSTEM  */
      break;
    case '{':
      nbRuleExec(context,cursor);                           /* AUTH_DEFINE */
      break;
    case '(':{
      int savetrace=trace;
      if(strstr(cursor,"!trace")!=NULL) trace=0;
      else if(strstr(cursor,"trace")!=NULL) trace=1;
      while(*cursor!=0 && *cursor!=')') cursor++;
      if(*cursor!=0){
        cursor++;
        nbCmd((NB_Cell *)context,cursor,(char)(cmdopt&NB_CMDOPT_HUSH));
        }
      trace=savetrace;
      }
      break;
    case '?':  // comput a cell expression 
      if(*cursor==0) nbCmdShow(context,"show",cursor);
      else{
        NB_Object *object,*cell;
        if(NULL==(cell=nbParseCell((NB_Term *)context,&cursor,0))) return;
        grabObject(cell);
        object=nbCellCompute_((NB_Cell *)cell);
        printObject(object);
        outPut("\n");
        dropObject(object);
        dropObject(cell);
        }
      addrContext=saveContext;
      return;                    // don't need to react to show commands
    default: 
      outMsg(0,'E',"First symbol in command \"%c\" not recognized.",symid);
    }
  nbRuleReact(); // react to any changing conditions
  if(change!=NULL) condChangeReset();
  addrContext=saveContext;
  }

void nbCmdSid(nbCELL context,char *cursor,char cmdopt,struct IDENTITY *identity){
  struct IDENTITY *clientIdentitySave=clientIdentity;

  if(*cursor==0){
    outPut(">\nNB000L nbCmdSid - NULL Statement ignored.\n");
    return;
    }
  if(sourceTrace) outPut("] %s\n",cursor);
  clientIdentity=identity;
  nbCmd((NB_Cell *)context,cursor,cmdopt);
  clientIdentity=clientIdentitySave;     
  }

/*
*  A command parsing routine is called as follows
*
*    parse(NB_Cell *context,char *verb,char *cursor);
*
*
*/
void nbCmdInit(NB_Stem *stem){
  stem->verbTree=NULL;
  stem->verbCount=0;
  // These should be built into the print function instead of being in the tree
  nbVerbDefine(stem," <context>.",AUTH_CONNECT,0,NULL,"<command>");
  nbVerbDefine(stem," <context>:",AUTH_CONNECT,0,NULL,"<node_command>");
  nbVerbDefine(stem," <context>(",AUTH_CONNECT,0,NULL,"<list>):<node_command>");

  nbVerbDefine(stem,"#",AUTH_CONNECT,0,NULL,"<comment>");
  nbVerbDefine(stem,":",AUTH_CONNECT,0,NULL,"<command> | <stdout_message>");
  nbVerbDefine(stem,"-",AUTH_SYSTEM,0,NULL,"[|][:]<shell_command>");
  nbVerbDefine(stem,"=",AUTH_SYSTEM,0,NULL,"[|][:]<shell_command>");
  nbVerbDefine(stem,"{",AUTH_CONNECT,0,NULL,"<rule>}");
  nbVerbDefine(stem,"?",AUTH_CONNECT,0,NULL,"<cell>");
  nbVerbDefine(stem,"(",AUTH_CONNECT,0,NULL,"<option>[,...]) <command>");
  nbVerbDefine(stem,"`",AUTH_ASSERT,0,NULL,"<assertion>");
  nbVerbDefine(stem,"alert",AUTH_ASSERT,0,&nbCmdAssert,"<assertion>");    // nbCmdAssert checks verb
  nbVerbDefine(stem,"archive",AUTH_CONTROL,0,&nbCmdArchive,"");
  nbVerbDefine(stem,"assert",AUTH_ASSERT,0,&nbCmdAssert,"( [!|?]<term>[<list] | <term>[<list>][=[=]<cell>] | [!|?]<list> | <list>[=<cell>] ) [,...]");
  nbVerbDefine(stem,"declare",AUTH_DECLARE,0,&nbCmdDeclare,"<term> <type> ...");
  nbVerbDefine(stem,"define",AUTH_DEFINE,0,&nbCmdDefine,"<term> <type> ...");
  nbVerbDefine(stem,"deny",AUTH_CONTROL,0,&nbCmdDeny,"*** future ***");
  nbVerbDefine(stem,"disable",AUTH_DEFINE,0,&nbCmdEnable,"<term>");  // nbCmdEnable checks verb
  nbVerbDefine(stem,"enable",AUTH_DEFINE,0,&nbCmdEnable,"<term>");
  nbVerbDefine(stem,"exit",AUTH_CONTROL,0,&nbCmdExit,"<cell>");
  nbVerbDefine(stem,"forecast",AUTH_CONNECT,0,&nbCmdForecast,"~(<timeCondition>)");
  nbVerbDefine(stem,"grant",AUTH_CONTROL,0,&nbCmdGrant,"*** future ***");
  nbVerbDefine(stem,"load",AUTH_CONTROL,0,&nbCmdLoad,"<library>");
  nbVerbDefine(stem,"profile",AUTH_CONTROL,0,&nbCmdProfile,"<command>");
  nbVerbDefine(stem,"query",AUTH_CONTROL,0,&nbCmdQuery,"<context>");
  nbVerbDefine(stem,"quit",AUTH_CONTROL,NB_VERB_LOCAL,&nbCmdQuit,"");
  nbVerbDefine(stem,"rank",AUTH_CONTROL,0,&nbCmdRank,"<identity> (owner|peer|guest)");
  nbVerbDefine(stem,"set",AUTH_CONTROL,0,&nbCmdSet,"<option>[,...]");
  nbVerbDefine(stem,"show",AUTH_CONNECT,0,&nbCmdShow,"<term> | (<cell>) | ?");
  nbVerbDefine(stem,"solve",AUTH_CONTROL,0,&nbCmdQuery,"<context>");
  nbVerbDefine(stem,"source",AUTH_ASSERT,0,&nbCmdSource,"<file>,<term>=<cell>[,...]");
  nbVerbDefine(stem,"stop",AUTH_CONTROL,0,&nbCmdStop,"");
  //nbVerbDefine(stem,"translate",AUTH_ASSERT,0,&nbCmdTranslate,"");
  nbVerbDefine(stem,"undefine",AUTH_DEFINE,0,&nbCmdUndefine,"<term>");
  nbVerbDefine(stem,"use",AUTH_CONTROL,0,&nbCmdUse,"");
#if defined(WIN32)
  nbVerbDefine(stem,"windows",AUTH_CONTROL,0,&nbwCommand,"service(Start|Stop) <service>");
#endif
  stem->verbTree=nbVerbBalance(stem->verbTree,stem->verbCount);
  }