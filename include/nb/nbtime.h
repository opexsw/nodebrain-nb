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
* File:     nbtime.h 
* 
* Title:    Time Condition Header
*
* Function:
*
*   This header defines routines that implement time conditions.
*
* See nbtime.c for more information.
*=============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2003-03-15 eat 0.5.1  Created to conform to new make file
* 2010-02-28 eat 0.7.9  Cleaned up -Wall warning messages. (gcc 4.5.0)
*=============================================================================
*/
#ifndef _NBTIME_H_
#define _NBTIME_H_       /* never again */

#include <nb/nbstem.h>

extern long maxtime;
/*
*  Time Condition Definition
*
*  o We have two parameters for operations.  The use of these parameters
*    varies by operation.  We have named them left and right to relate
*    to expression syntax.
*  o By "Simple" we mean any of the operations that call tcSimple with
*    an alignment, step and duration.  These functions do not require
*    left and right parameters.
*  o Indexed selection requires two tcDef structures which we call "Select"
*    and "Index".  The "Select" operation is the same as we use for
*    non-indexed selection.
*
*         tcDef1:  Select{tcDef2, tcDef(right)}
*         tcDef2:  Index{tcDef(left), bfi index}
*
*/
struct tcDef{             /* time condition definition */
  bfi (*operation)();     /* operation to perform on left and right */
  void *left;             /* Simple - NULL */
                          /* Match  - duration stepper */
                          /* Prefix - NULL */
                          /* Infix  - tcDef for left side */
                          /* Index  - tcDef for left side */
                          /* Plan   - plan */
  void *right;            /* Simple - NULL */
                          /* Match  - tcParm */
                          /* Prefix - tcDef for right side */
                          /* Infix  - tcDef for right side */
                          /* Index  - bfiindex */
                          /* Plan   - thread */
  };

typedef struct tcDef *tc;  /* time condition pointer */

struct tcQueue{
  tc   tcdef;       /* Time Condition Definition */
  bfi  set;         /* Time Interval Set */
  };

typedef struct tcQueue *tcq;

struct NB_CALENDAR{        /* Calendar object */
  struct NB_OBJECT object;
  struct STRING *text;     /* source expression */
  tc tcdef;
  };

typedef struct NB_CALENDAR NB_Calendar;

extern NB_Term *nb_TimeCalendarContext;  /* calendar context */

/*
*  Time function parameter list structure
*
*  o  There are three forms of parameters as illustrated by the following
*     expression.
*
*        jan(1,1_5,1..5)
*
*        1    - day 1 to day 2 (single segment)
*        1_5  - day 1 to day 6 (single segment) 
*        1..5 - day 1,2,3,4,5  (multiple segments)
*
*  o  A negative value for stop[0] indicates when a multiple segment
*     range is specified.
*
*  o  This structure contains a stepper function and start and stop patterns.  The
*     first element [0] of pattern arrays specifies how many of the array elements are
*     used [1]-[6].  The stepper function is associated with the first unspecified
*     element (sec,min,hour,day,month,(year,decade,century,millenium)). The last
*     array element [7] is for special functions that don't map nicely to the broken
*     down time structure tm for alignment.  For these functions the parent parameters
*     are specified in [1]-[6] and function specific parameter is stored in [7].
*/    
struct tcParm{
  struct tcParm *next;
  long (*step)();     /* stepper */
  int start[8];       /* n,sec,min,hour,day,month,(year,decade,century,millenium),special */
  int stop[8];        /* n,sec,min,hour,day,month,(year,decade,century,millenium),special */
                      /* special value is for su(1.4) or week(7.50) */
  };

tc tcParse(NB_Cell *context,char **source,char *msg,size_t msglen);
tcq tcQueueNew(tc tcdef,long begin,long end);
long tcQueueTrue(tcq queue,long begin,long end);
long tcQueueFalse(tcq queue);

void tcPrintSeg(long start,long stop,char *label);
long tcTime(void);

void nbTimeInit(NB_Stem *stem);
NB_Term *nbTimeDeclareCalendar(NB_Cell *context,char *ident,char **source,char *msg,size_t msglen);
NB_Term *nbTimeLocateCalendar(char *ident);

bfi tcCast(long begin,long end,tc tcdef);
long tcStepYear(long timer,int n);
long tcStepQuarter(long timer,int n);
long tcStepMonth(long timer,int n);
long tcStepWeek(long timer,int n);
long tcStepDay(long timer,int n);
long tcStepHour(long timer,int n);
long tcStepMinute(long timer,int n);
long tcStepSecond(long timer,int n);
char *tcTimeString(char *str,long timer);

#endif
